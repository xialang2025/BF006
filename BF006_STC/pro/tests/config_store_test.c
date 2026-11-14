#include "config_store.h"
#include "EEPROM.h"
#include "boot_identity.h"
#include "Modbus_New.h"
#include "snapshot4g.h"
#include "uart4.h"

#define PAGE_SIZE 128
#define FIRST_PAGE 0
#define SECOND_PAGE PAGE_SIZE
#define PAYLOAD_OFFSET 8
#define COMMIT_OFFSET 127
#define LEGACY_PAGE_SIZE 512
#define SIMULATED_IAP_SIZE (5 * LEGACY_PAGE_SIZE)
#define IDENTITY_PAGE_A 1280
#define IDENTITY_PAGE_B 1312
#define IDENTITY_SECTOR_SIZE 32

volatile u8 xdata Test_Result _at_ 0x0BFE;
extern void interEidt(void);
extern void EidtEnter(void);
extern u16 editingData;
static u8 xdata pages[SIMULATED_IAP_SIZE - 2 * (LEGACY_PAGE_SIZE - PAGE_SIZE)];
static u8 xdata payload[CONFIG_STORE_SIZE];
static Config_Byte_Edit xdata edits[2];
static u8 cut_at;
static u8 mutations;
static u8 corrupt_commit_read;
static u8 rs485_reply;
extern void UART4_int(void);
void Test_Done(void);

/******************************************************************************
 * 函数名称       : IAP_Test_Address
 * 功能描述       : 压缩两个身份扇区的空余测试空间。
 * 输入参数       : address 模拟逻辑地址
 * 返回值         : 模拟数组偏移
 * 日期            : 2026.11.14
 ******************************************************************************/
static u16 IAP_Test_Address(u16 address) //20261114-15 add
{
    if (address >= BF006_BOOT_SLOT_B) {
        return address - (LEGACY_PAGE_SIZE - PAGE_SIZE);
    }
    return address;
}

/******************************************************************************
 * 函数名称       : M_Uart_Send_Block
 * 功能描述       : 捕获现场串口写入应答的功能码。
 * 输入参数       : frame 应答数据，length 字节数
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void M_Uart_Send_Block(u8 *frame, u8 length)
{
    if (length >= 5) {
        rs485_reply = frame[1];
    }
}

/******************************************************************************
 * 函数名称       : Write_Range
 * 功能描述       : 通过现场串口接收入口写入传感器量程。
 * 输入参数       : value 量程原始值
 * 返回值         : 应答功能码
 * 日期            : 2026.11.14
 ******************************************************************************/
static u8 Write_Range(u16 value) //20261114-13 add
{
    u8 frame[8];
    u8 i;
    u16 crc;

    frame[0] = config.Dev_Address;
    frame[1] = 6;
    frame[2] = 0;
    frame[3] = 14;
    frame[4] = value >> 8;
    frame[5] = (u8)value;
    crc = BF006_Modbus_CRC(frame, 6);
    frame[6] = (u8)crc;
    frame[7] = crc >> 8;
    memset(&RecData, 0, sizeof(RecData));
    rs485_reply = 0;
    for (i = 0; i < sizeof(frame); i++) {
        Modbus_Receive_handle(frame[i]);
    }
    for (i = 0; i < 11; i++) {
        Modbus_Timer_handle();
    }
    Modbus_analysis();
    return rs485_reply;
}

/******************************************************************************
 * 函数名称       : Read_Board_Context
 * 功能描述       : 从串口四读取真实身份日志和定时器发布的单项结果。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
static void Read_Board_Context(void) //20261114-15 add
{
    u8 frame[14];
    u8 index;
    u8 value;
    u16 crc;
    memset(frame, 0, sizeof(frame));
    frame[0] = config.Dev_Address;
    frame[1] = 0x43;
    frame[2] = 0xBF;
    frame[3] = 6;
    frame[4] = 1;
    frame[5] = 4;
    frame[9] = 2;
    frame[10] = 17;
    crc = BF006_Modbus_CRC(frame, 12);
    frame[12] = (u8)crc;
    frame[13] = (u8)(crc >> 8);
    for (index = 0; index < 14; index++) {
        S4BUF = frame[index];
        S4CON |= 1;
        UART4_int();
    }
    for (index = 0; index < 11; index++) {
        Boot_Context_Tick();
        UART4_Tick();
    }
    Snapshot4G_Process();
    for (index = 0; index < 50; index++) {
        value = S4BUF;
        if ((index == 8 && value) || (index == 10 && value != 37) ||
            (index == 22 && value != 101) || (index == 30 && value != 11) ||
            (index == 35 && value != 17) || (index == 39 && value != 150)) {
            Test_Done();
        }
        SET_TI4();
        UART4_int();
    }
}

/******************************************************************************
 * 函数名称       : Board_Mode_Request
 * 功能描述       : 发送运行模式变更或查询原操作号并核对结果。
 * 输入参数       : operation 操作码，phase 预期阶段
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
static void Board_Mode_Request(u8 operation, u8 id, u8 phase, u8 expected_state, u8 result) //20261114-23 change
{
    u8 frame[55];
    u8 i;
    u8 length;
    u8 value;
    u16 crc;
    u32 boot;
    u32 revision;
    u32 now;
    u32 deadline;

    if (!Boot_Context_Read(&boot, &revision, &now)) {
        Test_Done();
    }
    memset(frame, 0, sizeof(frame));
    frame[0] = config.Dev_Address;
    frame[1] = 0x43;
    frame[2] = 0xBF;
    frame[3] = 6;
    frame[4] = 1;
    frame[5] = operation;
    frame[9] = operation == 0x10 ? 22 : (operation == 0x11 ? 26 : (operation == 0x12 ? 38 : 13));
    frame[17] = id;
    frame[21] = 101;
    if (operation != 0x13) {
        frame[22] = (u8)(revision >> 24);
        frame[23] = (u8)(revision >> 16);
        frame[24] = (u8)(revision >> 8);
        frame[25] = (u8)revision;
        deadline = id >= 11 ? now + 1000UL : (id >= 6 ? 2042UL : 250UL);
        if (id == 11) {
            deadline = 1000UL;
        }
        frame[26] = (u8)(deadline >> 24);
        frame[27] = (u8)(deadline >> 16);
        frame[28] = (u8)(deadline >> 8);
        frame[29] = (u8)deadline;
        frame[31] = operation == 0x10 ? 5 : 17;
        if (id == 8) {
            frame[31] = 2;
        } else if (id == 9 || id == 10) {
            frame[31] = 6;
        }
        frame[35] = 200;
        if (operation == 0x12) {
            frame[31] = 1;
            frame[32] = 3;
            frame[33] = 16;
            frame[35] = 0;
            frame[38] = 18;
            frame[42] = 20;
            frame[43] = 19;
            frame[47] = 90;
            if (id == 7) {
                frame[37] = CTROLTYPE_DRAIN;
                frame[42] = 50;
                frame[47] = 10;
            }
            if (id == 4) {
                frame[9] = 43;
                frame[31] = 2;
                frame[32] = 4;
                frame[33] = 21;
                frame[36] = VoltageValue == Vol_380 ? 1 : 0;
                frame[37] = VoltageValue == Vol_380 ? 64 : 150;
                frame[38] = 22;
                frame[41] = 1;
                frame[42] = VoltageValue == Vol_380 ? 224 : 4;
                frame[43] = 23;
                frame[47] = 100;
                frame[48] = 24;
            }
        }
    }
    length = frame[9] + 12;
    crc = BF006_Modbus_CRC(frame, length - 2);
    frame[length - 2] = (u8)crc;
    frame[length - 1] = (u8)(crc >> 8);
    for (i = 0; i < length; i++) {
        S4BUF = frame[i];
        S4CON |= 1;
        UART4_int();
    }
    for (i = 0; i < 11; i++) {
        Boot_Context_Tick();
        UART4_Tick();
    }
    Snapshot4G_Process();
    for (i = 0; i < 50; i++) {
        value = S4BUF;
        if ((i == 8 && value != result) || (i == 10 && value != 37) ||
            (i == 31 && value != phase) || (i == 34 && value != expected_state)) {
            Test_Done();
        }
        SET_TI4();
        UART4_int();
    }
}

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
        *destination = address >= BF006_BOOT_SLOT_A && (address & 511) >= PAGE_SIZE ? 0xFF :
                       pages[IAP_Test_Address(address)];
        if (corrupt_commit_read && (address == SECOND_PAGE + COMMIT_OFFSET ||
            (mutations >= 129 && (address == BF006_CONFIG_SLOT_A + COMMIT_OFFSET ||
             address == BF006_CONFIG_SLOT_B + COMMIT_OFFSET))) &&
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
            pages[IAP_Test_Address(address)] &= *source;
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
        if (address >= BF006_BOOT_SLOT_A) {
            memset(&pages[IAP_Test_Address(address)], 0xFF, PAGE_SIZE);
        } else if (address >= IDENTITY_PAGE_A) {
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

    Test_Result = 13;
    mutations = 0;
    if (Write_Range(150) != 6 || config.Sensor_Rang != 9999 || Write_Range(160) != 0x86) {
        Test_Done();
    }
    for (loop = 0; loop < 300; loop++) {
        Config_Background();
    }
    sys_init();
    Par_Init();
    if (config.Sensor_Rang != 150 || sys.Sensor_Rang != 1500) {
        Test_Done();
    }

    Test_Result = 14;
    mutations = 0;
    if (!Boot_Identity_Provision(BF006_BOOT_SLOT_A, BF006_BOOT_SLOT_B, 512, 100)) {
        Test_Done();
    }
    Boot_Context_Init();
    uart_init();
    Read_Board_Context();

    Test_Result = 15;
    config.Hand_Auto_timer_Flag = RUNMODE_MANUAL;
    mutations = 0;
    Board_Mode_Request(0x10, 1, 2, 1, 0);
    if (config.Hand_Auto_timer_Flag != RUNMODE_MANUAL) {
        Test_Done();
    }

    for (loop = 0; loop < 300; loop++) {
        Config_Background();
    }
    Board_Mode_Request(0x13, 1, 3, 6, 0);
    sys_init();
    Par_Init();
    if (config.Hand_Auto_timer_Flag != RUNMODE_AUTO) {
        Test_Done();
    }

    Test_Result = 16;
    config.Hand_Auto_timer_Flag = RUNMODE_MANUAL;
    config.Auto_Mode = SENSORTYPE_WATER4_20MA;
    mutations = 0;
    Board_Mode_Request(0x11, 2, 2, 1, 0);
    if (config.Sensor_Rang != 150) {
        Test_Done();
    }
    for (loop = 0; loop < 300; loop++) {
        Config_Background();
    }
    Board_Mode_Request(0x13, 2, 3, 1, 0);
    sys_init();
    Par_Init();
    if (config.Sensor_Rang != 200 || sys.Sensor_Rang != 2000) {
        Test_Done();
    }

    Test_Result = 17;
    mutations = 0;
    Board_Mode_Request(0x12, 3, 2, 1, 0);
    if (config.Sensor_Mode != CTROLTYPE_DRAIN || config.Sensor_ON1 != 50 || config.Sensor_STOP != 10) {
        Test_Done();
    }
    for (loop = 0; loop < 300; loop++) {
        Config_Background();
    }
    Board_Mode_Request(0x13, 3, 3, 1, 0);
    sys_init();
    Par_Init();
    if (config.Sensor_Mode != CTROLTYPE_PUMP || config.Sensor_ON1 != 20 || config.Sensor_STOP != 90 ||
        config.Sensor_Rang != 200) {
        Test_Done();
    }

    Test_Result = 18;
    mutations = 0;
    Board_Mode_Request(0x12, 4, 2, 1, 0);
    if (config.UVP_Value || config.OVP_Value || config.Set_OVERLOAD_Current1) {
        Test_Done();
    }
    for (loop = 0; loop < 300; loop++) {
        Config_Background();
    }
    Board_Mode_Request(0x13, 4, 3, 1, 0);
    sys_init();
    Par_Init();
    if (config.UVP_Value != (VoltageValue == Vol_380 ? 320 : 150) ||
        config.OVP_Value != (VoltageValue == Vol_380 ? 480 : 260) ||
        config.Set_OVERLOAD_Current1 != 100 || config.Set_STANDBY_Current1 || config.Sensor_Rang != 200) {
        Test_Done();
    }

    Test_Result = 19;
    mutations = 0;
    Board_Mode_Request(0x10, 5, 2, 1, 0);
    for (loop = 0; loop < 300; loop++) {
        Boot_Context_Tick();
        Snapshot4G_Before_Save();
        Config_Background();
    }
    Board_Mode_Request(0x13, 5, 5, 1, 0x28);
    sys_init();
    Par_Init();
    if (config.Hand_Auto_timer_Flag != RUNMODE_MANUAL) {
        Test_Done();
    }

    Test_Result = 20;
    mutations = 0;
    corrupt_commit_read = 1;
    Board_Mode_Request(0x10, 6, 2, 1, 0);
    for (loop = 0; loop < 300; loop++) {
        Boot_Context_Tick();
        Snapshot4G_Before_Save();
        Config_Background();
        Snapshot4G_Background();
    }
    if (config.Hand_Auto_timer_Flag != RUNMODE_AUTO) {
        Test_Done();
    }
    Board_Mode_Request(0x13, 6, 6, 1, 0x28);

    Test_Result = 21;
    config.Hand_Auto_timer_Flag = RUNMODE_MANUAL;
    mutations = 0;
    Board_Mode_Request(0x12, 7, 2, 1, 0);
    Config_Background();
    Test_Result = 22;
    Board_Mode_Request(0x10, 8, 4, 1, 0);
    for (loop = 0; loop < 300; loop++) {
        Config_Background();
        Snapshot4G_Background();
    }
    Test_Result = 23;
    Board_Mode_Request(0x13, 7, 5, 1, 0x28);
    Test_Result = 24;
    Board_Mode_Request(0x13, 8, 4, 1, 0);
    sys_init();
    Par_Init();
    if (config.Sensor_Mode != CTROLTYPE_PUMP || config.Sensor_ON1 != 20 || config.Sensor_STOP != 90) {
        Test_Done();
    }

    Test_Result = 25;
    config.Hand_Auto_timer_Flag = RUNMODE_MANUAL;
    config.AUTO_Timing_Run = 2;
    config.AUTO_Timing_Stop = 3;
    sys.Auto_timer_count = 17;
    mutations = 0;
    Board_Mode_Request(0x10, 9, 2, 1, 0);
    for (loop = 0; loop < 300; loop++) {
        Snapshot4G_Before_Save();
        Config_Background();
        Snapshot4G_Background();
    }
    Board_Mode_Request(0x13, 9, 3, 10, 0);
    if (sys.autoTimerFlag != 1 || sys.Auto_timer_count != 0) {
        Test_Done();
    }
    mutations = 0;
    sys.Auto_timer_count = 17;
    Board_Mode_Request(0x10, 10, 4, 10, 0);
    if (mutations || sys.Auto_timer_count != 17) {
        Test_Done();
    }
    sys_init();
    Par_Init();
    if (config.Hand_Auto_timer_Flag != RUNMODE_TIMER || config.AUTO_Timing_Run != 2 || config.AUTO_Timing_Stop != 3) {
        Test_Done();
    }

    Test_Result = 26;
    config.Hand_Auto_timer_Flag = RUNMODE_MANUAL;
    Board_Mode_Request(0x11, 11, 4, 1, 0);
    for (boot = 0; boot < 600001UL; boot++) {
        Boot_Context_Tick();
    }
    Board_Mode_Request(0x11, 11, 1, 1, 0x2C);
    Board_Mode_Request(0x13, 11, 6, 1, 0x2C);

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
