#include "config_store.h"
#include "EEPROM.h"

#if BF006_CONFIG_STORE_ENABLED
#define HEADER_SIZE 8
#define CRC_OFFSET (HEADER_SIZE + CONFIG_STORE_SIZE)
#define COMMIT_OFFSET (CRC_OFFSET + 2)
#define COMMIT_VALUE 0xA5
#define STORE_ERASE 1
#define STORE_WRITE 2
#define STORE_VERIFY 3
#define STORE_COMMIT 4
#define STORE_CONFIRM 5

typedef char Config_Store_Layout_Check[sizeof(config_t) == CONFIG_STORE_SIZE ? 1 : -1];

static u16 pages[2];
static u32 generation;
static u16 crc;
static u8 active_page;
static u8 pending_page;
static u8 state;
static u8 position;
static u8 uncertain;
static u8 opened;
static u8 edit_count;
static u8 *current_payload;
static const Config_Byte_Edit *pending_edits;

/******************************************************************************
 * 函数名称       : Store_Read
 * 功能描述       : 从指定配置槽读取一个字节。
 * 输入参数       : page 槽序号，offset 槽内偏移
 * 返回值         : 字节内容
 * 日期            : 2026.11.14
 ******************************************************************************/
static u8 Store_Read(u8 page, u8 offset) //20261114-4 add
{
    u8 value;

    EEPROM_read_n(pages[page] + offset, &value, 1);
    return value;
}

/******************************************************************************
 * 函数名称       : Store_CRC_Byte
 * 功能描述       : 将一个字节累加到配置槽校验值。
 * 输入参数       : checksum 前次校验值，value 当前字节
 * 返回值         : 更新后的校验值
 * 日期            : 2026.11.14
 ******************************************************************************/
static u16 Store_CRC_Byte(u16 checksum, u8 value) //20261114-4 add
{
    u8 i;

    checksum ^= value;
    for (i = 0; i < 8; i++) {
        if (checksum & 1) {
            checksum = (checksum >> 1) ^ 0xA001;
        } else {
            checksum >>= 1;
        }
    }
    return checksum;
}

/******************************************************************************
 * 函数名称       : Store_Check_Page
 * 功能描述       : 验证提交标记、格式、代次和完整槽校验。
 * 输入参数       : page 槽序号，page_generation 用于返回代次
 * 返回值         : 空、有效或证据不确定
 * 日期            : 2026.11.14
 ******************************************************************************/
static u8 Store_Check_Page(u8 page, u32 *page_generation) //20261114-4 add
{
    u8 offset;
    u8 marker;
    u16 checksum;
    u16 stored_crc;
    u32 sequence;

    marker = Store_Read(page, COMMIT_OFFSET);
    if (marker != COMMIT_VALUE) {
        return CONFIG_STORE_EMPTY;
    }
    if (Store_Read(page, 0) != 0x42 || Store_Read(page, 1) != 0x46 ||
        Store_Read(page, 2) != 1 || Store_Read(page, 3) != CONFIG_STORE_SIZE) {
        return CONFIG_STORE_UNCERTAIN;
    }
    checksum = 0xFFFF;
    sequence = 0;
    for (offset = 0; offset < CRC_OFFSET; offset++) {
        marker = Store_Read(page, offset);
        if (offset >= 4 && offset < HEADER_SIZE) {
            sequence = (sequence << 8) | marker;
        }
        checksum = Store_CRC_Byte(checksum, marker);
    }
    stored_crc = Store_Read(page, CRC_OFFSET);
    stored_crc |= (u16)Store_Read(page, CRC_OFFSET + 1) << 8;
    if (!sequence || stored_crc != checksum) {
        return CONFIG_STORE_UNCERTAIN;
    }
    *page_generation = sequence;
    return CONFIG_STORE_READY;
}

/******************************************************************************
 * 函数名称       : Config_Store_Open
 * 功能描述       : 扫描两个配置槽并装载最新完整提交。
 * 输入参数       : first 和 second 为独立槽起点，payload 为运行配置
 * 返回值         : 空、有效或证据不确定
 * 日期            : 2026.11.14
 ******************************************************************************/
u8 Config_Store_Open(u16 first, u16 second, u8 *payload) //20261114-4 add
{
    u8 first_state;
    u8 second_state;
    u8 offset;
    u8 value;
    u32 first_generation;
    u32 second_generation;

    opened = 0;
    state = 0;
    uncertain = 1;
    if ((first < second ? second - first : first - second) <= COMMIT_OFFSET ||
        first > 0xFFFFU - COMMIT_OFFSET || second > 0xFFFFU - COMMIT_OFFSET) {
        return CONFIG_STORE_UNCERTAIN;
    }
    pages[0] = first;
    pages[1] = second;
    current_payload = payload;
    first_generation = 0;
    second_generation = 0;
    first_state = Store_Check_Page(0, &first_generation);
    second_state = Store_Check_Page(1, &second_generation);
    if (first_state == CONFIG_STORE_UNCERTAIN || second_state == CONFIG_STORE_UNCERTAIN) {
        return CONFIG_STORE_UNCERTAIN;
    }
    if (first_generation && first_generation == second_generation) {
        for (offset = HEADER_SIZE; offset < CRC_OFFSET; offset++) {
            value = Store_Read(0, offset);
            if (value != Store_Read(1, offset)) {
                return CONFIG_STORE_UNCERTAIN;
            }
        }
    }
    opened = 1;
    uncertain = 0;
    active_page = second_generation > first_generation;
    generation = active_page ? second_generation : first_generation;
    if (!generation) {
        return CONFIG_STORE_EMPTY;
    }
    EEPROM_read_n(pages[active_page] + HEADER_SIZE, payload, CONFIG_STORE_SIZE);
    return CONFIG_STORE_READY;
}

/******************************************************************************
 * 函数名称       : Config_Store_Begin
 * 功能描述       : 预约配置提交，保留当前配置及按偏移排序的候选字节。
 * 输入参数       : payload 为已打开的配置，edits 和 count 为候选字节及数量
 * 返回值         : 进行中、同值、失败或证据不确定
 * 日期            : 2026.11.14
 ******************************************************************************/
u8 Config_Store_Begin(const u8 *payload, const Config_Byte_Edit *edits, u8 count) //20261114-4 add
{
    u8 i;
    u8 changed;

    if (uncertain) {
        return CONFIG_STORE_UNCERTAIN;
    }
    if (!opened || payload != current_payload || generation == 0xFFFFFFFFUL) {
        return CONFIG_STORE_FAILED;
    }
    if (state) {
        return CONFIG_STORE_LOCKED;
    }
    changed = 0;
    for (i = 0; i < count; i++) {
        if (edits[i].offset >= CONFIG_STORE_SIZE || (i && edits[i - 1].offset >= edits[i].offset)) {
            return CONFIG_STORE_FAILED;
        }
        if (payload[edits[i].offset] != edits[i].value) {
            changed = 1;
        }
    }
    if (count && !changed && generation) {
        return CONFIG_STORE_READY;
    }
    pending_edits = edits;
    edit_count = count;
    pending_page = generation ? active_page ^ 1 : 0;
    state = STORE_ERASE;
    return CONFIG_STORE_BUSY;
}

/******************************************************************************
 * 函数名称       : Store_Candidate_Byte
 * 功能描述       : 按当前配置及编辑字节生成目标槽的一个字节。
 * 输入参数       : offset 槽内偏移
 * 返回值         : 目标字节
 * 日期            : 2026.11.14
 ******************************************************************************/
static u8 Store_Candidate_Byte(u8 offset) //20261114-4 add
{
    static const u8 code header[] = {0x42, 0x46, 1, CONFIG_STORE_SIZE};
    u8 i;

    if (offset < sizeof(header)) {
        return header[offset];
    }
    if (offset < HEADER_SIZE) {
        return (u8)((generation + 1UL) >> ((7 - offset) * 8));
    }
    if (offset >= CRC_OFFSET) {
        return offset == CRC_OFFSET ? (u8)crc : (u8)(crc >> 8);
    }
    offset -= HEADER_SIZE;
    for (i = 0; i < edit_count && pending_edits[i].offset <= offset; i++) {
        if (pending_edits[i].offset == offset) {
            return pending_edits[i].value;
        }
    }
    return current_payload[offset];
}

/******************************************************************************
 * 函数名称       : Config_Store_Step
 * 功能描述       : 推进一个擦除、编程或读回动作，确认后才发布修改。
 * 输入参数       : None
 * 返回值         : 提交状态
 * 日期            : 2026.11.14
 ******************************************************************************/
u8 Config_Store_Step(void) //20261114-4 add
{
    u8 value;
    u8 i;

    switch (state) {
    case STORE_ERASE:
        EEPROM_SectorErase(pages[pending_page]);
        state = STORE_WRITE;
        position = 0;
        crc = 0xFFFF;
        break;
    case STORE_WRITE:
        value = Store_Candidate_Byte(position);
        EEPROM_write_n(pages[pending_page] + position, &value, 1);
        if (position < CRC_OFFSET) {
            crc = Store_CRC_Byte(crc, value);
        }
        if (++position == COMMIT_OFFSET) {
            state = STORE_VERIFY;
            position = 0;
        }
        break;
    case STORE_VERIFY:
        value = Store_Read(pending_page, position);
        if (value != (position == COMMIT_OFFSET ? 0xFF : Store_Candidate_Byte(position))) {
            state = 0;
            return CONFIG_STORE_FAILED;
        }
        if (position++ == COMMIT_OFFSET) {
            state = STORE_COMMIT;
        }
        break;
    case STORE_COMMIT:
        value = COMMIT_VALUE;
        EEPROM_write_n(pages[pending_page] + COMMIT_OFFSET, &value, 1);
        state = STORE_CONFIRM;
        break;
    case STORE_CONFIRM:
        state = 0;
        if (Store_Read(pending_page, COMMIT_OFFSET) != COMMIT_VALUE) {
            uncertain = 1;
            return CONFIG_STORE_UNCERTAIN;
        }
        generation++;
        active_page = pending_page;
        for (i = 0; i < edit_count; i++) {
            current_payload[pending_edits[i].offset] = pending_edits[i].value;
        }
        return CONFIG_STORE_SAVED;
    default:
        return uncertain ? CONFIG_STORE_UNCERTAIN : CONFIG_STORE_READY;
    }
    return CONFIG_STORE_BUSY;
}

/******************************************************************************
 * 函数名称       : Config_Store_Cancel
 * 功能描述       : 取消未提交的目标，标记已写时保留待确认状态。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void Config_Store_Cancel(void) //20261114-4 add
{
    if (state != STORE_CONFIRM) {
        state = 0;
    }
}
#endif
