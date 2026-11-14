#include "snapshot4g.h"
#include "uart4.h"
#include "Modbus_New.h"
#include "Display.h"
#include "HT1622.h"

#if (Model_OUT == Model_1C1)
#define SNAPSHOT_FUNCTION 0x43
#define SNAPSHOT_FORMAT 1
#define SNAPSHOT_OPERATION 1
#define SNAPSHOT_HEADER_SIZE 11
#define SNAPSHOT_DATA_SIZE 77
#define SNAPSHOT_REQUEST_SIZE 10
#define SNAPSHOT_INVALID 0xFFFF
#define SNAPSHOT_CONTEXT_UNAVAILABLE 0x32 //20261114-5 add 执行代次或时基尚未验证

/******************************************************************************
 * 函数名称       : Snapshot_Put16
 * 功能描述       : 将十六位字段按高字节在前写入共享帧。
 * 输入参数       : offset 帧内偏移，value 字段值
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
static void Snapshot_Put16(u8 offset, u16 value) //20261114-2 add
{
    uart4_frame[offset] = value >> 8;
    uart4_frame[offset + 1] = (u8)value;
}

/******************************************************************************
 * 函数名称       : Snapshot_Put32
 * 功能描述       : 将三十二位字段按高字在前写入共享帧。
 * 输入参数       : offset 帧内偏移，value 字段值
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
static void Snapshot_Put32(u8 offset, u32 value) //20261114-2 add
{
    Snapshot_Put16(offset, (u16)(value >> 16));
    Snapshot_Put16(offset + 2, (u16)value);
}

/******************************************************************************
 * 函数名称       : Snapshot_Write_Data
 * 功能描述       : 在共享帧中构造同一逻辑采样点的二十八项快照。
 * 输入参数       : base 快照块起点
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
static void Snapshot_Write_Data(u8 base) //20261114-6 change
{
    bit interrupt_enabled;
    u8 adc_valid;
    u8 sensor_type;
    u8 water_direction;
    u8 display_status;
    u8 pump_status;
    u8 offset;
    u16 voltage;
    u16 current_a;
#if (Model_Voltage == Model_3PH)
    u16 current_b;
    u16 current_c;
#endif
    u32 faults;
    u32 valid;
    u32 supported;

    interrupt_enabled = EA;
    EA = 0;
    adc_valid = sys.snapshot_adc_valid;
    voltage = sys.Voltage_Line;
    current_a = sys.Current_A1;
#if (Model_Voltage == Model_3PH)
    current_b = sys.Current_B1;
    current_c = sys.Current_C1;
#endif
    EA = interrupt_enabled;

#if (Model_Voltage == Model_3PH)
    supported = 0x0FFFFFFFUL;
#else
    supported = 0x0FFFFFF3UL; // 单相只支持第一相电流
#endif
    valid = supported;
    if (!(adc_valid & SNAPSHOT_VOLTAGE_VALID)) {
        valid &= ~1UL;
        voltage = SNAPSHOT_INVALID;
    }
    if (!(adc_valid & SNAPSHOT_CURRENT_VALID)) {
        valid &= ~0x0EUL;
        current_a = SNAPSHOT_INVALID;
#if (Model_Voltage == Model_3PH)
        current_b = SNAPSHOT_INVALID;
        current_c = SNAPSHOT_INVALID;
#endif
    }

    sensor_type = config.Auto_Mode;
    water_direction = config.Sensor_Mode;
    if (!sys.sensor_value_valid || sensor_type != sys.Auto_Mode) {
        valid &= ~0x0800UL;
        Snapshot_Put32(base + 28, 0xFFFFFFFFUL);
    } else {
        Snapshot_Put32(base + 28, sys.SensorOut);
    }
    Snapshot_Put32(base + 0, supported);
    Snapshot_Put32(base + 4, valid);
    Snapshot_Put16(base + 8, voltage);
    Snapshot_Put16(base + 10, current_a);
#if (Model_Voltage == Model_3PH)
    Snapshot_Put16(base + 12, current_b);
    Snapshot_Put16(base + 14, current_c);
#else
    Snapshot_Put16(base + 12, SNAPSHOT_INVALID);
    Snapshot_Put16(base + 14, SNAPSHOT_INVALID);
#endif

    pump_status = 0;
    switch (config.Hand_Auto_timer_Flag) {
    case RUNMODE_MANUAL:
        pump_status = 1;
        break;
    case RUNMODE_AUTO:
        pump_status = 6;
        break;
    case RUNMODE_TIMER:
        pump_status = 10;
        break;
    }
    if (sys.Pump_A_state) {
        pump_status |= 0x10;
    }
    Snapshot_Put16(base + 16, pump_status);
    faults = sys.Fault_Error; // 故障码和完整故障位图使用同一原值
    Snapshot_Put16(base + 18, BF006_Fault_Code(faults));
    Snapshot_Put32(base + 20, faults);

    uart4_frame[base + 24] = (LCD_WATER_Hlevel1 ? 1 : 0) | (LCD_WATER_Hlevel2 ? 2 : 0) |
                      (LCD_WATER_Hlevel3 ? 4 : 0) | (LCD_WATER_Hlevel4 ? 8 : 0) |
                      (LCD_WATER_Hlevel5 ? 16 : 0) | (LCD_WATER_Hlevel6 ? 32 : 0);
    uart4_frame[base + 25] = (LCD_WATER_Llevel1 ? 1 : 0) | (LCD_WATER_Llevel2 ? 2 : 0) |
                      (LCD_WATER_Llevel3 ? 4 : 0) | (LCD_WATER_Llevel4 ? 8 : 0) |
                      (LCD_WATER_Llevel5 ? 16 : 0) | (LCD_WATER_Llevel6 ? 32 : 0);
    uart4_frame[base + 26] = sys.shang_shuiwei;
    uart4_frame[base + 27] = sys.xia_shuiwei;
    uart4_frame[base + 32] = water_direction;
    display_status = sys.sensor_err ? 1 : 0;
    if (Dis7_Flash == 1 || sys.shang_shuiwei == 2) {
        display_status |= 8;
    }
    if (Dis8_Flash == 1) {
        display_status |= 16;
    }
    Snapshot_Put16(base + 33, display_status);
    uart4_frame[base + 35] = sensor_type;
    uart4_frame[base + 36] = water_direction;

    Snapshot_Put16(base + 37, config.Sensor_Rang); // 直接报告配置单位及原始值
    Snapshot_Put16(base + 39, config.Sensor_ON1);
    Snapshot_Put16(base + 41, config.Sensor_STOP);
    Snapshot_Put16(base + 43, config.Sensor_overFlow);
    Snapshot_Put16(base + 45, config.UVP_Value);
    Snapshot_Put16(base + 47, config.OVP_Value);
    Snapshot_Put16(base + 49, config.Set_OVERLOAD_Current1);
    Snapshot_Put16(base + 51, config.Set_STANDBY_Current1);
    Snapshot_Put16(base + 53, config.AUTO_Timing_Run);
    Snapshot_Put16(base + 55, config.AUTO_Timing_Stop);
    uart4_frame[base + 57] = Dev_Model;
    uart4_frame[base + 58] = BF006_Device_Spec();
    Snapshot_Put16(base + 59, MODBUS_NEW_VERSION);
    for (offset = 61; offset < SNAPSHOT_DATA_SIZE; offset++) {
        uart4_frame[base + offset] = 0;
    }
}

#ifdef BF006_CONTEXT_SIMULATION
#define SNAPSHOT_CONTEXT_SIZE 37
#define SNAPSHOT_CONTEXT_DATA 48

extern u8 Snapshot_Test_Context(u32 *boot, u32 *revision, u32 *uptime);

/******************************************************************************
 * 函数名称       : Snapshot_Read_Field
 * 功能描述       : 从同一快照提取单项值及支持和有效状态。
 * 输入参数       : field 字段编号，actual 用于返回原始整数值
 * 返回值         : 成功、不支持或当前无有效读数
 * 日期            : 2026.11.14
 ******************************************************************************/
static u8 Snapshot_Read_Field(u8 field, u32 *actual) //20261114-6 add
{
    static const u8 code offsets[] = {8,10,12,14,16,18,20,24,25,26,27,28,32,33,35,36,
                                     37,39,41,43,45,47,49,51,53,55,57,59,61};
    u8 bit_index;
    u8 bitmap_offset;
    u8 offset;
    u8 end;
    u8 mask;

    *actual = 0;
    if (!field || field >= sizeof(offsets)) {
        return 0x29;
    }
    bit_index = field - 1;
    bitmap_offset = SNAPSHOT_CONTEXT_DATA + 3 - (bit_index >> 3);
    mask = 1 << (bit_index & 7);
    if (!(uart4_frame[bitmap_offset] & mask)) {
        return 0x29;
    }
    if (!(uart4_frame[bitmap_offset + 4] & mask)) {
        return 0x35;
    }
    offset = offsets[field - 1];
    end = offsets[field];
    while (offset < end) {
        *actual = (*actual << 8) | uart4_frame[SNAPSHOT_CONTEXT_DATA + offset++];
    }
    return 0;
}

/******************************************************************************
 * 函数名称       : Snapshot_Field_Reply
 * 功能描述       : 从同一次采样生成单项短应答或附完整快照的应答。
 * 输入参数       : length 用于返回数据区长度
 * 返回值         : 协议结果码
 * 日期            : 2026.11.14
 ******************************************************************************/
static u8 Snapshot_Field_Reply(u8 *length) //20261114-6 add
{
    u8 field;
    u8 mode;
    u8 result;
    u8 flags;
    u8 state;
    u32 boot;
    u32 revision;
    u32 uptime;
    u32 actual;

    if (!Snapshot_Test_Context(&boot, &revision, &uptime)) {
        return SNAPSHOT_CONTEXT_UNAVAILABLE;
    }
    field = uart4_frame[10];
    mode = uart4_frame[11];
    Snapshot_Write_Data(SNAPSHOT_CONTEXT_DATA);
    state = uart4_frame[SNAPSHOT_CONTEXT_DATA + 17];
    actual = 0;
    if (mode > 1) {
        result = 0x33;
    } else {
        result = Snapshot_Read_Field(field, &actual);
    }
    flags = 1;
    if (!result) {
        flags |= 2;
    }
    if (mode == 1) {
        flags |= 16;
    }
    memset(&uart4_frame[SNAPSHOT_HEADER_SIZE], 0, SNAPSHOT_CONTEXT_SIZE);
    Snapshot_Put32(19, boot);
    Snapshot_Put32(23, revision);
    Snapshot_Put32(27, uptime);
    uart4_frame[31] = result ? 1 : 0;
    uart4_frame[32] = flags;
    Snapshot_Put16(33, state);
    uart4_frame[35] = field;
    Snapshot_Put32(36, actual);
    *length = SNAPSHOT_CONTEXT_SIZE + (mode == 1 ? SNAPSHOT_DATA_SIZE : 0);
    return result;
}
#endif

/******************************************************************************
 * 函数名称       : Snapshot4G_Process
 * 功能描述       : 验证串口请求并按原快照格式启动应答。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void Snapshot4G_Process(void) //20261114-6 change
{
    u8 length;
    u8 result;
    u8 data_length;
    u16 crc;

    length = UART4_Claim();
    if (!length) {
        return;
    }
    if (length < SNAPSHOT_REQUEST_SIZE || uart4_frame[0] == 0 || uart4_frame[0] == 0xFF ||
        uart4_frame[0] != config.Dev_Address || uart4_frame[1] != SNAPSHOT_FUNCTION ||
        uart4_frame[2] != 0xBF || uart4_frame[3] != 0x06 ||
        BF006_Modbus_CRC(uart4_frame, length) != 0) {
        UART4_Finish(0);
        return;
    }
    if (uart4_frame[5] == SNAPSHOT_OPERATION && length != SNAPSHOT_REQUEST_SIZE) {
        UART4_Finish(0); // 原快照只接受固定十字节请求
        return;
    }

    result = 0;
    data_length = 0;
    if (uart4_frame[4] != SNAPSHOT_FORMAT) {
        result = 1;
    } else if (uart4_frame[5] == 2 || uart4_frame[5] == 3) {
        if (length != 12 || uart4_frame[8] || uart4_frame[9]) {
            result = 3;
        } else {
            result = SNAPSHOT_CONTEXT_UNAVAILABLE;
        }
    } else if (uart4_frame[5] == 4) {
        if (length != 14 || uart4_frame[8] || uart4_frame[9] != 2) {
            result = 3;
        } else {
#ifdef BF006_CONTEXT_SIMULATION
            result = Snapshot_Field_Reply(&data_length);
#else
            result = SNAPSHOT_CONTEXT_UNAVAILABLE;
#endif
        }
    } else if (uart4_frame[5] != SNAPSHOT_OPERATION) {
        if (length != SNAPSHOT_REQUEST_SIZE &&
            (length < 12 || uart4_frame[8] != 0 || length != uart4_frame[9] + 12)) {
            result = 3;
        } else {
            result = 2; // 本阶段仅提供原快照
        }
    } else {
        data_length = SNAPSHOT_DATA_SIZE;
        Snapshot_Write_Data(SNAPSHOT_HEADER_SIZE);
    }

    uart4_frame[8] = result;
    uart4_frame[9] = 0;
    uart4_frame[10] = data_length;
    length = SNAPSHOT_HEADER_SIZE + data_length;
    crc = BF006_Modbus_CRC(uart4_frame, length);
    uart4_frame[length] = (u8)crc;
    uart4_frame[length + 1] = crc >> 8;
    UART4_Finish(length + 2);
}
#endif
