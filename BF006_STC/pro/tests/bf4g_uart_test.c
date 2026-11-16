#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uart4.h"
#include "configPar.h"
#include "ad.h"
#include "Display.h"
#include "HT1622.h"

u8 S4CON, S4BUF, ACC, IE2, P_SW2, T4T3M, TH4, TL4, TH3, TL3, AUXINTIF, EA;
u16 Limit_Current;
const u16 Value_OVP = 480;
static u8 flash[117];
static u8 flash_writes;
static u32 control_faults;
union NUM_REG NUM5, NUM6, NUM7, NUM8;
u8 LCD_P3, LCD_COL;
void lcd_sensor_probe(void);
sys_t sys;
bit Boot_Flag;
u8 LowWater_M, LowWater_H, HighWater_M, HighWater_H, PUMP_ADD, OverFlow_P, P34;
u8 RELAY1, RELAY2, RELAY3;
volatile bit voltage_sample_ready, current_sample_ready, sensor_adc_fresh;
u8 Dis7_Flash, Dis8_Flash;
u8 LCD_WATER_Hlevel1, LCD_WATER_Hlevel2, LCD_WATER_Hlevel3,
   LCD_WATER_Hlevel4, LCD_WATER_Hlevel5, LCD_WATER_Hlevel6;
u8 LCD_WATER_Llevel1, LCD_WATER_Llevel2, LCD_WATER_Llevel3,
   LCD_WATER_Llevel4, LCD_WATER_Llevel5, LCD_WATER_Llevel6;

void UART4_int(void);
void UART4_Timer3_int(void);
void shuiwei_check(void);
void Timer_mode(void);
void RELAY_Auto_Control(void);
void FanXiu_pro(void);
void Fault_ReStart(void);
void Sys_Control(void);

/******************************************************************************
 * 函数名称       : Speak_Con
 * 功能描述       : 在软件模拟中接收蜂鸣提示。
 * 输入参数       : value，提示码。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
void Speak_Con(u8 value) //20261116-2 add
{
    (void)value;
}

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "%d: %s\n", __LINE__, #expr); exit(1); } } while (0)

/******************************************************************************
 * 函数名称       : EEPROM_read_n
 * 功能描述       : 从模拟数据区读取原参数副本。
 * 输入参数       : address，逻辑地址；out，目标；length，字节数。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
void EEPROM_read_n(u16 address, u8 *out, u16 length) //20261116-4 add
{
    CHECK(address == 0 && length == sizeof(flash));
    memcpy(out, flash, length);
}

/******************************************************************************
 * 函数名称       : EEPROM_SectorErase
 * 功能描述       : 在测试中模拟参数区整扇区擦除。
 * 输入参数       : address，逻辑地址。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
void EEPROM_SectorErase(u16 address) //20261116-4 add
{
    CHECK(address == 0);
    memset(flash, 0xff, sizeof(flash));
}

/******************************************************************************
 * 函数名称       : EEPROM_write_n
 * 功能描述       : 将参数副本写入模拟数据区。
 * 输入参数       : address，逻辑地址；source，参数副本；length，字节数。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
void EEPROM_write_n(u16 address, u8 *source, u16 length) //20261116-4 add
{
    CHECK(address == 0 && length == sizeof(flash));
    CHECK(flash[0] == 0xff && flash[sizeof(flash) - 1] == 0xff);
    memcpy(flash, source, length);
    flash_writes++;
}

/******************************************************************************
 * 函数名称       : Speak_Stop
 * 功能描述       : 在软件模拟中处理取消蜂鸣输出。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
void Speak_Stop(void) //20261116-5 add
{
}

static const u8 info_request[] = {0x01, 0x43, 0xbf, 0x06, 0x03, 0x01,
                                  0x00, 0x01, 0x00, 0x49, 0x84};

/******************************************************************************
 * 函数名称       : receive_byte
 * 功能描述       : 通过串口接收中断输入一个完整字节。
 * 输入参数       : value，接收字节。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void receive_byte(u8 value) //20261116-0 add
{
    S4BUF = value;
    S4CON |= 1;
    UART4_int();
}

/******************************************************************************
 * 函数名称       : timer_expire
 * 功能描述       : 模拟定时器三达到重装时长后的溢出中断。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void timer_expire(void) //20261116-0 add
{
    CHECK(T4T3M & 0x08);
    AUXINTIF |= 2;
    UART4_Timer3_int();
}

/******************************************************************************
 * 函数名称       : send_request
 * 功能描述       : 经接收中断输入请求，模拟静默并处理主循环任务。
 * 输入参数       : frame，报文；length，字节数。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void send_request(const u8 *frame, u8 length) //20261116-0 add
{
    u8 i;

    for (i = 0; i < length; i++) {
        receive_byte(frame[i]);
        uart4_service();
        CHECK(!uart4_sending());
    }
    CHECK(((u16)TH3 << 8 | TL3) == 65536UL - MAIN_Fosc / 12 / 100);
    timer_expire();
    uart4_service();
}

/******************************************************************************
 * 函数名称       : collect_reply
 * 功能描述       : 模拟发送中断逐字节收集应答，检查最后停止位的等待。
 * 输入参数       : out，收集位置；length，应答长度；interfere，途中是否注入字节。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void collect_reply(u8 *out, u8 length, u8 interfere) //20261116-0 add
{
    u8 i;

    for (i = 0; i < length; i++) {
        CHECK(uart4_sending());
        out[i] = S4BUF;
        if (i == 2 && interfere) {
            receive_byte(0x01);
            receive_byte(0x43);
        }
        S4CON |= 2;
        UART4_int();
    }
    CHECK(uart4_sending());
    CHECK(65536UL - ((u16)TH3 << 8 | TL3) > MAIN_Fosc / 12 / 9600);
    timer_expire();
    CHECK(!uart4_sending());
}

/******************************************************************************
 * 函数名称       : crc
 * 功能描述       : 独立按从高位到低位反馈的等价算法校验报文。
 * 输入参数       : frame，报文；length，计算字节数。
 * 返回值         : 低字节先传的校验值。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u16 crc(const u8 *frame, u8 length) //20261116-0 add
{
    u16 state = 0xffff;
    u16 result = 0;
    u8 mask;
    u8 i;

    for (i = 0; i < length; i++) {
        for (mask = 1; mask; mask <<= 1) {
            if (((state >> 15) & 1) != ((frame[i] & mask) != 0)) {
                state = (state << 1) ^ 0x8005;
            } else {
                state <<= 1;
            }
        }
    }
    for (i = 0; i < 16; i++) {
        result = (result << 1) | (state & 1);
        state >>= 1;
    }
    return result;
}

/******************************************************************************
 * 函数名称       : fill_crc
 * 功能描述       : 补齐测试请求的校验字节。
 * 输入参数       : frame，报文；length，含校验的总长度。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void fill_crc(u8 *frame, u8 length) //20261116-0 add
{
    u16 check = crc(frame, length - 2);

    frame[length - 2] = check;
    frame[length - 1] = check >> 8;
}

/******************************************************************************
 * 函数名称       : check_info
 * 功能描述       : 逐字段检查设备信息和校验值。
 * 输入参数       : interfere，是否模拟发送期收到字节。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_info(u8 interfere) //20261116-0 add
{
    u8 reply[27];
    static const u8 expected[] = {
        0x01, 0x43, 0xbf, 0x06, 0x03, 0x81, 0x00, 0x01, 0x10,
        0x00, 0x00, TEST_MODEL_BYTE, 0x93, 0x00, 0xc9, TEST_PHASES, TEST_PUMPS, TEST_VOLTS >> 8,
        TEST_VOLTS & 0xff, 0x00, 0x1f, 0x61, 0x10, 0x09, 0x08
    };

    send_request(info_request, sizeof(info_request));
    collect_reply(reply, sizeof(reply), interfere);
    CHECK(memcmp(reply, expected, sizeof(expected)) == 0);
    CHECK(crc(reply, sizeof(reply)) == 0);
}

/******************************************************************************
 * 函数名称       : check_error_range
 * 功能描述       : 核对错误应答合同和请求回显。
 * 输入参数       : request，请求；length，长度；status，预期错误码；object，预期目标；minimum／maximum，范围。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_error_range(u8 *request, u8 length, u8 status, u8 object,
                              u16 minimum, u16 maximum) //20261116-4 change
{
    u8 reply[21];

    fill_crc(request, length);
    send_request(request, length);
    collect_reply(reply, sizeof(reply), 0);
    CHECK(memcmp(reply, request, 4) == 0);
    CHECK(reply[4] == 3 && reply[5] == (request[5] | 0x80));
    CHECK(reply[6] == request[6] && reply[7] == request[7]);
    CHECK(reply[8] == 10 && reply[9] == status && reply[10] == object);
    CHECK(reply[11] == 0 && reply[12] == 0);
    CHECK(((u16)reply[13] << 8 | reply[14]) == minimum);
    CHECK(reply[15] == 0 && reply[16] == 0);
    CHECK(((u16)reply[17] << 8 | reply[18]) == maximum);
    CHECK(crc(reply, sizeof(reply)) == 0);
}

/******************************************************************************
 * 函数名称       : check_error
 * 功能描述       : 核对不携带取值范围的错误应答。
 * 输入参数       : request，请求；length，长度；status，错误码；object，目标。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_error(u8 *request, u8 length, u8 status, u8 object) //20261116-4 add
{
    check_error_range(request, length, status, object, 0, 0);
}

/******************************************************************************
 * 函数名称       : check_one_parameter
 * 功能描述       : 通过串口读取一个当前电流阈值。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_one_parameter(void) //20261116-1 add
{
    u8 request[] = {1, 0x43, 0xbf, 6, 3, 2, 0x12, 0x34, 2, 3, 1, 0, 0};
    u8 reply[17];

    config_loaded = 1;
    config.Set_OVERLOAD_Current1 = 900;
    fill_crc(request, sizeof(request));
    send_request(request, sizeof(request));
    collect_reply(reply, sizeof(reply), 0);
    CHECK(reply[5] == 0x82 && reply[6] == 0x12 && reply[7] == 0x34);
    CHECK(reply[8] == 6 && reply[9] == 0 && reply[10] == 3 && reply[11] == 1);
    CHECK(reply[12] == 0 && reply[13] == 3 && reply[14] == 0x84);
    CHECK(crc(reply, sizeof(reply)) == 0);
    config.Set_OVERLOAD_Current1 = 0;
}

/******************************************************************************
 * 函数名称       : read_parameters
 * 功能描述       : 读取一段参数并核对每项原值及状态。
 * 输入参数       : start，首项；count，数量；values，原值；qualities，质量。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void read_parameters(u8 start, u8 count, const u16 *values, const u8 *qualities) //20261116-1 add
{
    u8 request[] = {1, 0x43, 0xbf, 6, 3, 2, 0, 2, 2, start, count, 0, 0};
    u8 reply[62];
    u8 i;

    fill_crc(request, sizeof(request));
    send_request(request, sizeof(request));
    collect_reply(reply, 14 + 3 * count, 0);
    CHECK(reply[5] == 0x82 && reply[6] == 0 && reply[7] == 2);
    CHECK(reply[8] == 3 + 3 * count && reply[9] == 0 && reply[10] == start && reply[11] == count);
    for (i = 0; i < count; i++) {
        CHECK(reply[12 + 3 * i] == qualities[i]);
        CHECK(((u16)reply[13 + 3 * i] << 8 | reply[14 + 3 * i]) == values[i]);
    }
    CHECK(crc(reply, 14 + 3 * count) == 0);
}

/******************************************************************************
 * 函数名称       : check_parameter_ranges
 * 功能描述       : 验证两段读取、质量优先级、非法区间及未装载。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_parameter_ranges(void) //20261116-1 add
{
    static const u16 values[23] = {
        280, 84, 0, 900, 902, 901, 9999, 10000, 0, 9998, 999, 1, 3,
        1, 0, 7, 0, 6, 4, 5, 0, 1234, 2321
    };
    static const u8 qualities[23] = {
        0, 3, 0, 0, TEST_PUMPS == 1 ? 1 : 3, TEST_PUMPS == 1 ? 1 : 3, 0, 3, 0, 0,
        0, 0, TEST_PUMPS == 1 ? 1 : 3, 0, 0, 3, 0, TEST_PUMPS == 1 ? 1 : 3, 0, 3, 0, 0, 0
    };
    static const u16 zeros[] = {0, 0};
    static const u8 unloaded[] = {2, 2};
    u8 request[] = {1, 0x43, 0xbf, 6, 3, 2, 0, 3, 2, 23, 0, 0, 0};

    config.OVP_Value = 280;
    config.UVP_Value = 84;
    config.Set_STANDBY_Current1 = 0;
    config.Set_OVERLOAD_Current1 = 900;
    config.Set_STANDBY_Current2 = 902;
    config.Set_OVERLOAD_Current2 = 901;
    config.AUTO_Timing_Run = 9999;
    config.AUTO_Timing_Stop = 10000;
    config.Timer_Standby_Restart = 0;
    config.Timer_Unload_Restart = 9998;
    config.AUTO_FANGXIU = 999;
    config.Pump_AB_Mode = 3;
    config.Sensor_Mode = 1;
    config.Sensor_Rang = 0;
    config.Sensor_STOP = 7;
    config.Sensor_ON1 = 0;
    config.Sensor_ON2 = 6;
    config.Auto_Mode = 4;
    config.fault_mask = 5;
    config.Sensor_overFlow = 0;
    config.Total_Run_Hour = 1234;
    config.Total_Power_Day = 2321;

    read_parameters(0, 16, values, qualities);
    read_parameters(16, 7, values + 16, qualities + 16);
    read_parameters(22, 1, values + 22, qualities + 22);
    CHECK(config.Sensor_STOP == 7 && config.Sensor_ON2 == 6);
    check_error(request, sizeof(request), 4, 23); // 首项错误优先于数量错误。
    request[9] = 0;
    check_error(request, sizeof(request), 3, 0);
    request[10] = 17;
    check_error(request, sizeof(request), 3, 0);
    request[9] = 22;
    request[10] = 2;
    check_error(request, sizeof(request), 3, 22);
    request[8] = 1;
    request[9] = 23;
    check_error(request, sizeof(request) - 1, 3, 23);
    request[8] = 0;
    check_error(request, sizeof(request) - 2, 3, 0xff);
    config_loaded = 0;
    read_parameters(4, 2, zeros, unloaded);
    config_loaded = 1;
}

/******************************************************************************
 * 函数名称       : check_one_runtime_field
 * 功能描述       : 通过串口读取独立的工作电压。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_one_runtime_field(void) //20261116-2 add
{
    u8 request[] = {1, 0x43, 0xbf, 6, 3, 3, 0, 4, 2, 1, 1, 0, 0};
    u8 reply[19];

    voltage_sample_ready = 1;
    sys.Voltage_Line = 256;
    fill_crc(request, sizeof(request));
    send_request(request, sizeof(request));
    collect_reply(reply, sizeof(reply), 0);
    CHECK(reply[5] == 0x83 && reply[8] == 8 && reply[9] == 0);
    CHECK(reply[10] == 1 && reply[11] == 1 && reply[12] == 0);
    CHECK(reply[13] == 0 && reply[14] == 0 && reply[15] == 1 && reply[16] == 0);
    CHECK(crc(reply, sizeof(reply)) == 0);
}

/******************************************************************************
 * 函数名称       : read_runtime
 * 功能描述       : 读取一段运行字段，核对数据、质量及校验。
 * 输入参数       : start，首字段；count，数量；values，预期值；qualities，预期质量。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void read_runtime(u8 start, u8 count, const u32 *values, const u8 *qualities) //20261116-2 add
{
    u8 request[] = {1, 0x43, 0xbf, 6, 3, 3, 0, 5, 2, start, count, 0, 0};
    u8 reply[59];
    u8 i;
    u8 *entry;
    u32 value;

    fill_crc(request, sizeof(request));
    send_request(request, sizeof(request));
    collect_reply(reply, 14 + 5 * count, 0);
    CHECK(reply[5] == 0x83 && reply[6] == 0 && reply[7] == 5);
    CHECK(reply[8] == 3 + 5 * count && reply[9] == 0 && reply[10] == start && reply[11] == count);
    for (i = 0; i < count; i++) {
        entry = reply + 12 + 5 * i;
        value = (u32)entry[1] << 24 | (u32)entry[2] << 16 | (u32)entry[3] << 8 | entry[4];
        CHECK(entry[0] == qualities[i] && value == values[i]);
    }
    CHECK(crc(reply, 14 + 5 * count) == 0);
}

/******************************************************************************
 * 函数名称       : check_runtime_fields
 * 功能描述       : 覆盖二十四项、不同机型和未就绪及异常分支。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_runtime_fields(void) //20261116-2 add
{
    static const u32 values[] = {
        219, 11, TEST_PHASES == 3 ? 22 : 0, TEST_PHASES == 3 ? 33 : 0,
        TEST_PUMPS == 2 ? 44 : 0, TEST_PUMPS == 2 && TEST_PHASES == 3 ? 55 : 0,
        TEST_PUMPS == 2 && TEST_PHASES == 3 ? 66 : 0, TEST_PUMPS == 2 ? 0x1936 : 0x1816,
        2, 6, 0x146, 0x21, 0x0a, 2, 0, 12345, 30, 2, 1, 1, 2,
        (TEST_MODEL_BYTE << 8) | 0x93, 201, 0
    };
    static const u8 qualities[] = {
        0, 0, TEST_PHASES == 3 ? 0 : 1, TEST_PHASES == 3 ? 0 : 1, TEST_PUMPS == 2 ? 0 : 1,
        TEST_PHASES == 3 && TEST_PUMPS == 2 ? 0 : 1, TEST_PHASES == 3 && TEST_PUMPS == 2 ? 0 : 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
    };
    static const u32 zeros[] = {0, 0, 0};
    static const u8 valid[] = {0, 0, 0};
    static const u8 unavailable[] = {1, 1, 1};
    static const u8 unloaded[] = {2, 2, 2};
    static const u8 anomalies[] = {3, 3, 3};
    static const u32 faults[] = {0, 0x80000002UL, 0};
    u8 request[] = {1, 0x43, 0xbf, 6, 3, 3, 0, 6, 2, 0, 0, 0, 0};

    config.Hand_Auto_timer_Flag = RUNMODE_AUTO;
    config.Pump_AB_Mode = 2;
    config.Auto_Mode = SENSORTYPE_WATER4_20MA;
    config.Sensor_Mode = CTROLTYPE_DRAIN;
    config.Set_OVERLOAD_Current1 = 900;
    sys.Auto_Mode = SENSORTYPE_WATER4_20MA;
    sys.Voltage_Line = 219;
    sys.Current_A1 = 11;
    sys.Current_A2 = 44;
#if TEST_PHASES == 3
    sys.Current_B1 = 22;
    sys.Current_C1 = 33;
    sys.Current_B2 = 55;
    sys.Current_C2 = 66;
#endif
    sys.Pump_A_state = 1;
    sys.Pump_B_state = 1;
    sys.shang_shuiwei = 2;
    sys.xia_shuiwei = 0;
    sys.Fault_Error = 6;
    sys.SensorOut = 12345;
    sys.SensorLever = 2;
    sys.Sensor_OverFlow = 1;
    sys.flowover = 1;
    Dis8_Flash = 1;
    LCD_WATER_Hlevel1 = LCD_WATER_Hlevel6 = 1;
    LCD_WATER_Llevel2 = LCD_WATER_Llevel4 = 1;
    current_sample_ready = voltage_sample_ready = sensor_sample_ready = 1;
    water_status_ready = Boot_Flag = 1;

    read_runtime(1, 9, values, qualities);
    read_runtime(10, 9, values + 9, qualities + 9);
    read_runtime(19, 6, values + 18, qualities + 18);

    water_status_ready = 0;
    read_runtime(8, 1, zeros, unloaded);
    water_status_ready = 1;
    sys.shang_shuiwei = 9;
    read_runtime(8, 1, zeros, anomalies);
    read_runtime(14, 1, zeros, anomalies);
    sys.shang_shuiwei = 2;
    config.Pump_AB_Mode = 9;
    if (TEST_PUMPS == 1) {
        read_runtime(8, 1, values + 7, valid);
    } else {
        read_runtime(8, 1, zeros, anomalies);
    }
    config.Hand_Auto_timer_Flag = 9;
    read_runtime(8, 1, zeros, anomalies);
    sys.Fault_Error = faults[1];
    read_runtime(9, 3, faults, anomalies);

    config_loaded = 0;
    read_runtime(16, 3, zeros, unloaded);
    read_runtime(24, 1, zeros, unavailable);
    config_loaded = 1;
    current_sample_ready = 0;
    read_runtime(2, 1, zeros, unloaded);
    if (TEST_PHASES == 1) {
        read_runtime(3, 2, zeros, unavailable);
    }
    sensor_sample_ready = 0;
    read_runtime(16, 1, zeros, unloaded);
    sensor_sample_ready = 1;
    sys.SensorOut = 0;
    sys.sensor_err = 1;
    read_runtime(16, 1, zeros, anomalies);
    read_runtime(21, 1, zeros, anomalies);
    sys.sensor_err = 0;
    read_runtime(16, 1, zeros, valid);
    sys.Auto_Mode = SENSORTYPE_PRESS0_5V;
    read_runtime(18, 1, zeros, unloaded);
    read_runtime(20, 1, zeros, unloaded);
    config.Auto_Mode = SENSORTYPE_PRESS0_5V;
    sys.sensor_err = 1;
    read_runtime(16, 1, zeros, anomalies);
    sys.Auto_Mode = config.Auto_Mode = SENSORTYPE_POLE;
    read_runtime(16, 1, zeros, unavailable);
    read_runtime(21, 1, zeros, unavailable);
    read_runtime(20, 1, zeros, valid);
    sys.Auto_Mode = config.Auto_Mode = 5;
    read_runtime(18, 1, zeros, anomalies);
    config.Sensor_Mode = 2;
    read_runtime(19, 1, zeros, anomalies);

    check_error(request, sizeof(request), 4, 0);
    request[9] = 24;
    request[10] = 2;
    check_error(request, sizeof(request), 3, 24);
    request[9] = 1;
    request[10] = 10;
    check_error(request, sizeof(request), 3, 1);
}

/******************************************************************************
 * 函数名称       : check_sensor_sample
 * 功能描述       : 让实际换算代码处理两个模拟量输入的异常及有效零。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_sensor_sample(void) //20261116-2 add
{
    static const u32 zeros[] = {0};
    static const u8 valid[] = {0};
    static const u8 anomalous[] = {3};
    static const u8 unloaded[] = {2};

    config.Sensor_Mode = CTROLTYPE_PUMP;
    config.Auto_Mode = sys.Auto_Mode = SENSORTYPE_WATER4_20MA;
    sensor_adc_fresh = 0;
    sys.Cur_AD = 99;
    shuiwei_check();
    read_runtime(16, 1, zeros, unloaded);

    sensor_adc_fresh = 1;
    shuiwei_check();
    read_runtime(16, 1, zeros, anomalous);
    sys.Cur_AD = 100;
    shuiwei_check();
    read_runtime(16, 1, zeros, valid);

    config.Auto_Mode = sys.Auto_Mode = SENSORTYPE_PRESS0_5V;
    sys.Vol_AD = 1;
    shuiwei_check();
    read_runtime(16, 1, zeros, anomalous);
    sys.Vol_AD = 2;
    shuiwei_check();
    read_runtime(16, 1, zeros, valid);
}

/******************************************************************************
 * 函数名称       : read_history
 * 功能描述       : 读取一段历史，逐字节核对故障号、原值和质量。
 * 输入参数       : start，首槽；count，槽数；expected，预期条目。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void read_history(u8 start, u8 count, const u8 *expected) //20261116-3 add
{
    u8 request[] = {1, 0x43, 0xbf, 6, 3, 4, 0, 7, 2, start, count, 0, 0};
    u8 reply[62];

    fill_crc(request, sizeof(request));
    send_request(request, sizeof(request));
    collect_reply(reply, 14 + 6 * count, 0);
    CHECK(reply[5] == 0x84 && reply[6] == 0 && reply[7] == 7);
    CHECK(reply[8] == 3 + 6 * count && reply[9] == 0 && reply[10] == start && reply[11] == count);
    CHECK(memcmp(reply + 12, expected, 6 * count) == 0);
    CHECK(crc(reply, 14 + 6 * count) == 0);
}

/******************************************************************************
 * 函数名称       : check_history
 * 功能描述       : 验证全零欠压记录、空槽、异常原值、未装载和读取边界。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_history(void) //20261116-3 add
{
    static const u8 expected[] = {
        0, 0, 0, 0, 0, 0,
        4, 17, 0, 0, 0, 0,
        3, 255, 0x12, 0x34, 0x56, 0x78,
        0, 16, 0, 0, 0, 0,
        0, 15, 0, 0, 0, 0,
        0, 14, 0, 0, 0, 0,
        0, 13, 0, 0, 0, 0,
        0, 12, 0, 0, 0, 0,
        0, 11, 0, 0, 0, 0,
        0, 10, 0, 0, 0, 0
    };
    static const u8 unloaded[] = {2, 17, 0, 0, 0, 0};
    u8 request[] = {1, 0x43, 0xbf, 6, 3, 4, 0, 8, 2, 10, 0, 0, 0};
    config_t before;
    u8 i;

    for (i = 0; i < 10; i++) {
        config.Record_Type[i] = expected[6 * i + 1];
    }
    config.Record_Dat1[1] = 99;
    config.Record_Dat2[1] = 52;
    config.Record_Dat1[2] = 0x1234;
    config.Record_Dat2[2] = 0x5678;
    sys.save_flag = 0;
    memcpy(&before, &config, sizeof(before));

    read_history(0, 8, expected);
    read_history(8, 2, expected + 48);
    read_history(9, 1, expected + 54);
    config_loaded = 0;
    read_history(0, 1, unloaded);
    config_loaded = 1;
    check_error(request, sizeof(request), 4, 10);
    request[9] = 0;
    check_error(request, sizeof(request), 3, 0);
    request[10] = 9;
    check_error(request, sizeof(request), 3, 0);
    request[9] = 9;
    request[10] = 2;
    check_error(request, sizeof(request), 3, 9);
    CHECK(sys.save_flag == 0 && memcmp(&before, &config, sizeof(before)) == 0);
}

/******************************************************************************
 * 函数名称       : write_parameter_request
 * 功能描述       : 发出单写参数请求并核对成功应答。
 * 输入参数       : number，参数；value，期望的新参数值。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void write_parameter_request(u8 number, u16 value) //20261116-4 add
{
    u8 request[] = {1, 0x43, 0xbf, 6, 3, 0x10, 0x12, 0x34, 3, number, value >> 8, value, 0, 0};
    u8 reply[15];

    fill_crc(request, sizeof(request));
    send_request(request, sizeof(request));
    collect_reply(reply, sizeof(reply), 0);
    CHECK(reply[5] == 0x90 && reply[6] == 0x12 && reply[7] == 0x34);
    CHECK(reply[8] == 4 && reply[9] == 0 && reply[10] == number);
    CHECK(((u16)reply[11] << 8 | reply[12]) == value);
    CHECK(crc(reply, sizeof(reply)) == 0);
}

/******************************************************************************
 * 函数名称       : check_parameter_writes
 * 功能描述       : 验证合法赋值、模式独立性、非法参数和边界。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_parameter_writes(void) //20261116-4 add
{
    u8 request[] = {1, 0x43, 0xbf, 6, 3, 0x10, 0, 9, 3, 3, 3, 0x85, 0, 0};
    config_t expected;
    u8 mode;
    const u16 voltage_limit = TEST_VOLTS == 380 ? 480 : 280;

    memcpy(&expected, &config, sizeof(expected));
    write_parameter_request(0, voltage_limit);
    write_parameter_request(1, 85);
    write_parameter_request(2, 0);
    expected.OVP_Value = voltage_limit;
    expected.UVP_Value = 85;
    expected.Set_STANDBY_Current1 = 0;
    if (TEST_PUMPS == 2) {
        write_parameter_request(4, 901 - 1);
        write_parameter_request(5, 500);
        write_parameter_request(12, 2);
        expected.Set_STANDBY_Current2 = 900;
        expected.Set_OVERLOAD_Current2 = 500;
        expected.Pump_AB_Mode = 2;
    }
    for (mode = 0; mode < 3; mode++) {
        sys.Pump_A_state = mode != 1;
        config.Hand_Auto_timer_Flag = mode;
        write_parameter_request(3, 898 + mode);
        write_parameter_request(19, mode + 1);
        expected.Hand_Auto_timer_Flag = mode;
        expected.Set_OVERLOAD_Current1 = 898 + mode;
        expected.fault_mask = mode + 1;
        CHECK(memcmp(&config, &expected, sizeof(config)) == 0);
    }
    CHECK(sys.save_flag == 1);
    sys.save_flag = 0;
    write_parameter_request(3, 900);
    CHECK(sys.save_flag == 0);
    check_error_range(request, sizeof(request), 7, 3, 0, 900);
    request[9] = 0;
    check_error_range(request, sizeof(request), 7, 0, 85, voltage_limit);
    request[9] = 19;
    request[10] = 1;
    request[11] = 0;
    check_error_range(request, sizeof(request), 7, 19, 0, 4);
    if (TEST_PUMPS == 1) {
        request[9] = 4;
        check_error(request, sizeof(request), 6, 4);
        request[9] = 12;
        check_error(request, sizeof(request), 6, 12);
    } else {
        request[9] = 12;
        check_error_range(request, sizeof(request), 7, 12, 0, 2);
    }
    request[9] = 21;
    check_error(request, sizeof(request), 5, 21);
    request[9] = 22;
    check_error(request, sizeof(request), 5, 22);
    request[9] = 23;
    check_error(request, sizeof(request), 4, 23);
    request[9] = 19;
    config_loaded = 0;
    check_error(request, sizeof(request), 0x0e, 19);
    config_loaded = 1;
    CHECK(sys.save_flag == 0 && memcmp(&config, &expected, sizeof(config)) == 0);
}

/******************************************************************************
 * 函数名称       : check_parameter_save
 * 功能描述       : 经原保存入口验证停止位避让、节流合并及重新装载。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_parameter_save(void) //20261116-4 add
{
    u8 request[] = {1, 0x43, 0xbf, 6, 3, 0x10, 0, 10, 3, 3, 3, 0x83, 0, 0};
    static const u16 written[] = {899};
    static const u8 valid[] = {0};
    u16 i;

    CHECK(sizeof(config_t) == 117);
    config.banben = 18;
    fill_crc(request, sizeof(request));
    send_request(request, sizeof(request));
    CHECK(config.Set_OVERLOAD_Current1 == 899 && sys.save_flag == 1);
    for (i = 0; i < 15; i++) {
        CHECK(uart4_sending());
        save_pro();
        CHECK(flash_writes == 0);
        S4CON |= 2;
        UART4_int();
    }
    CHECK(uart4_sending());
    save_pro();
    CHECK(flash_writes == 0);
    timer_expire();
    save_pro();
    CHECK(flash_writes == 1 && sys.save_flag == 0);

    config.Set_OVERLOAD_Current1 = 0;
    Par_Init();
    CHECK(config.Set_OVERLOAD_Current1 == 899);
    read_parameters(3, 1, written, valid);
    write_parameter_request(3, 888);
    save_pro();
    write_parameter_request(19, 4);
    save_pro();
    CHECK(flash_writes == 1 && sys.save_flag == 1);
    for (i = 0; i < 199; i++) {
        save_pro();
    }
    CHECK(flash_writes == 2 && sys.save_flag == 0);
    config.Set_OVERLOAD_Current1 = 0;
    config.fault_mask = 0;
    Par_Init();
    CHECK(config.Set_OVERLOAD_Current1 == 888 && config.fault_mask == 4);
}

/******************************************************************************
 * 函数名称       : check_cycle_counter
 * 功能描述       : 验证实际循环计时跨越十六位边界。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_cycle_counter(void) //20261116-5 add
{
    config.Hand_Auto_timer_Flag = RUNMODE_TIMER;
    config.AUTO_Timing_Run = 9999;
    config.AUTO_Timing_Stop = 9999;
    sys.autoTimerFlag = 1;
    sys.Auto_timer_count = 65535;
    Timer_mode();
    RELAY_Auto_Control();
    CHECK(sys.Auto_timer_count == 65536UL);
    CHECK(sys.autoTimerFlag == 1 && sys.Pump_Flag == 1);
}

/******************************************************************************
 * 函数名称       : check_cycle_settings
 * 功能描述       : 从串口写入计时并验证完整秒数及零阶段的控制意图。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_cycle_settings(void) //20261116-5 add
{
    static const u16 values[] = {9999, 9999, 9999, 9999, 999};
    static const u8 qualities[] = {0, 0, 0, 0, 0};
    u8 i;

    for (i = 0; i < 5; i++) {
        write_parameter_request(6 + i, values[i]);
    }
    read_parameters(6, 5, values, qualities);
    CHECK(sys.Auto_timer_count == 65536UL);
    sys.save_flag = 0;
    write_parameter_request(6, 9999);
    CHECK(sys.save_flag == 0 && sys.Auto_timer_count == 65536UL);
    sys.Auto_timer_count = 10116UL;
    RELAY_Auto_Control();
    CHECK(sys.Pump_Flag == 1);
    sys.Auto_timer_count = 599938UL;
    Timer_mode();
    RELAY_Auto_Control();
    CHECK(sys.Pump_Flag == 1 && sys.Auto_timer_count == 599939UL);
    Timer_mode();
    RELAY_Auto_Control();
    CHECK(sys.Pump_Flag == 0 && sys.autoTimerFlag == 0 && sys.Auto_timer_count == 0);
    sys.Auto_timer_count = 599939UL;
    Timer_mode();
    RELAY_Auto_Control();
    CHECK(sys.Pump_Flag == 1 && sys.autoTimerFlag == 1 && sys.Auto_timer_count == 0);

    write_parameter_request(7, 0);
    sys.Auto_timer_count = 599939UL;
    Timer_mode();
    RELAY_Auto_Control();
    CHECK(sys.Pump_Flag == 1 && sys.autoTimerFlag == 1 && sys.Auto_timer_count == 0);

    sys.Auto_timer_count = 12;
    write_parameter_request(6, 0);
    CHECK(sys.Auto_timer_count == 12);
    Timer_mode();
    RELAY_Auto_Control();
    CHECK(sys.Pump_Flag == 0 && sys.autoTimerFlag == 1 && sys.Auto_timer_count == 12);
    Sys_Control();
    CHECK(sys.Pump_A_ON == 0 && sys.Pump_B_ON == 0 && config.Hand_Auto_timer_Flag == RUNMODE_TIMER);
    write_parameter_request(7, 1);
    RELAY_Auto_Control();
    Sys_Control();
    CHECK(sys.Pump_Flag == 0 && sys.autoTimerFlag == 0 && sys.Auto_timer_count == 0);
    sys.Auto_timer_count = 59;
    Timer_mode();
    RELAY_Auto_Control();
    Sys_Control();
    CHECK(sys.Pump_A_ON == 0 && sys.Pump_B_ON == 0 && sys.autoTimerFlag == 0);
    write_parameter_request(6, 1);
    sys.Auto_timer_count = 59;
    Timer_mode();
    RELAY_Auto_Control();
    CHECK(sys.Pump_Flag == 1 && sys.autoTimerFlag == 1);
}

/******************************************************************************
 * 函数名称       : check_recovery_timers
 * 功能描述       : 连续推进四条故障恢复计时以及两路防锈计时。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_recovery_timers(void) //20261116-5 add
{
    u32 i;
    const u32 pump_a_faults = ((u32)1 << ERR_STANDBY1) | ((u32)1 << ERR_OVERLOAD1);
    const u32 pump_b_faults = ((u32)1 << ERR_STANDBY2) | ((u32)1 << ERR_OVERLOAD2);
    const u32 all_faults = pump_a_faults | (TEST_PUMPS == 2 ? pump_b_faults : 0);

    sys.Fault_Error = all_faults;
    write_parameter_request(8, 0);
    write_parameter_request(9, 0);
    Fault_ReStart();
    CHECK(sys.Fault_Error == all_faults);
    write_parameter_request(8, 9999);
    write_parameter_request(9, 9999);
    Fault_ReStart();                   // 故障当秒执行原恢复检查。
    for (i = 1; i < 599940UL; i++) {
        Fault_ReStart();
        CHECK(sys.Fault_Error == all_faults);
    }
    Fault_ReStart();
    CHECK(sys.Fault_Error == 0);
    sys.Fault_Error = (u32)1 << ERR_OVERLOAD1;
    sys.Contact_Overload_A_Flag = 1;
#if TEST_PUMPS == 2
    sys.Fault_Error |= (u32)1 << ERR_OVERLOAD2;
    sys.Contact_Overload_B_Flag = 1;
#endif
    write_parameter_request(9, 1);
    for (i = 0; i <= 60; i++) {
        Fault_ReStart();
    }
    CHECK(sys.Fault_Error & ((u32)1 << ERR_OVERLOAD1));
    sys.Contact_Overload_A_Flag = 0;
#if TEST_PUMPS == 2
    CHECK(sys.Fault_Error & ((u32)1 << ERR_OVERLOAD2));
    sys.Contact_Overload_B_Flag = 0;
#endif

    sys.Pump_A_state = sys.Pump_B_state = 0;
    sys.Fang_Xiu_count_A = sys.Fang_Xiu_count_B = 9575;
    FanXiu_pro();
    CHECK(sys.FangXiuAFlag == 0 && sys.FangXiuBFlag == 0);
    for (i = 9576; i < 599399UL; i++) {
        FanXiu_pro();
        CHECK(sys.FangXiuAFlag == 0 && sys.FangXiuBFlag == 0);
    }
    FanXiu_pro();
    CHECK(sys.FangXiuAFlag == 1 && sys.FangXiuBFlag == 1);
    write_parameter_request(10, 0);
    FanXiu_pro();
    CHECK(sys.FangXiuAFlag == 0 && sys.FangXiuBFlag == 0);
    CHECK(sys.Fault_Error & ((u32)1 << ERR_OVERLOAD1));
}

/******************************************************************************
 * 函数名称       : check_sensor_width
 * 功能描述       : 使用满量程模拟量验证源端换算及串口原值。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_sensor_width(void) //20261116-6 add
{
    static const u32 expected[] = {99990};
    static const u8 valid[] = {0};

    config.Sensor_Rang = 9999;
    config_loaded = 1;
    updatSysdata();
    config.Auto_Mode = sys.Auto_Mode = SENSORTYPE_PRESS0_5V;
    sensor_adc_fresh = 1;
    sys.Vol_AD = 1023;
    shuiwei_check();
    CHECK(sys.Sensor_Rang == 99990UL);
    read_runtime(16, 1, expected, valid);
    write_parameter_request(13, CTROLTYPE_DRAIN);
    write_parameter_request(16, 9000);
    config.Hand_Auto_timer_Flag = RUNMODE_AUTO;
    sys.shang_DIR = 0;
    sys.Pump_Flag = 0;
    shuiwei_check();
    RELAY_Auto_Control();
    CHECK(sys.Sensor_ON1 == 90000UL && sys.Pump_Flag == 1);
    sys.Pump_Flag = 0;
}

/******************************************************************************
 * 函数名称       : check_sensor_parameter_changes
 * 功能描述       : 验证量程、方向、类型和阈值修改后的数据恢复。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_sensor_parameter_changes(void) //20261116-6 add
{
    static const u32 zeros[] = {0, 0};
    static const u8 pending[] = {2, 2};
    static const u8 valid[] = {0};
    static const u8 anomalies[] = {3};
    static const u32 scaled[] = {10000};
    static const u32 max_current[] = {10013};
    static const u16 old_stop[] = {900};
    u8 request[] = {1, 0x43, 0xbf, 6, 3, 0x10, 0, 11, 3, 17, 0, 1, 0, 0};

    write_parameter_request(13, CTROLTYPE_PUMP);
    shuiwei_check();
    RELAY_Auto_Control();
    water_status_ready = Boot_Flag = 1;
    sys.xia_shuiwei = 0;
    write_parameter_request(14, 1000);
    read_runtime(16, 1, zeros, pending);
    read_runtime(0x0f, 1, zeros, valid);
    shuiwei_check();
    RELAY_Auto_Control();
    read_runtime(16, 1, scaled, valid);

    write_parameter_request(15, 900);
    write_parameter_request(16, 800);
    if (TEST_PUMPS == 2) {
        write_parameter_request(17, 700);
    } else {
        check_error(request, sizeof(request), 6, 17);
    }
    write_parameter_request(13, 1);
    CHECK(config.Sensor_STOP == 900 && config.Sensor_ON1 == 800);
    read_runtime(16, 1, scaled, valid);
    read_runtime(21, 1, zeros, pending);
    read_runtime(8, 1, zeros, pending);
    shuiwei_check();
    RELAY_Auto_Control();
    CHECK(sys.SensorLever == 2);

    write_parameter_request(18, SENSORTYPE_WATER4_20MA);
    read_runtime(18, 1, zeros, pending);
    sys.Auto_Mode = config.Auto_Mode;       // 模拟正常扫描开始时更新运行类型。
    shuiwei_check();
    RELAY_Auto_Control();
    read_runtime(16, 1, zeros, pending);
    CHECK(!sensor_adc_fresh);
    sys.Cur_AD = 983;
    sensor_adc_fresh = 1;                  // 模拟下一批采样发布。
    shuiwei_check();
    RELAY_Auto_Control();
    read_runtime(16, 1, max_current, valid);

    write_parameter_request(20, 1);
    CHECK(sys.Sensor_OverFlow == 1);
    sys.Cur_AD = 99;
    shuiwei_check();
    read_runtime(16, 1, zeros, anomalies);
    write_parameter_request(20, 0);
    read_runtime(17, 1, zeros, pending);
    sys.Cur_AD = 100;
    shuiwei_check();
    RELAY_Auto_Control();
    CHECK(!sensor_alarm_pending && sys.Sensor_OverFlow == 0);

    write_parameter_request(14, 0);
    read_parameters(15, 1, old_stop, anomalies);
    request[9] = 15;
    check_error_range(request, sizeof(request), 7, 15, 0, 0);
    write_parameter_request(15, 0);
}

/******************************************************************************
 * 函数名称       : query_address
 * 功能描述       : 通过设备信息应答验证地址是否已生效。
 * 输入参数       : address，请求地址；active，是否应有应答。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void query_address(u8 address, u8 active) //20261116-7 add
{
    u8 query[sizeof(info_request)];
    u8 reply[27];

    memcpy(query, info_request, sizeof(query));
    query[0] = address;
    fill_crc(query, sizeof(query));
    send_request(query, sizeof(query));
    CHECK(uart4_sending() == active);
    if (active) {
        collect_reply(reply, sizeof(reply), 0);
        CHECK(reply[0] == address && reply[5] == 0x81 && crc(reply, sizeof(reply)) == 0);
    }
}

/******************************************************************************
 * 函数名称       : check_address_change
 * 功能描述       : 验证改址的旧地址应答、新地址生效及另一路改址交错。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_address_change(void) //20261116-7 add
{
    u8 request[] = {1, 0x43, 0xbf, 6, 3, 0x10, 0x45, 0x67, 3, 11, 0, 2, 0, 0};
    u8 reply[15];
    u8 i;
    u8 writes;

    sys.save_flag = 0;
    for (i = 0; i < 200; i++) {
        save_pro();
    }
    writes = flash_writes;
    fill_crc(request, sizeof(request));
    send_request(request, sizeof(request));
    CHECK(config.Dev_Address == 2 && uart4_sending());
    save_pro();
    CHECK(flash_writes == writes);
    collect_reply(reply, sizeof(reply), 0);
    CHECK(reply[0] == 1 && reply[5] == 0x90 && reply[6] == 0x45 && reply[7] == 0x67);
    CHECK(reply[10] == 11 && reply[11] == 0 && reply[12] == 2);
    CHECK(crc(reply, sizeof(reply)) == 0);
    save_pro();
    CHECK(flash_writes == writes + 1);
    query_address(1, 0);
    query_address(2, 1);

    request[0] = 2;
    request[11] = 0;
    check_error_range(request, sizeof(request), 7, 11, 1, 254);
    request[11] = 255;
    check_error_range(request, sizeof(request), 7, 11, 1, 254);
    request[10] = 1;
    request[11] = 1;
    check_error_range(request, sizeof(request), 7, 11, 1, 254);
    CHECK(config.Dev_Address == 2 && sys.save_flag == 0);

    request[10] = 0;
    request[11] = 254;
    fill_crc(request, sizeof(request));
    send_request(request, sizeof(request));
    CHECK(config.Dev_Address == 254 && uart4_sending());
    config.Dev_Address = 13;        // 模拟另一入口在串口四应答期间再次改址。
    collect_reply(reply, sizeof(reply), 1);
    CHECK(reply[0] == 2 && reply[11] == 0 && reply[12] == 254);
    CHECK(crc(reply, sizeof(reply)) == 0);
    timer_expire();
    query_address(2, 0);
    query_address(254, 0);
    query_address(13, 1);

    request[0] = 13;
    request[11] = 1;
    fill_crc(request, sizeof(request));
    send_request(request, sizeof(request));
    collect_reply(reply, sizeof(reply), 0);
    CHECK(reply[0] == 13 && reply[12] == 1 && config.Dev_Address == 1);
    query_address(1, 1);
}

/******************************************************************************
 * 函数名称       : control_wire
 * 功能描述       : 发送远程控制并读取成功应答。
 * 输入参数       : action，动作；target，目标。
 * 返回值         : 应答中的运行状态。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u16 control_wire(u8 action, u8 target) //20261116-8 add
{
    u8 request[] = {1, 0x43, 0xbf, 6, 3, 0x11, 0x11, 0x11, 2, action, target, 0, 0};
    u8 reply[20];

    fill_crc(request, sizeof(request));
    send_request(request, sizeof(request));
    collect_reply(reply, sizeof(reply), 0);
    CHECK(reply[5] == 0x91 && reply[6] == 0x11 && reply[7] == 0x11);
    CHECK(reply[8] == 9 && reply[9] == 0 && reply[10] == action && reply[11] == target);
    CHECK(crc(reply, sizeof(reply)) == 0);
    control_faults = (u32)reply[14] << 24 | (u32)reply[15] << 16 | (u32)reply[16] << 8 | reply[17];
    return (u16)reply[12] << 8 | reply[13];
}

/******************************************************************************
 * 函数名称       : check_manual_start
 * 功能描述       : 验证手动开停只设置选定泵的控制意图。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_manual_start(void) //20261116-8 add
{
    config.Hand_Auto_timer_Flag = RUNMODE_MANUAL;
    config.Lock1 = 0;
    sys.Pump_A_ON = 0;
    sys.Fault_A_Flag = 0;
    sys.Contact_Overload_A_Flag = 0;
    sys.Pump_Add_Flag = 0;
    sys.Fault_Error = 0;

    CHECK((control_wire(1, 1) & 1) == 1);
    CHECK(sys.Pump_A_ON == 1);
    CHECK((control_wire(2, 1) & 1) == 1);
    CHECK(sys.Pump_A_ON == 0 && config.Hand_Auto_timer_Flag == RUNMODE_MANUAL);
}

/******************************************************************************
 * 函数名称       : check_control_limits
 * 功能描述       : 验证远程开停的模式、目标、锁定和重复操作。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_control_limits(void) //20261116-8 add
{
    u8 request[] = {1, 0x43, 0xbf, 6, 3, 0x11, 0x11, 0x11, 2, 1, 1, 0, 0};

    config.Hand_Auto_timer_Flag = RUNMODE_AUTO;
    sys.Pump_A_ON = 1;
    check_error(request, sizeof(request), 8, 1);
    request[9] = 2;
    check_error(request, sizeof(request), 8, 2);
    CHECK(sys.Pump_A_ON == 1 && config.Hand_Auto_timer_Flag == RUNMODE_AUTO);

    sys.save_flag = 0;
    CHECK((control_wire(3, 0) & 0x0f) == 1);
    CHECK(sys.Pump_A_ON == 1 && sys.save_flag == 1);
    sys.save_flag = 0;
    control_wire(3, 0);
    CHECK(sys.save_flag == 0);
    control_wire(2, 3);

    request[9] = 1;
    request[10] = 3;
    config.Lock1 = 1;
    check_error(request, sizeof(request), 9, 1);
    CHECK(sys.Pump_A_ON == 0 && sys.Pump_B_ON == 0);
    config.Lock1 = 0;
    if (TEST_PUMPS == 2) {
        config.Lock2 = 1;
        check_error(request, sizeof(request), 9, 1);
        CHECK(sys.Pump_A_ON == 0 && sys.Pump_B_ON == 0);
        config.Lock2 = 0;
    } else {
        request[10] = 2;
        check_error(request, sizeof(request), 6, 1);
    }
    control_wire(1, 3);
    CHECK(sys.Pump_A_ON == 1 && sys.Pump_B_ON == (TEST_PUMPS == 2));
    control_wire(2, 3);

    sys.Auto_timer_count = 500;
    CHECK((control_wire(5, 0) & 0x0f) == 10);
    CHECK(sys.autoTimerFlag == 1 && sys.Auto_timer_count == 0 && sys.save_flag == 1);
    sys.save_flag = 0;
    sys.Auto_timer_count = 50;
    control_wire(5, 0);
    CHECK(sys.Auto_timer_count == 50 && sys.save_flag == 0);
    CHECK((control_wire(4, 0) & 0x0f) == 6);
    CHECK(sys.save_flag == 1);
    control_wire(3, 0);

    request[9] = 7;
    check_error(request, sizeof(request), 4, 7);
    request[9] = 3;
    check_error(request, sizeof(request), 4, 3);
    request[9] = 1;
    request[10] = 0;
    check_error(request, sizeof(request), 4, 1);
    request[10] = 1;
    config_loaded = 0;
    check_error(request, sizeof(request), 0x0e, 1);
    config_loaded = 1;
}

/******************************************************************************
 * 函数名称       : check_control_contact
 * 功能描述       : 验证触点持续接通和断开后显式清除的原保护行为。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_control_contact(void) //20261116-8 add
{
    config_t before;
    u32 expected = (u32)1 << ERR_OVERLOAD1;
    u8 request[] = {1, 0x43, 0xbf, 6, 3, 0x11, 0x11, 0x11, 2, 1, 3, 0, 0};

    config.Hand_Auto_timer_Flag = RUNMODE_MANUAL;
    memcpy(&before, &config, sizeof(before));
    sys.Pump_Add_Flag = 1;
    if (TEST_PUMPS == 2) {
        sys.Flag_OverFlow = 1;
        expected |= (u32)1 << ERR_OVERLOAD2;
    }
    Contact_Overload_Control();
    CHECK(sys.Fault_Error == expected);
    check_error(request, sizeof(request), 9, 1);
    control_wire(6, 0);
    CHECK(control_faults == expected && sys.Fault_Error == expected);
    CHECK(RELAY1 == 0 && RELAY2 == 0);

    sys.Pump_Add_Flag = sys.Flag_OverFlow = 0;
    Contact_Overload_Control();
    CHECK(sys.Fault_Error == expected);
    control_wire(6, 0);
    CHECK(control_faults == 0 && sys.Fault_Error == 0);
    CHECK(memcmp(&config, &before, sizeof(config)) == 0);
    control_wire(1, 3);             // 相同关联号按当前已清除的条件重新受理。
    CHECK(sys.Pump_A_ON == 1);
    control_wire(2, 3);
}

/******************************************************************************
 * 函数名称       : check_display_digits
 * 功能描述       : 检查连续量显示的位数、小数点、边界和无效提示。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_display_digits(void) //20261116-9 add
{
    sensor_sample_ready = 1;
    config.Auto_Mode = sys.Auto_Mode = SENSORTYPE_WATER4_20MA;
    sys.sensor_err = 0;
    sys.SensorOut = 12345;
    lcd_sensor_probe();
    CHECK(NUM5.All == LED_LIST[1] && NUM6.All == LED_LIST[2]);
    CHECK(NUM7.All == LED_LIST[3] && NUM8.All == LED_LIST[4]);
    CHECK(!LCD_P3 && LCD_COL);

    sys.SensorOut = 0;
    lcd_sensor_probe();
    CHECK(NUM5.All == 0 && NUM6.All == LED_LIST[0]);
    CHECK(NUM7.All == LED_LIST[0] && NUM8.All == LED_LIST[0]);
    CHECK(LCD_P3 && !LCD_COL);

    sys.SensorOut = 9999;
    lcd_sensor_probe();
    CHECK(NUM5.All == LED_LIST[9] && NUM8.All == LED_LIST[9] && LCD_P3 && !LCD_COL);

    sys.SensorOut = 10000;
    lcd_sensor_probe();
    CHECK(NUM5.All == LED_LIST[1] && NUM6.All == LED_LIST[0]);
    CHECK(NUM7.All == LED_LIST[0] && NUM8.All == LED_LIST[0] && !LCD_P3 && LCD_COL);

    sys.SensorOut = 99999;
    lcd_sensor_probe();
    CHECK(NUM5.All == LED_LIST[9] && NUM8.All == LED_LIST[9] && !LCD_P3 && LCD_COL);
    CHECK(sys.SensorOut == 99999UL);

    sys.SensorOut = 100000;
    lcd_sensor_probe();
    CHECK(NUM5.All == LED_LIST[11] && NUM6.All == LED_LIST[11]);
    CHECK(NUM7.All == LED_LIST[11] && NUM8.All == LED_LIST[11] && !LCD_P3 && !LCD_COL);

    sys.SensorOut = 5000;
    sys.sensor_err = 1;
    lcd_sensor_probe();
    CHECK(NUM5.All == LED_LIST[11] && NUM8.All == LED_LIST[11] && !LCD_P3 && !LCD_COL);
    sys.sensor_err = 0;
    sensor_sample_ready = 0;
    lcd_sensor_probe();
    CHECK(NUM5.All == LED_LIST[11] && NUM8.All == LED_LIST[11] && !LCD_P3 && !LCD_COL);
}

/******************************************************************************
 * 函数名称       : main
 * 功能描述       : 从串口入口验证设备信息、报文破坏和静默恢复。
 * 输入参数       : None
 * 返回值         : 测试退出码。
 * 日期            : 2026.11.16
 ******************************************************************************/
int main(void) //20261116-0 add
{
    u8 bad[98];
    u8 i;

    config.Dev_Address = 1;
    config_loaded = 1;
    uart_init();
    CHECK((S4CON & 0xf0) == 0x50);
    CHECK((P_SW2 & 4) != 0);
    CHECK(((u16)TH4 << 8 | TL4) == 65536UL - MAIN_Fosc / 4 / 9600);
    check_info(0);
    check_info(0);                      // 相同关联号仍正常应答。
    check_info(1);
    send_request(info_request, sizeof(info_request)); // 发送期输入后的尾部仍丢弃。
    CHECK(!uart4_sending());
    check_info(0);

    memcpy(bad, info_request, sizeof(info_request));
    bad[10] ^= 1;
    send_request(bad, sizeof(info_request));
    CHECK(!uart4_sending());
    send_request(info_request, sizeof(info_request) - 1);
    CHECK(!uart4_sending());
    bad[0] = 2;
    fill_crc(bad, sizeof(info_request));
    send_request(bad, sizeof(info_request));
    CHECK(!uart4_sending());

    memcpy(bad, info_request, sizeof(info_request));
    for (i = sizeof(info_request); i < sizeof(bad); i++) {
        bad[i] = 0;
    }
    send_request(bad, sizeof(bad));     // 超长帧拥有合法前缀也不能应答。
    CHECK(!uart4_sending());
    check_info(0);

    memcpy(bad, info_request, sizeof(info_request));
    bad[4] = 2;
    check_error(bad, sizeof(info_request), 2, 0xff);
    bad[8] = 1;
    fill_crc(bad, sizeof(info_request));
    send_request(bad, sizeof(info_request));
    CHECK(!uart4_sending());
    bad[4] = 3;
    check_error(bad, sizeof(info_request), 3, 0xff);
    bad[9] = 0x42;
    check_error(bad, 12, 3, 0xff);
    bad[8] = 0;
    bad[7] = 0;
    check_error(bad, sizeof(info_request), 3, 0xff);
    bad[6] = 0x12;
    bad[7] = 0x34;
    bad[5] = 0x35;
    check_error(bad, sizeof(info_request), 1, 0xff);
    bad[5] = 0x81;
    fill_crc(bad, sizeof(info_request));
    send_request(bad, sizeof(info_request));
    CHECK(!uart4_sending());
    check_info(0);

    check_one_parameter();
    check_parameter_ranges();
    check_one_runtime_field();
    check_runtime_fields();
    check_sensor_sample();
    check_history();
    check_parameter_writes();
    check_parameter_save();
    check_cycle_counter();
    check_cycle_settings();
    check_recovery_timers();
    check_sensor_width();
    check_sensor_parameter_changes();
    check_address_change();
    check_manual_start();
    check_control_limits();
    check_control_contact();
    check_display_digits();
    puts("BF4G_UART_TEST=PASS");
    return 0;
}
