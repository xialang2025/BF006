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
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
static void Snapshot_Write_Data(void) //20261114-2 add
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
        Snapshot_Put32(39, 0xFFFFFFFFUL);
    } else {
        Snapshot_Put32(39, sys.SensorOut);
    }
    Snapshot_Put32(11, supported);
    Snapshot_Put32(15, valid);
    Snapshot_Put16(19, voltage);
    Snapshot_Put16(21, current_a);
#if (Model_Voltage == Model_3PH)
    Snapshot_Put16(23, current_b);
    Snapshot_Put16(25, current_c);
#else
    Snapshot_Put16(23, SNAPSHOT_INVALID);
    Snapshot_Put16(25, SNAPSHOT_INVALID);
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
    Snapshot_Put16(27, pump_status);
    faults = sys.Fault_Error; // 故障码和完整故障位图使用同一原值
    Snapshot_Put16(29, BF006_Fault_Code(faults));
    Snapshot_Put32(31, faults);

    uart4_frame[35] = (LCD_WATER_Hlevel1 ? 1 : 0) | (LCD_WATER_Hlevel2 ? 2 : 0) |
                      (LCD_WATER_Hlevel3 ? 4 : 0) | (LCD_WATER_Hlevel4 ? 8 : 0) |
                      (LCD_WATER_Hlevel5 ? 16 : 0) | (LCD_WATER_Hlevel6 ? 32 : 0);
    uart4_frame[36] = (LCD_WATER_Llevel1 ? 1 : 0) | (LCD_WATER_Llevel2 ? 2 : 0) |
                      (LCD_WATER_Llevel3 ? 4 : 0) | (LCD_WATER_Llevel4 ? 8 : 0) |
                      (LCD_WATER_Llevel5 ? 16 : 0) | (LCD_WATER_Llevel6 ? 32 : 0);
    uart4_frame[37] = sys.shang_shuiwei;
    uart4_frame[38] = sys.xia_shuiwei;
    uart4_frame[43] = water_direction;
    display_status = sys.sensor_err ? 1 : 0;
    if (Dis7_Flash == 1 || sys.shang_shuiwei == 2) {
        display_status |= 8;
    }
    if (Dis8_Flash == 1) {
        display_status |= 16;
    }
    Snapshot_Put16(44, display_status);
    uart4_frame[46] = sensor_type;
    uart4_frame[47] = water_direction;

    Snapshot_Put16(48, config.Sensor_Rang); // 直接报告配置单位及原始值
    Snapshot_Put16(50, config.Sensor_ON1);
    Snapshot_Put16(52, config.Sensor_STOP);
    Snapshot_Put16(54, config.Sensor_overFlow);
    Snapshot_Put16(56, config.UVP_Value);
    Snapshot_Put16(58, config.OVP_Value);
    Snapshot_Put16(60, config.Set_OVERLOAD_Current1);
    Snapshot_Put16(62, config.Set_STANDBY_Current1);
    Snapshot_Put16(64, config.AUTO_Timing_Run);
    Snapshot_Put16(66, config.AUTO_Timing_Stop);
    uart4_frame[68] = Dev_Model;
    uart4_frame[69] = BF006_Device_Spec();
    Snapshot_Put16(70, MODBUS_NEW_VERSION);
    for (offset = 72; offset < SNAPSHOT_HEADER_SIZE + SNAPSHOT_DATA_SIZE; offset++) {
        uart4_frame[offset] = 0;
    }
}

/******************************************************************************
 * 函数名称       : Snapshot4G_Process
 * 功能描述       : 验证串口请求并按原快照格式启动应答。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void Snapshot4G_Process(void) //20261114-2 add
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
    } else if (uart4_frame[5] != SNAPSHOT_OPERATION) {
        if (length != SNAPSHOT_REQUEST_SIZE &&
            (length < 12 || uart4_frame[8] != 0 || length != uart4_frame[9] + 12)) {
            result = 3;
        } else {
            result = 2; // 本阶段仅提供原快照
        }
    } else {
        data_length = SNAPSHOT_DATA_SIZE;
        Snapshot_Write_Data();
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
