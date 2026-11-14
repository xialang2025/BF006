#include "boot_identity.h"
#include "config_store.h"
#include "EEPROM.h"
#include "Modbus_New.h"

#if BF006_CONFIG_STORE_ENABLED
#define IDENTITY_RECORD_SIZE 8
#define IDENTITY_COMMIT 0xA5

#if BF006_BOARD_CONTEXT
#if BF006_BOOT_SLOT_A < 512 || BF006_BOOT_SLOT_B < 512 || BF006_BOOT_SLOT_A % 512 || BF006_BOOT_SLOT_B % 512 || BF006_BOOT_SLOT_A > 65024 || BF006_BOOT_SLOT_B > 65024 || BF006_BOOT_SLOT_A == BF006_BOOT_SLOT_B || BF006_BOOT_SLOT_A == BF006_CONFIG_SLOT_A || BF006_BOOT_SLOT_A == BF006_CONFIG_SLOT_B || BF006_BOOT_SLOT_B == BF006_CONFIG_SLOT_A || BF006_BOOT_SLOT_B == BF006_CONFIG_SLOT_B
#error BF006_BOOT_SLOT_LAYOUT
#endif
static u32 boot_generation = 0;
static volatile u32 boot_uptime = 0;
static volatile u8 boot_time_lost = 0;

/******************************************************************************
 * 函数名称       : Boot_Context_Init
 * 功能描述       : 启动时从维护登记日志取得读回确认的新代次。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void Boot_Context_Init(void) //20261114-14 add
{
    boot_generation = Boot_Identity_Advance(BF006_BOOT_SLOT_A, BF006_BOOT_SLOT_B, 512);
}

/******************************************************************************
 * 函数名称       : Boot_Context_Tick
 * 功能描述       : 以已经过实体核验的毫秒节拍推进时间。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void Boot_Context_Tick(void) //20261114-14 add
{
    if (++boot_uptime == 0) {
        boot_time_lost = 1;
    }
}

/******************************************************************************
 * 函数名称       : Boot_Context_Wrapped
 * 功能描述       : 查询时间是否已经回绕并暂停执行上下文。
 * 输入参数       : None
 * 返回值         : 回绕标记
 * 日期            : 2026.11.14
 ******************************************************************************/
u8 Boot_Context_Wrapped(void) //20261114-21 add
{
    return boot_time_lost;
}

/******************************************************************************
 * 函数名称       : Boot_Context_Renew
 * 功能描述       : 在旧事务收束后保存回绕所需的新代次。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void Boot_Context_Renew(void) //20261114-21 add
{
    if (!boot_time_lost || !boot_generation) {
        return;
    }
    boot_generation = Boot_Identity_Advance(BF006_BOOT_SLOT_A, BF006_BOOT_SLOT_B, 512);
    if (boot_generation) {
        config_revision = 1;
        boot_time_lost = 0;
    }
}

/******************************************************************************
 * 函数名称       : Boot_Context_Read
 * 功能描述       : 返回经过身份、配置和时钟检查的执行上下文。
 * 输入参数       : boot 代次，revision 配置版本，uptime 当前毫秒
 * 返回值         : 可用时返回一
 * 日期            : 2026.11.14
 ******************************************************************************/
u8 Boot_Context_Read(u32 *boot, u32 *revision, u32 *uptime) //20261114-14 add
{
    bit interrupt_enabled;
    u8 time_lost;

    if (!boot_generation || !config_trusted) {
        return 0;
    }
    interrupt_enabled = EA;
    EA = 0;
    *uptime = boot_uptime;
    time_lost = boot_time_lost;
    EA = interrupt_enabled;
    *boot = boot_generation;
    *revision = config_revision;
    return !time_lost;
}
#endif

typedef struct {
    u32 generation;
    u16 available;
    u8 blank;
} Identity_Page;

/******************************************************************************
 * 函数名称       : Identity_Scan
 * 功能描述       : 找出日志中最大可信代次和第一个空白条目。
 * 输入参数       : start 区起点，size 区长度，page 扫描结果
 * 返回值         : 扫描可信时返回一
 * 日期            : 2026.11.14
 ******************************************************************************/
static u8 Identity_Scan(u16 start, u16 size, Identity_Page *page) //20261114-5 add
{
    u8 record[IDENTITY_RECORD_SIZE];
    u16 offset;
    u8 i;
    u8 used;
    u16 checksum;
    u32 generation;

    page->available = 0xFFFF;
    page->generation = 0;
    page->blank = 1;
    for (offset = 0; offset < size; offset += IDENTITY_RECORD_SIZE) {
        EEPROM_read_n(start + offset, record, sizeof(record));
        used = 0;
        for (i = 0; i < sizeof(record); i++) {
            if (record[i] != 0xFF) {
                used = 1;
            }
        }
        if (!used) {
            if (page->available == 0xFFFF) {
                page->available = offset;
            }
            continue;
        }
        page->blank = 0;
        page->available = 0xFFFF; // 只在已用条目之后追加
        if (record[7] != IDENTITY_COMMIT) {
            continue;
        }
        checksum = BF006_Modbus_CRC(record, 5);
        generation = 0;
        for (i = 0; i < 4; i++) {
            generation = (generation << 8) | record[i];
        }
        if (record[4] != 1 || record[5] != (u8)checksum || record[6] != (u8)(checksum >> 8) ||
            generation <= page->generation) {
            return 0;
        }
        page->generation = generation;
    }
    return 1;
}

/******************************************************************************
 * 函数名称       : Identity_Append
 * 功能描述       : 写入代次和校验，逐字节核对后最后写提交标记。
 * 输入参数       : address 空白条目地址，generation 待写代次
 * 返回值         : 验证成功时返回一
 * 日期            : 2026.11.14
 ******************************************************************************/
static u8 Identity_Append(u16 address, u32 generation) //20261114-5 add
{
    u8 record[IDENTITY_RECORD_SIZE];
    u8 i;
    u8 value;
    u16 checksum;

    for (i = 0; i < sizeof(record); i++) {
        EEPROM_read_n(address + i, &value, 1);
        if (value != 0xFF) {
            return 0;
        }
    }
    record[0] = (u8)(generation >> 24);
    record[1] = (u8)(generation >> 16);
    record[2] = (u8)(generation >> 8);
    record[3] = (u8)generation;
    record[4] = 1;
    checksum = BF006_Modbus_CRC(record, 5);
    record[5] = (u8)checksum;
    record[6] = (u8)(checksum >> 8);
    record[7] = IDENTITY_COMMIT;

    EEPROM_write_n(address, record, 7);
    for (i = 0; i < 7; i++) {
        EEPROM_read_n(address + i, &value, 1);
        if (value != record[i]) {
            return 0;
        }
    }
    EEPROM_write_n(address + 7, &record[7], 1);
    for (i = 0; i < sizeof(record); i++) {
        EEPROM_read_n(address + i, &value, 1);
        if (value != record[i]) {
            return 0;
        }
    }
    return 1;
}

/******************************************************************************
 * 函数名称       : Identity_Layout_Valid
 * 功能描述       : 检查调用方提供的两个独立日志地址是否可寻址且互不重叠。
 * 输入参数       : first 和 second 为起点，size 为每区长度
 * 返回值         : 布局可用时返回一
 * 日期            : 2026.11.14
 ******************************************************************************/
static u8 Identity_Layout_Valid(u16 first, u16 second, u16 size) //20261114-5 add
{
    return size >= IDENTITY_RECORD_SIZE && size % IDENTITY_RECORD_SIZE == 0 &&
           (u32)first + size <= 0x10000UL && (u32)second + size <= 0x10000UL &&
           (first < second ? second - first : first - second) >= size;
}

/******************************************************************************
 * 函数名称       : Boot_Identity_Advance
 * 功能描述       : 在已登记的日志上提交下一代次，保留上一可信记录。
 * 输入参数       : first 和 second 为独立日志区起点，size 为每区长度
 * 返回值         : 经读回确认的新代次，失败返回零
 * 日期            : 2026.11.14
 ******************************************************************************/
u32 Boot_Identity_Advance(u16 first, u16 second, u16 size) //20261114-5 add
{
    Identity_Page page_a;
    Identity_Page page_b;
    u16 active;
    u16 other;
    u16 offset;
    u32 generation;

    if (!Identity_Layout_Valid(first, second, size) ||
        !Identity_Scan(first, size, &page_a) || !Identity_Scan(second, size, &page_b) ||
        page_a.generation == page_b.generation) {
        return 0;
    }
    if (page_a.generation > page_b.generation) {
        generation = page_a.generation;
        offset = page_a.available;
        active = first;
        other = second;
    } else {
        generation = page_b.generation;
        offset = page_b.available;
        active = second;
        other = first;
    }
    if (generation == 0xFFFFFFFFUL) {
        return 0;
    }
    if (offset == 0xFFFF) {
        EEPROM_SectorErase(other); // 含最大可信代次的旧区保持可恢复
        active = other;
        offset = 0;
    }
    generation++;
    return Identity_Append(active + offset, generation) ? generation : 0;
}

/******************************************************************************
 * 函数名称       : Boot_Identity_Provision
 * 功能描述       : 将现场维护明确提供的初始代次写入空白日志。
 * 输入参数       : first 和 second 为独立日志区起点，size 为每区长度，seed 为登记代次
 * 返回值         : 验证成功时返回一
 * 日期            : 2026.11.14
 ******************************************************************************/
u8 Boot_Identity_Provision(u16 first, u16 second, u16 size, u32 seed) //20261114-5 add
{
    Identity_Page page_a;
    Identity_Page page_b;

    if (!seed || seed == 0xFFFFFFFFUL || !Identity_Layout_Valid(first, second, size) ||
        !Identity_Scan(first, size, &page_a) || !Identity_Scan(second, size, &page_b) ||
        !page_a.blank || !page_b.blank) {
        return 0;
    }
    return Identity_Append(first, seed);
}
#endif
