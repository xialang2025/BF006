#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uart4.h"
#include "configPar.h"
#include "ad.h"
#include "Display.h"
#include "HT1622.h"

u8 S4CON, S4BUF, ACC, IE2, P_SW2, T4T3M, TH4, TL4, TH3, TL3, AUXINTIF, EA;
config_t config;
bit config_loaded;
sys_t sys;
bit Boot_Flag;
u8 LowWater_M, LowWater_H, HighWater_M, HighWater_H, PUMP_ADD, OverFlow_P, P34;
volatile bit voltage_sample_ready, current_sample_ready;
u8 Dis7_Flash, Dis8_Flash;
u8 LCD_WATER_Hlevel1, LCD_WATER_Hlevel2, LCD_WATER_Hlevel3,
   LCD_WATER_Hlevel4, LCD_WATER_Hlevel5, LCD_WATER_Hlevel6;
u8 LCD_WATER_Llevel1, LCD_WATER_Llevel2, LCD_WATER_Llevel3,
   LCD_WATER_Llevel4, LCD_WATER_Llevel5, LCD_WATER_Llevel6;

void UART4_int(void);
void UART4_Timer3_int(void);
void shuiwei_check(void);

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
        TEST_VOLTS & 0xff, 0x00, 0x05, 0x61, 0x10, 0x09, 0x08
    };

    send_request(info_request, sizeof(info_request));
    collect_reply(reply, sizeof(reply), interfere);
    CHECK(memcmp(reply, expected, sizeof(expected)) == 0);
    CHECK(crc(reply, sizeof(reply)) == 0);
}

/******************************************************************************
 * 函数名称       : check_error
 * 功能描述       : 核对错误应答合同和请求回显。
 * 输入参数       : request，请求；length，长度；status，预期错误码；object，预期目标。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_error(u8 *request, u8 length, u8 status, u8 object) //20261116-1 change
{
    u8 reply[21];
    u8 i;

    fill_crc(request, length);
    send_request(request, length);
    collect_reply(reply, sizeof(reply), 0);
    CHECK(memcmp(reply, request, 4) == 0);
    CHECK(reply[4] == 3 && reply[5] == (request[5] | 0x80));
    CHECK(reply[6] == request[6] && reply[7] == request[7]);
    CHECK(reply[8] == 10 && reply[9] == status && reply[10] == object);
    for (i = 11; i < 19; i++) {
        CHECK(reply[i] == 0);
    }
    CHECK(crc(reply, sizeof(reply)) == 0);
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
    current_sample_ready = 0;
    sys.Cur_AD = 99;
    shuiwei_check();
    read_runtime(16, 1, zeros, unloaded);

    current_sample_ready = 1;
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
    puts("BF4G_UART_TEST=PASS");
    return 0;
}
