#include "config_store.h"
#include "EEPROM.h"
#include "boot_identity.h"

#define PAGE_SIZE 128
#define FIRST_PAGE 0
#define SECOND_PAGE PAGE_SIZE
#define PAYLOAD_OFFSET 8
#define COMMIT_OFFSET 127
#define LEGACY_PAGE_SIZE 512
#define SIMULATED_IAP_SIZE (3 * LEGACY_PAGE_SIZE)
#define IDENTITY_PAGE_A 1280
#define IDENTITY_PAGE_B 1312
#define IDENTITY_SECTOR_SIZE 32

volatile u8 xdata Test_Result _at_ 0x0BFE;
extern void interEidt(void);
extern void EidtEnter(void);
extern u16 editingData;
static u8 xdata pages[SIMULATED_IAP_SIZE];
static u8 xdata payload[CONFIG_STORE_SIZE];
static Config_Byte_Edit xdata edits[2];
static u8 cut_at;
static u8 mutations;
static u8 corrupt_commit_read;

/******************************************************************************
 * 函数名称       : EEPROM_read_n
 * 功能描述       : 模拟非易失存储及一次提交标记读回异常。
 * 输入参数       : address 读取地址，destination 目标，length 长度
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void EEPROM_read_n(u16 address, u8 *destination, u16 length)
{
    while (length--) {
        *destination = pages[address];
        if (corrupt_commit_read && (address == SECOND_PAGE + COMMIT_OFFSET ||
            address == BF006_CONFIG_SLOT_B + COMMIT_OFFSET) &&
            *destination == 0xA5) {
            *destination = 0xFF;
            corrupt_commit_read = 0;
        }
        destination++;
        address++;
    }
}

/******************************************************************************
 * 函数名称       : EEPROM_write_n
 * 功能描述       : 模拟仅能清位的写入，以及断电后停止写入。
 * 输入参数       : address 写入地址，source 数据，length 长度
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void EEPROM_write_n(u16 address, u8 *source, u16 length)
{
    while (length--) {
        if (mutations++ < cut_at) {
            pages[address] &= *source;
        }
        address++;
        source++;
    }
}

/******************************************************************************
 * 函数名称       : EEPROM_SectorErase
 * 功能描述       : 模拟相互独立的两个测试扇区。
 * 输入参数       : address 扇区起点
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void EEPROM_SectorErase(u16 address)
{
    if (mutations++ < cut_at) {
        if (address >= IDENTITY_PAGE_A) {
            memset(&pages[address], 0xFF, IDENTITY_SECTOR_SIZE);
        } else {
            memset(&pages[address], 0xFF, address >= LEGACY_PAGE_SIZE ? LEGACY_PAGE_SIZE : PAGE_SIZE);
        }
    }
}

/******************************************************************************
 * 函数名称       : Test_Done
 * 功能描述       : 在测试结果位置停止。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void Test_Done(void)
{
    while (1) {
    }
}

/******************************************************************************
 * 函数名称       : Complete_Save
 * 功能描述       : 循环推进保存，观察终态和运行配置是否提前生效。
 * 输入参数       : None
 * 返回值         : 保存结果
 * 日期            : 2026.11.14
 ******************************************************************************/
static u8 Complete_Save(void) //20261114-4 add
{
    u16 step;
    u8 state;

    for (step = 0; step < 600; step++) {
        state = Config_Store_Step();
        if (state != CONFIG_STORE_BUSY) {
            return state;
        }
        if (payload[3] != 0x12 || payload[91] != 0x34) {
            Test_Done();
        }
    }
    return CONFIG_STORE_FAILED;
}

/******************************************************************************
 * 函数名称       : main
 * 功能描述       : 验证保存、各写入位置断电及提交读回异常后的整组恢复。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void main(void) //20261114-4 add
{
    u8 interruption;
    u8 step;
    u16 loop;
    u32 boot;

    Test_Result = 1;
    cut_at = 0xFF;
    mutations = 0;
    memset(pages, 0xFF, sizeof(pages));
    memset(payload, 0, sizeof(payload));
    payload[3] = 0x12;
    payload[91] = 0x34;
    if (Config_Store_Open(FIRST_PAGE, SECOND_PAGE, payload) != CONFIG_STORE_EMPTY ||
        Config_Store_Begin(payload, 0, 0) != CONFIG_STORE_BUSY ||
        Complete_Save() != CONFIG_STORE_SAVED ||
        pages[FIRST_PAGE + COMMIT_OFFSET] != 0xA5) {
        Test_Done();
    }

    Test_Result = 2;
    edits[0].offset = 3;
    edits[0].value = 0xAB;
    edits[1].offset = 91;
    edits[1].value = 0xCD;
    for (interruption = 0; interruption < 129; interruption++) {
        memset(&pages[SECOND_PAGE], 0xFF, PAGE_SIZE);
        mutations = 0;
        cut_at = interruption;
        if (Config_Store_Open(FIRST_PAGE, SECOND_PAGE, payload) != CONFIG_STORE_READY ||
            Config_Store_Begin(payload, edits, 2) != CONFIG_STORE_BUSY) {
            Test_Done();
        }
        for (step = 0; step < 255 && mutations < interruption; step++) {
            Config_Store_Step();
        }
        if (Config_Store_Open(FIRST_PAGE, SECOND_PAGE, payload) != CONFIG_STORE_READY ||
            payload[3] != 0x12 || payload[91] != 0x34) {
            Test_Done();
        }
    }

    Test_Result = 3;
    memset(&pages[SECOND_PAGE], 0xFF, PAGE_SIZE);
    mutations = 0;
    cut_at = 0xFF;
    corrupt_commit_read = 1;
    if (Config_Store_Begin(payload, edits, 2) != CONFIG_STORE_BUSY ||
        Complete_Save() != CONFIG_STORE_UNCERTAIN ||
        Config_Store_Begin(payload, edits, 2) != CONFIG_STORE_UNCERTAIN ||
        Config_Store_Open(FIRST_PAGE, SECOND_PAGE, payload) != CONFIG_STORE_READY ||
        payload[3] != 0xAB || payload[91] != 0xCD) {
        Test_Done();
    }

    Test_Result = 4;
    mutations = 0;
    edits[0].value = 0xAB;
    edits[1].value = 0xCD;
    if (Config_Store_Begin(payload, edits, 2) != CONFIG_STORE_READY || mutations != 0) {
        Test_Done();
    }

    Test_Result = 5;
    memset(pages, 0xFF, sizeof(pages));
    memset(&config, 0, sizeof(config));
    config.banben = 18;
    config.Dev_Address = 7;
    config.Sensor_Rang = 100;
    memcpy(pages, &config, sizeof(config));
    sys_init();
    Par_Init();
    if (!config_trusted || config.Sensor_Rang != 100 || config_revision != 1) {
        Test_Done();
    }
    mutations = 0;
    save_pro();
    for (loop = 0; loop < 300; loop++) {
        Config_Background();
    }
    if (pages[BF006_CONFIG_SLOT_A + COMMIT_OFFSET] != 0xA5 ||
        memcmp(pages, &config, sizeof(config)) != 0) {
        Test_Done();
    }

    Test_Result = 6;
    mutations = 0;
    sys.Setting_Index = 14;
    interEidt();
    editingData = 9999;
    EidtEnter();
    if (config.Sensor_Rang != 100 || Config_Panel_Available()) {
        Test_Done();
    }
    for (loop = 0; loop < 300; loop++) {
        Config_Background();
    }
    if (config.Sensor_Rang != 9999 || sys.Sensor_Rang != 99990UL || config_revision != 2) {
        Test_Done();
    }
    sys_init();
    Par_Init();
    if (!config_trusted || config.Sensor_Rang != 9999) {
        Test_Done();
    }

    Test_Result = 8;
    sys.Setting_Index = 14;
    interEidt();
    editingData = 120;
    config.Hand_Auto_timer_Flag = RUNMODE_AUTO;
    Config_Observe_Changes();
    EidtEnter();
    if (config.Sensor_Rang != 9999 || !Config_Panel_Available()) {
        Test_Done();
    }

    Test_Result = 9;
    config.Sensor_STOP = 90;
    config.Sensor_ON1 = 50;
    config.Sensor_ON2 = 10;
    Config_Observe_Changes();
    sys.Setting_Index = 13;
    interEidt();
    editingData = CTROLTYPE_DRAIN;
    mutations = 0;
    EidtEnter();
    if (config.Sensor_Mode != CTROLTYPE_PUMP || config.Sensor_STOP != 90) {
        Test_Done();
    }
    for (loop = 0; loop < 300; loop++) {
        Config_Background();
    }
    if (config.Sensor_Mode != CTROLTYPE_DRAIN || config.Sensor_STOP != 10 ||
        config.Sensor_ON1 != 50 || config.Sensor_ON2 != 90) {
        Test_Done();
    }

    Test_Result = 7;
    memset(pages, 0xFF, sizeof(pages));
    sys_init();
    Par_Init();
    sys.Pump_A_ON = 1;
    Sys_Control();
    if (config_trusted || sys.Pump_A_ON || sys.Pump_A_state) {
        Test_Done();
    }

    Test_Result = 10;
    mutations = 0;
    cut_at = 0xFF;
    if (Boot_Identity_Advance(IDENTITY_PAGE_A, IDENTITY_PAGE_B, IDENTITY_SECTOR_SIZE) != 0 ||
        mutations || !Boot_Identity_Provision(IDENTITY_PAGE_A, IDENTITY_PAGE_B, IDENTITY_SECTOR_SIZE, 100)) {
        Test_Done();
    }
    for (boot = 101; boot < 108; boot++) {
        if (Boot_Identity_Advance(IDENTITY_PAGE_A, IDENTITY_PAGE_B, IDENTITY_SECTOR_SIZE) != boot) {
            Test_Done();
        }
    }
    if (Boot_Identity_Provision(IDENTITY_PAGE_A, IDENTITY_PAGE_B, IDENTITY_SECTOR_SIZE, 109)) {
        Test_Done();
    }

    Test_Result = 11;
    memset(&pages[IDENTITY_PAGE_A], 0xFF, 2 * IDENTITY_SECTOR_SIZE);
    if (!Boot_Identity_Provision(IDENTITY_PAGE_A, IDENTITY_PAGE_B, IDENTITY_SECTOR_SIZE, 100)) {
        Test_Done();
    }
    cut_at = 4;
    mutations = 0;
    if (Boot_Identity_Advance(IDENTITY_PAGE_A, IDENTITY_PAGE_B, IDENTITY_SECTOR_SIZE) != 0) {
        Test_Done();
    }
    cut_at = 0xFF;
    if (Boot_Identity_Advance(IDENTITY_PAGE_A, IDENTITY_PAGE_B, IDENTITY_SECTOR_SIZE) != 101) {
        Test_Done();
    }
    pages[IDENTITY_PAGE_A + 16] ^= 1;
    if (Boot_Identity_Advance(IDENTITY_PAGE_A, IDENTITY_PAGE_B, IDENTITY_SECTOR_SIZE) != 0) {
        Test_Done();
    }

    Test_Result = 12;
    memset(&pages[IDENTITY_PAGE_A], 0xFF, 2 * IDENTITY_SECTOR_SIZE);
    if (!Boot_Identity_Provision(IDENTITY_PAGE_A, IDENTITY_PAGE_B, IDENTITY_SECTOR_SIZE, 0xFFFFFFFEUL) ||
        Boot_Identity_Advance(IDENTITY_PAGE_A, IDENTITY_PAGE_B, IDENTITY_SECTOR_SIZE) != 0xFFFFFFFFUL ||
        Boot_Identity_Advance(IDENTITY_PAGE_A, IDENTITY_PAGE_B, IDENTITY_SECTOR_SIZE) != 0) {
        Test_Done();
    }
    Test_Result = 0;
    Test_Done();
}
