#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uart4.h"
#include "configPar.h"
#include "eeprom.h" //20261124-6 add 使用实际设备的台架初始化开关。
#include "ad.h"
#include "Display.h"
#include "HT1622.h"

u8 S4CON, S4BUF, ACC, IE2, P_SW2, T4T3M, TH4, TL4, TH3, TL3, AUXINTIF, EA;
u16 Limit_Current;
const u16 Value_OVP = 480;
static u8 flash[117];
static u8 flash_writes;
static u8 boot_flash[2][512]; //20261124-6 change 模拟编号扇区全长和末端已有数据。
#if BF4G_BENCH_BOOT_INIT
static bit boot_probe_available = 1;
#endif
static u8 boot_writes;
static u8 boot_write_limit = 12; //20261124-1 add 模拟断电时只写完记录的一部分。
static u32 control_faults;
static u8 simulate_rf;
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
void sys_init(void);
void Water_Level(void);
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
    if (address == 0x0200 || address == 0x0400) { //20261124-1 add 编号读入与参数区独立。
        CHECK(length == 12);
        memcpy(out, boot_flash[address == 0x0400], length);
        return;
    }
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
    if (address == 0x0200 || address == 0x0400) { //20261124-1 add 模拟单个编号扇区被擦除。
        memset(boot_flash[address == 0x0400], 0xff, sizeof(boot_flash[0]));
        return;
    }
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
    if (address == 0x0200 || address == 0x0400) { //20261124-1 add 写入前须已经擦除编号扇区。
        u8 *slot = boot_flash[address == 0x0400];
        u8 i;

        CHECK(length == 12);
        for (i = 0; i < length; i++) {
            CHECK(slot[i] == 0xff);
        }
        memcpy(slot, source, boot_write_limit);
        boot_writes++;
        return;
    }
    CHECK(address == 0 && length == sizeof(flash));
    CHECK(flash[0] == 0xff && flash[sizeof(flash) - 1] == 0xff);
    memcpy(flash, source, length);
    flash_writes++;
}

#if BF4G_BENCH_BOOT_INIT
/******************************************************************************
 * 函数名称       : EEPROM_is_erased
 * 功能描述       : 模拟读取编号扇区和数据区不可用。
 * 输入参数       : address，扇区地址；length，检查长度。
 * 返回值         : 区间可读且为擦除值时为一。
 * 日期            : 2026.11.24
 ******************************************************************************/
bit EEPROM_is_erased(u16 address, u16 length) //20261124-6 add
{
    const u8 *slot;
    u16 i;

    CHECK((address == 0x0200 || address == 0x0400) && length == 512);
    if (!boot_probe_available) {
        return 0;
    }
    slot = boot_flash[address == 0x0400];
    for (i = 0; i < length; i++) {
        if (slot[i] != 0xff) {
            return 0;
        }
    }
    return 1;
}
#endif

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
        CHECK(uart4_sending() && !(T4T3M & 0x08));
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
        0x00, 0x02, TEST_MODEL_BYTE, 0x93, 0x00, 0xc9, TEST_PHASES, TEST_PUMPS, TEST_VOLTS >> 8, //20261124-5 change
        TEST_VOLTS & 0xff, 0x00, 0x3f, 0x61, 0x10, 0x09, 0x08
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
 * 函数名称       : scan_control
 * 功能描述       : 模拟主循环执行保护、泵状态更新和串口结果确认。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.17
 ******************************************************************************/
static void scan_control(void) //20261117-7 add
{
    Contact_Overload_Control();
    RELAY_Auto_Control();
    Sys_Control();
    uart4_control_scanned();
}

/******************************************************************************
 * 函数名称       : control_wire
 * 功能描述       : 发送远程控制并读取成功应答。
 * 输入参数       : action，动作；target，目标。
 * 返回值         : 应答中的运行状态。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u16 control_wire(u8 action, u8 target) //20261117-7 change
{
    u8 request[] = {1, 0x43, 0xbf, 6, 3, 0x11, 0x11, 0x11, 2, action, target, 0, 0};
    u8 reply[20];

    fill_crc(request, sizeof(request));
    send_request(request, sizeof(request));
    CHECK(!uart4_sending());
    scan_control();
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
static void check_manual_start(void) //20261117-7 change
{
    u8 request[] = {1, 0x43, 0xbf, 6, 3, 0x11, 0x12, 0x34, 2, 1, 1, 0, 0};
    u8 reply[20];

    config.Hand_Auto_timer_Flag = RUNMODE_MANUAL;
    config.Lock1 = 0;
    sys.Pump_A_ON = 0;
    sys.Fault_A_Flag = 0;
    sys.Contact_Overload_A_Flag = 0;
    sys.Pump_Add_Flag = 0;
    sys.Fault_Error = 0;

    sys.Pump_A_state = 0;
    fill_crc(request, sizeof(request));
    send_request(request, sizeof(request));
    CHECK(!uart4_sending());
    scan_control();
    collect_reply(reply, sizeof(reply), 0);
    CHECK(reply[5] == 0x91 && reply[6] == 0x12 && reply[7] == 0x34);
    CHECK(reply[9] == 0 && (reply[13] & 0x10) != 0 && crc(reply, sizeof(reply)) == 0);
    CHECK(sys.Pump_A_ON == 1);
    CHECK((control_wire(2, 1) & 0x1f) == 1);
    CHECK(sys.Pump_A_ON == 0 && config.Hand_Auto_timer_Flag == RUNMODE_MANUAL);
    CHECK((control_wire(2, 1) & 0x1f) == 1); // 已达到目标仍正常确认一次。
    if (TEST_PUMPS == 2) {
        CHECK((control_wire(1, 2) & 0x30) == 0x20);
        CHECK((control_wire(2, 2) & 0x30) == 0);
    }
}

/******************************************************************************
 * 函数名称       : check_control_deadline
 * 功能描述       : 验证防锈输出阻止停泵确认时的失败帧、收帧边界和保护扫描。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.17
 ******************************************************************************/
static void check_control_deadline(void) //20261117-7 add
{
    u8 request[] = {1, 0x43, 0xbf, 6, 3, 0x11, 0x12, 0x35, 2, 2, 1, 0, 0};
    u8 reply[21];
    u8 i;
    u8 saved_flag = sys.save_flag;
    u8 writes = flash_writes;

    sys.save_flag = 0;
    for (i = 0; i < 200; i++) {
        save_pro();                  // 排除上次擦写冷却时间对本次断言的影响。
    }
    sys.FangXiuAFlag = 1;
    scan_control();
    fill_crc(request, sizeof(request));
    send_request(request, sizeof(request));
    CHECK(!uart4_sending());
    sys.save_flag = 1;
    save_pro();
    CHECK(sys.save_flag == 1 && flash_writes == writes);
    sys.save_flag = saved_flag;

    for (i = 0; i < sizeof(info_request); i++) {
        receive_byte(info_request[i]); // 等待期间的新请求不能替换原关联号。
    }
    for (i = 1; i < 30; i++) {
        timer_expire();
        if (i % 10 == 0) {
            scan_control();
        }
        uart4_service();
        CHECK(!uart4_sending());
    }
    timer_expire();
    uart4_service();
    collect_reply(reply, sizeof(reply), 0);
    CHECK(reply[0] == 1 && reply[5] == 0x91 && reply[6] == 0x12 && reply[7] == 0x35);
    CHECK(reply[8] == 10 && reply[9] == 0x0a && reply[10] == 2);
    CHECK(memcmp(reply + 11, "\0\0\0\0\0\0\0\0", 8) == 0 && crc(reply, sizeof(reply)) == 0);
    timer_expire();                 // 额外数据须经过新的完整静默再接收。
    uart4_service();
    CHECK(!uart4_sending());
    sys.FangXiuAFlag = 0;
    CHECK((control_wire(2, 1) & 0x30) == 0);

    request[9] = 4;
    request[10] = 0;
    fill_crc(request, sizeof(request));
    send_request(request, sizeof(request));
    CHECK(!uart4_sending());
    config.Hand_Auto_timer_Flag = RUNMODE_MANUAL; // 模拟本地入口抢先切换模式。
    scan_control();
    collect_reply(reply, sizeof(reply), 0);
    CHECK(reply[5] == 0x91 && reply[6] == 0x12 && reply[7] == 0x35);
    CHECK(reply[9] == 0x0a && reply[10] == 4 && crc(reply, sizeof(reply)) == 0);
    check_info(0);
}

/******************************************************************************
 * 函数名称       : check_control_limits
 * 功能描述       : 验证远程开停的模式、目标、锁定和重复操作。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_control_limits(void) //20261117-7 change
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
    CHECK((control_wire(1, 3) & 0x30) == (TEST_PUMPS == 2 ? 0x30 : 0x10));
    CHECK(sys.Pump_A_ON == 1 && sys.Pump_B_ON == (TEST_PUMPS == 2));
    CHECK((control_wire(2, 3) & 0x30) == 0);

    config.Auto_Mode = sys.Auto_Mode = SENSORTYPE_POLE;
    config.Sensor_Mode = CTROLTYPE_DRAIN;
    config.Pump_AB_Mode = 0;
    sys.shang_DIR = sys.xia_DIR = sys.sensor_err = 0;
    config.AUTO_Timing_Run = config.AUTO_Timing_Stop = 2;
    sys.Auto_timer_count = 500;
    CHECK((control_wire(5, 0) & 0x3f) == 0x1a);
    CHECK(sys.autoTimerFlag == 1 && sys.Auto_timer_count == 0 && sys.save_flag == 1);
    sys.save_flag = 0;
    sys.Auto_timer_count = 50;
    control_wire(5, 0);
    CHECK(sys.Auto_timer_count == 50 && sys.save_flag == 0);
    CHECK((control_wire(4, 0) & 0x3f) == 6);
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
    Boot_Flag = 0;
    check_error(request, sizeof(request), 0x0e, 1);
    Boot_Flag = 1;
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
 * 函数名称       : check_main_summary_length
 * 功能描述       : 从串口接收完整主控综合应答。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_main_summary_length(void) //20261116-10 add
{
    u8 request[] = {1, 0x43, 0xbf, 6, 3, 5, 0, 12, 0, 0, 0};
    u8 reply[74];

    fill_crc(request, sizeof(request));
    send_request(request, sizeof(request));
    collect_reply(reply, sizeof(reply), 0);
    CHECK(reply[5] == 0x85 && reply[8] == 63 && reply[9] == 0);
    CHECK(crc(reply, sizeof(reply)) == 0);
}

/******************************************************************************
 * 函数名称       : dashboard_fixture
 * 功能描述       : 设置独立主控样本，覆盖有效、未就绪、不适用及异常。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void dashboard_fixture(void) //20261116-10 add
{
    config_loaded = 1;
    water_status_ready = Boot_Flag = sensor_sample_ready = 1;
    sensor_pending_tanks = 0;
    sensor_alarm_pending = 0;
    current_sample_ready = 1;
    voltage_sample_ready = 0;
    sys.Voltage_Line = 220;
    sys.Current_A1 = 12;
#if TEST_PHASES == 3
    sys.Current_B1 = 34;
    sys.Current_C1 = 56;
#endif
    config.Hand_Auto_timer_Flag = RUNMODE_AUTO;
    config.Pump_AB_Mode = 1;
    sys.Pump_A_state = sys.Pump_B_state = 1;
    sys.Fault_Error = 0x80000001UL;
    sys.shang_shuiwei = 2;
    sys.xia_shuiwei = 0;
    LCD_WATER_Hlevel1 = LCD_WATER_Hlevel6 = 1;
    LCD_WATER_Llevel2 = LCD_WATER_Llevel4 = 1;
    Dis7_Flash = Dis8_Flash = 0;
    sys.SensorOut = 99990UL;
    sys.SensorLever = 2;
    sys.sensor_err = sys.flowover = 0;
    sys.Sensor_OverFlow = 1;
    config.Auto_Mode = sys.Auto_Mode = SENSORTYPE_WATER4_20MA;
    config.Sensor_Mode = CTROLTYPE_DRAIN;
    config.Sensor_Rang = 9999;
    config.Sensor_ON1 = 9000;
    config.Sensor_STOP = 5000;
    config.Sensor_overFlow = 9998;
    config.OVP_Value = 500;
    config.UVP_Value = 123;
    config.Set_OVERLOAD_Current1 = 888;
    config.Set_STANDBY_Current1 = 77;
    config.AUTO_Timing_Run = 6;
    config.AUTO_Timing_Stop = 7;
}

/******************************************************************************
 * 函数名称       : check_main_summary_fields
 * 功能描述       : 独立解码主控综合应答的全部项目和保留区。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_main_summary_fields(void) //20261116-10 add
{
    static const u8 widths[] = {
        2, 2, 2, 2, 2, 4, 1, 1, 1, 1, 4, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2
    };
    static const u8 qualities[] = {
        2, 0, TEST_PHASES == 3 ? 0 : 1, TEST_PHASES == 3 ? 0 : 1,
        0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0
    };
    static const u32 values[] = {
        0, 12, TEST_PHASES == 3 ? 34 : 0, TEST_PHASES == 3 ? 56 : 0,
        TEST_PUMPS == 2 ? 0x18b6 : 0x1816, 0x80000001UL, 0x21, 0x0a, 2, 0,
        99990UL, 10, 2, 1, 9999, 9000, 5000, 9998, 500, 123, 888, 77, 6, 7
    };
    u8 request[] = {1, 0x43, 0xbf, 6, 3, 5, 0x7e, 0xab, 0, 0, 0, 0};
    u8 reply[74];
    u8 i;
    u8 j;
    u8 offset = 17;
    u32 value;
    u8 features = (TEST_PHASES == 3 ? 1 : 0) | (TEST_PUMPS == 2 ? 2 : 0) |
                  (TEST_VOLTS == 380 ? 4 : 0);

    dashboard_fixture();
    fill_crc(request, sizeof(request) - 1);
    send_request(request, sizeof(request) - 1);
    CHECK(uart4_sending());
    sys.Fault_Error = 0;                 // 应答启动后修改业务状态，冻结的帧不变化。
    collect_reply(reply, sizeof(reply), 1);
    CHECK(reply[5] == 0x85 && reply[6] == 0x7e && reply[7] == 0xab);
    CHECK(reply[8] == 63 && reply[9] == 0 && reply[10] == features);
    for (i = 0; i < 24; i++) {
        CHECK(((reply[11 + i / 4] >> (2 * (i % 4))) & 3) == qualities[i]);
        value = 0;
        for (j = 0; j < widths[i]; j++) {
            value = (value << 8) | reply[offset++];
        }
        CHECK(value == values[i]);
    }
    CHECK(offset == 62);
    for (i = 62; i < 72; i++) {
        CHECK(reply[i] == 0);
    }
    CHECK(crc(reply, sizeof(reply)) == 0);
    timer_expire();
    request[8] = 1;
    request[9] = 0xa5;
    check_error(request, sizeof(request), 3, 0xff);
}

/******************************************************************************
 * 函数名称       : check_web_summary_length
 * 功能描述       : 验证串口可发送九十七字节的综合应答。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_web_summary_length(void) //20261116-11 add
{
    u8 request[] = {1, 0x43, 0xbf, 6, 3, 6, 0, 13, 0, 0, 0};
    u8 reply[97];

    fill_crc(request, sizeof(request));
    send_request(request, sizeof(request));
    collect_reply(reply, sizeof(reply), 0);
    CHECK(reply[5] == 0x86 && reply[8] == 86 && reply[9] == 0);
    CHECK(crc(reply, sizeof(reply)) == 0);
}

/******************************************************************************
 * 函数名称       : summary_quality
 * 功能描述       : 从紧凑质量区解析某项的两位质量。
 * 输入参数       : reply，应答；item，项目号。
 * 返回值         : 读数质量。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u8 summary_quality(const u8 *reply, u8 item) //20261116-11 add
{
    return (reply[11 + item / 4] >> (2 * (item % 4))) & 3;
}

/******************************************************************************
 * 函数名称       : check_web_summary
 * 功能描述       : 独立核对网页应答补充字段、公共字段偏移和连续组帧边界。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_web_summary(void) //20261116-11 add
{
    static const u8 tail_widths[] = {2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 1};
    static const u8 tail_qualities[] = {
        TEST_PUMPS == 2 ? 0 : 1,
        TEST_PUMPS == 2 && TEST_PHASES == 3 ? 0 : 1,
        TEST_PUMPS == 2 && TEST_PHASES == 3 ? 0 : 1,
        TEST_PUMPS == 2 ? 0 : 1, TEST_PUMPS == 2 ? 3 : 1,
        TEST_PUMPS == 2 ? 0 : 1, TEST_PUMPS == 2 ? 0 : 1, 0, 0, 0, 0
    };
    static const u32 tail_values[] = {
        TEST_PUMPS == 2 ? 78 : 0,
        TEST_PUMPS == 2 && TEST_PHASES == 3 ? 90 : 0,
        TEST_PUMPS == 2 && TEST_PHASES == 3 ? 123 : 0,
        55, 901, 1, 8999, (TEST_MODEL_BYTE << 8) | 0x93, 201, 0x146, 2
    };
    u8 request[] = {1, 0x43, 0xbf, 6, 3, 5, 0x88, 0x77, 0, 0, 0};
    u8 poison[97];
    u8 brief[74];
    u8 reply[97];
    u8 i;
    u8 j;
    u8 offset = 65;
    u32 value;

    dashboard_fixture();
    sys.Current_A2 = 78;
#if TEST_PHASES == 3
    sys.Current_B2 = 90;
    sys.Current_C2 = 123;
#endif
    sys.Fault_Error = 6;
    config.Set_STANDBY_Current2 = 55;
    config.Set_OVERLOAD_Current2 = 901;
    config.Sensor_ON2 = 8999;

    fill_crc(request, sizeof(request));
    send_request(request, sizeof(request));
    collect_reply(brief, sizeof(brief), 0);

    memcpy(poison, request, 9);
    memset(poison + 9, 0xa5, 86);
    poison[5] = 0x61;
    poison[8] = 86;
    check_error(poison, sizeof(poison), 1, 0xff);
    request[5] = 6;
    fill_crc(request, sizeof(request));
    send_request(request, sizeof(request));
    collect_reply(reply, sizeof(reply), 0);
    CHECK(reply[5] == 0x86 && reply[6] == 0x88 && reply[7] == 0x77 && reply[8] == 86);
    CHECK(reply[9] == 0 && reply[10] == brief[10]);
    CHECK(memcmp(reply + 11, brief + 11, 6) == 0);
    CHECK(memcmp(reply + 20, brief + 17, 45) == 0);
    for (i = 0; i < 11; i++) {
        CHECK(summary_quality(reply, 24 + i) == tail_qualities[i]);
        value = 0;
        for (j = 0; j < tail_widths[i]; j++) {
            value = (value << 8) | reply[offset++];
        }
        CHECK(value == tail_values[i]);
    }
    CHECK(offset == 85);
    CHECK((reply[19] & 0xc0) == 0);
    for (i = 85; i < 95; i++) {
        CHECK(reply[i] == 0);
    }
    CHECK(crc(reply, sizeof(reply)) == 0);

    sys.Fault_Error = 0x90000001UL;
    config.Pump_AB_Mode = 9;
    sys.Voltage_Line = 222;
    voltage_sample_ready = 1;
    send_request(request, sizeof(request));
    collect_reply(reply, sizeof(reply), 0);
    CHECK(summary_quality(reply, 0) == 0 && reply[20] == 0 && reply[21] == 222);
    CHECK(summary_quality(reply, 5) == 3 && reply[30] == 0x90 && reply[33] == 1);
    CHECK(summary_quality(reply, 29) == (TEST_PUMPS == 2 ? 3 : 1) && reply[75] == 9);
    CHECK(summary_quality(reply, 33) == 3 && reply[82] == 0 && reply[83] == 0);
    CHECK((reply[19] & 0xc0) == 0);
    for (i = 85; i < 95; i++) {
        CHECK(reply[i] == 0);
    }
    CHECK(crc(reply, sizeof(reply)) == 0);
}

/******************************************************************************
 * 函数名称       : decode_vector
 * 功能描述       : 将定稿的十六进制文字转换为测试报文字节。
 * 输入参数       : hex，连续十六进制；out，解码位置。
 * 返回值         : 报文字节数。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u8 decode_vector(const char *hex, u8 *out) //20261116-12 add
{
    unsigned int byte;
    u8 length = 0;

    while (*hex) {
        CHECK(hex[1] != 0 && length < 97);
        CHECK(sscanf(hex, "%2x", &byte) == 1);
        out[length++] = (u8)byte;
        hex += 2;
    }
    return length;
}

/******************************************************************************
 * 函数名称       : check_vector
 * 功能描述       : 校验定稿帧并从串口入口比对应答。
 * 输入参数       : input，请求文字；output，固定应答或空；length，期望长度。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_vector(const char *input, const char *output, u8 length) //20261116-12 add
{
    u8 request[97];
    u8 expected[97];
    u8 reply[97];
    u8 request_length;

    request_length = decode_vector(input, request);
    CHECK(request_length == 11 + request[8] && crc(request, request_length) == 0);
    send_request(request, request_length);
    if (request[5] == 0x11 && !uart4_sending()) {
        scan_control();
    }
    collect_reply(reply, length, 0);
    CHECK(length == 11 + reply[8] && crc(reply, length) == 0);
    CHECK(reply[5] == (request[5] | 0x80));
    CHECK(reply[6] == request[6] && reply[7] == request[7]);
    if (output) {
        CHECK(decode_vector(output, expected) == length);
        CHECK(crc(expected, length) == 0);
        CHECK(memcmp(reply, expected, length) == 0);
    }
}

/******************************************************************************
 * 函数名称       : check_final_vectors
 * 功能描述       : 逐字节验证实施差异规范的十四个请求或应答报文。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_final_vectors(void) //20261116-12 add
{
    config.Dev_Address = 1;
    config.Set_OVERLOAD_Current1 = 100;
    config.Hand_Auto_timer_Flag = RUNMODE_MANUAL;
    sys.Fault_Error = 0;
    sys.Pump_A_state = 1;
    sys.Fault_A_Flag = 0;
    sys.Pump_Add_Flag = 0;
    sys.Contact_Overload_A_Flag = 0;
    config.Lock1 = 0;

    if (TEST_PUMPS == 2 && TEST_PHASES == 1) {
        check_vector("0143BF0603010001004984",
                     "0143BF0603810001100002A39300C9010200DC003F611009080EB3", 27); //20261124-5 change 修订与能力变更后的完整设备信息。
    }
    check_vector("0143BF06030200020203015774", "0143BF0603820002060003010000646BA2", 17);
    check_vector("0143BF0603100003030303843AC0", "0143BF060390000304000303849E64", 15);
    sys.save_flag = 0;
    check_vector("0143BF0603100003030303843AC0", "0143BF060390000304000303849E64", 15);
    CHECK(sys.save_flag == 0);
    check_vector("0143BF0603100004030303854EC0", "0143BF06039000040A0703000000000000038499C1", 21);
    if (TEST_PUMPS == 1) {
        water_status_ready = 0;
        check_vector("0143BF060311000502010155C3", "0143BF0603910005090001010011000000007B41", 20);
    }
    config.Hand_Auto_timer_Flag = RUNMODE_AUTO;
    check_vector("0143BF06031100060201015587", "0143BF06039100060A080100000000000000004904", 21);
    check_vector("0143BF0603050007004B14", NULL, 74);
    check_vector("0143BF0603060008004EA0", NULL, 97);
}

/******************************************************************************
 * 函数名称       : check_long_send_integration
 * 功能描述       : 验证最大帧发送期间本地保护与旧格式拒绝。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_long_send_integration(void) //20261116-12 add
{
    u8 summary[] = {1, 0x43, 0xbf, 6, 3, 6, 0, 1, 0, 0, 0};
    u8 legacy_write[] = {
        1, 0x43, 0xbf, 6, 2, 0x10, 0, 2, 15,
        0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 100, 3, 3, 0x84, 0, 0
    };
    u8 legacy_control[] = {
        1, 0x43, 0xbf, 6, 2, 0x11, 0, 2, 14,
        0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 100, 1, 3, 0, 0
    };
    u8 new_write[] = {1, 0x43, 0xbf, 6, 3, 0x10, 0, 2, 3, 3, 0, 80, 0, 0};
    u8 reply[97];
    u8 i;

    dashboard_fixture();
    config.Hand_Auto_timer_Flag = RUNMODE_MANUAL;
    sys.Fault_Error = 0;
    sys.Pump_A_ON = 0;
    check_error(legacy_write, sizeof(legacy_write), 2, 0xff);
    check_error(legacy_control, sizeof(legacy_control), 2, 0xff);
    CHECK(config.Set_OVERLOAD_Current1 == 888 && sys.Pump_A_ON == 0);
    fill_crc(summary, sizeof(summary));
    fill_crc(new_write, sizeof(new_write));
    send_request(summary, sizeof(summary));
    CHECK(uart4_sending());
    for (i = 0; i < sizeof(reply); i++) {
        reply[i] = S4BUF;
        if (i < sizeof(new_write)) {
            receive_byte(new_write[i]);
        }
        if (i == 45) {
            sys.Pump_Add_Flag = 1;
            RELAY1 = 1;
            Contact_Overload_Control();
            CHECK(RELAY1 == 0 && (sys.Fault_Error & ((u32)1 << ERR_OVERLOAD1)));
        }
        uart4_service();
        S4CON |= 2;
        UART4_int();
    }
    CHECK(uart4_sending());
    timer_expire();
    CHECK(!uart4_sending() && crc(reply, sizeof(reply)) == 0 && reply[33] == 0);
    timer_expire();                 // 放行完整静默之后的新请求。
    CHECK(config.Set_OVERLOAD_Current1 == 888);
    send_request(new_write, sizeof(new_write));
    collect_reply(reply, 15, 0);
    CHECK(reply[9] == 0 && reply[12] == 80 && config.Set_OVERLOAD_Current1 == 80);
}

/******************************************************************************
 * 函数名称       : simulate_rf_at_summary_boundary
 * 功能描述       : 模拟遥控停止在综合应答构造中途到达。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
void simulate_rf_at_summary_boundary(void) //20261116-12 add
{
    if (simulate_rf) {
        simulate_rf = 0;
        sys.Pump_A_ON = 0;
        config.Hand_Auto_timer_Flag = RUNMODE_MANUAL;
        clear_A_Fault();               // 与现有遥控停止清故障的副作用一致。
    }
}

/******************************************************************************
 * 函数名称       : check_rf_during_summary
 * 功能描述       : 验证遥控插入后，同帧模式和故障仍来自同一次读取。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_rf_during_summary(void) //20261116-12 add
{
    u8 request[] = {1, 0x43, 0xbf, 6, 3, 6, 0, 14, 0, 0, 0};
    u8 reply[97];

    dashboard_fixture();
    sys.Pump_Add_Flag = 0;
    sys.Contact_Overload_A_Flag = 0;
    sys.Fault_Error = 4;
    simulate_rf = 1;
    fill_crc(request, sizeof(request));
    send_request(request, sizeof(request));
    CHECK(simulate_rf == 0 && config.Hand_Auto_timer_Flag == RUNMODE_MANUAL && sys.Fault_Error == 0);
    collect_reply(reply, sizeof(reply), 0);
    CHECK((reply[29] & 0x0f) == 6);
    CHECK(reply[33] == 4 && (reply[83] & 4));
    CHECK(crc(reply, sizeof(reply)) == 0);
}

/******************************************************************************
 * 函数名称       : check_sensor_startup
 * 功能描述       : 模拟外部数据区带有上次运行的传感器状态，检查系统上电初始化。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_sensor_startup(void) //20261116-14 add
{
    u16 i;

    sys_init();
    for (i = 0; i < 132; i++) {
        Water_Level();
    }
    water_status_ready = 0;
    shuiwei_check();
    shuiwei_check();
    CHECK(!water_status_ready);

    sensor_pending_tanks = SENSOR_PENDING_UPPER | SENSOR_PENDING_LOWER;
    sys_init();
    CHECK(sensor_pending_tanks == 0);
    shuiwei_check();
    shuiwei_check();
    CHECK(!water_status_ready);
    shuiwei_check();
    CHECK(water_status_ready);
}

/******************************************************************************
 * 函数名称       : check_sensor_switch_hysteresis
 * 功能描述       : 确认传感器切换后首次新样本使用新类型的档位状态。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_sensor_switch_hysteresis(void) //20261116-14 add
{
    static const u32 middle_level[] = {1};
    static const u32 invalid_value[] = {0};
    static const u8 valid[] = {0};
    static const u8 invalid[] = {3};

    config.Dev_Address = 1;
    config_loaded = 1;
    config.Hand_Auto_timer_Flag = RUNMODE_MANUAL;
    config.Auto_Mode = sys.Auto_Mode = SENSORTYPE_PRESS0_5V;
    config.Sensor_Mode = CTROLTYPE_PUMP;
    config.Sensor_Rang = 1000;
    config.Sensor_STOP = 900;
    config.Sensor_ON1 = 500;
    config.Sensor_ON2 = 300;
    Boot_Flag = sensor_adc_fresh = 1;
    sys.Vol_AD = 1023;
    updatSysdata();
    shuiwei_check();
    RELAY_Auto_Control();
    CHECK(sys.SensorLever == 2);
    write_parameter_request(18, SENSORTYPE_WATER4_20MA);
    sys.Auto_Mode = config.Auto_Mode;
    shuiwei_check();
    RELAY_Auto_Control();

    sys.Cur_AD = 668;                 // 新输入处于第一启动阈值和停止阈值之间。
    sensor_adc_fresh = 1;
    shuiwei_check();
    RELAY_Auto_Control();
    read_runtime(21, 1, middle_level, valid);
    sys.Cur_AD = 99;
    shuiwei_check();
    RELAY_Auto_Control();
    read_runtime(21, 1, invalid_value, invalid);
    CHECK(sys.SensorLever == 1);    // 异常采样不能改写已建立的档位状态。

    config.Hand_Auto_timer_Flag = RUNMODE_TIMER;
    config.Sensor_Mode = CTROLTYPE_DRAIN;
    config.AUTO_Timing_Run = 1;
    config.AUTO_Timing_Stop = 1;
    sys.Auto_timer_count = 0;
    sys.autoTimerFlag = 1;
    sys.shang_DIR = 1;
    sensor_sample_ready = 0;
    RELAY_Auto_Control();
    CHECK(sys.Pump_Flag == 0);
}

/******************************************************************************
 * 函数名称       : collect_fault
 * 功能描述       : 收集独立发送的故障帧并推进发送尾部与静默。
 * 输入参数       : frame，收集位置。
 * 返回值         : None
 * 日期            : 2026.11.24
 ******************************************************************************/
static void collect_fault(u8 *frame) //20261124-4 add
{
    u8 i;

    for (i = 0; i < 24; i++) {
        CHECK(uart4_sending());
        frame[i] = S4BUF;
        S4CON |= 2;
        UART4_int();
    }
    CHECK(uart4_sending());
    uart4_fault_tick();
    CHECK(uart4_sending());
    uart4_fault_tick();
    CHECK(!uart4_sending());
    for (i = 0; i < 11; i++) {
        uart4_fault_tick();
    }
}

/******************************************************************************
 * 函数名称       : check_fault_baseline
 * 功能描述       : 模拟两组保护检查后捕获完整的健康首帧。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.24
 ******************************************************************************/
static void check_fault_baseline(void) //20261124-0 add
{
    static const u8 expected[] = {
        0x01, 0x43, 0xbf, 0x06, 0x03, 0x20, 0x00, 0x01, 0x0d,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x04, 0x3b
    };
    u8 frame[sizeof(expected)];

    uart_init();
    config.Dev_Address = 1;
    config_loaded = 1;
    sys.Fault_Error = 0;
    Boot_Flag = 0;
    uart4_fault_start(1);
    uart4_fault_scanned(0);
    uart4_fault_scanned(1);
    uart4_service();
    CHECK(!uart4_sending());

    Boot_Flag = 1;
    uart4_fault_scanned(1);
    uart4_service();
    CHECK(!uart4_sending());
    uart4_fault_scanned(0);
    uart4_service();
    collect_fault(frame);
    CHECK(memcmp(frame, expected, sizeof(frame)) == 0);

    uart4_fault_scanned(1);
    uart4_service();
    CHECK(!uart4_sending());
}

/******************************************************************************
 * 函数名称       : check_fault_ack
 * 功能描述       : 验证只有匹配的串口确认能释放已发送的首帧。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.24
 ******************************************************************************/
static void check_fault_ack(void) //20261124-0 add
{
    static const u8 wrong_offsets[] = {0, 1, 4, 5, 7, 8, 9, 13, 17};
    u8 ack[20] = {
        1, 0x43, 0xbf, 0x06, 3, 0xa0, 0, 1, 9, 0, 0, 0, 0, 1, 0, 0, 0, 1
    };
    u8 bad[sizeof(ack)];
    u8 i;

    CHECK(uart4_fault_awaiting_ack());
    fill_crc(ack, sizeof(ack));
    for (i = 0; i < sizeof(wrong_offsets); i++) {
        memcpy(bad, ack, sizeof(bad));
        bad[wrong_offsets[i]] ^= 2;
        fill_crc(bad, sizeof(bad));
        send_request(bad, sizeof(bad));
        CHECK(!uart4_sending() && uart4_fault_awaiting_ack());
    }

    memcpy(bad, ack, sizeof(bad));
    bad[19] ^= 1;
    send_request(bad, sizeof(bad));
    CHECK(!uart4_sending() && uart4_fault_awaiting_ack());

    memcpy(bad, ack, sizeof(bad));
    fill_crc(bad, sizeof(bad) - 1);
    send_request(bad, sizeof(bad) - 1);
    CHECK(!uart4_sending() && uart4_fault_awaiting_ack());

    memcpy(bad, ack, sizeof(bad));
    bad[5] = 0x20;                        // 用控制器发出的命令方向尝试确认。
    check_error(bad, sizeof(bad), 1, 0xff);
    CHECK(uart4_fault_awaiting_ack());

    send_request(ack, sizeof(ack));
    CHECK(!uart4_sending() && !uart4_fault_awaiting_ack());
    send_request(ack, sizeof(ack));
    CHECK(!uart4_sending() && !uart4_fault_awaiting_ack());
    check_info(0);
}

/******************************************************************************
 * 函数名称       : check_fault_baseline_with_faults
 * 功能描述       : 启动编号不可用或位图不合法时等待，首次有效时携带实际故障。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.24
 ******************************************************************************/
static void check_fault_baseline_with_faults(void) //20261124-0 add
{
    u8 frame[24];

    uart_init();
    Boot_Flag = 1;
    sys.Fault_Error = 5;
    uart4_fault_scanned(0);
    uart4_fault_scanned(1);
    uart4_service();
    CHECK(!uart4_sending());

    uart4_fault_start(0x12345678UL);
    sys.Fault_Error = 0x80000000UL;       // 保留位无效，不能用零位图替代。
    uart4_fault_scanned(0);
    uart4_service();
    CHECK(!uart4_sending());
    uart4_fault_scanned(1);
    uart4_service();
    CHECK(!uart4_sending());

    sys.Fault_Error = 5;
    uart4_fault_scanned(1);
    uart4_service();
    collect_fault(frame);
    CHECK(frame[5] == 0x20 && frame[7] == 1 && frame[8] == 13);
    CHECK(frame[9] == 0x12 && frame[10] == 0x34 && frame[11] == 0x56 && frame[12] == 0x78);
    CHECK(frame[16] == 1 && frame[17] == 0);
    CHECK(frame[18] == 0 && frame[19] == 0 && frame[20] == 0 && frame[21] == 5);
    CHECK(crc(frame, sizeof(frame)) == 0);
}

#if BF4G_BENCH_BOOT_INIT
/******************************************************************************
 * 函数名称       : check_bench_boot_init
 * 功能描述       : 从擦空的独立板启动，并验证首次上报前的存储判断。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.24
 ******************************************************************************/
static void check_bench_boot_init(void) //20261124-6 add
{
    u8 frame[24];
    u8 parameters[sizeof(flash)];
    u32 boot;

    memcpy(parameters, flash, sizeof(flash));
    boot_writes = 0;
    memset(boot_flash, 0xff, sizeof(boot_flash));
    boot_probe_available = 0;
    CHECK(Par_AdvanceFaultBoot() == 0 && boot_writes == 0);
    boot_probe_available = 1;

    boot_flash[1][511] = 0;
    CHECK(Par_AdvanceFaultBoot() == 0 && boot_writes == 0);
    boot_flash[1][511] = 0xff;

    boot_write_limit = 3;
    CHECK(Par_AdvanceFaultBoot() == 0 && boot_writes == 1);
    boot_write_limit = 12;
    CHECK(Par_AdvanceFaultBoot() == 0 && boot_writes == 1);

    memset(boot_flash, 0xff, sizeof(boot_flash));
    boot_writes = 0;
    boot = Par_AdvanceFaultBoot();
    CHECK(boot == 1 && boot_writes == 2);
    uart_init();
    uart4_fault_cold_start();
    uart4_fault_start(boot);
    Boot_Flag = 1;
    config.Dev_Address = 1;
    sys.Fault_Error = 0;
    uart4_fault_scanned(0);
    uart4_fault_scanned(1);
    uart4_service();
    collect_fault(frame);
    CHECK(frame[5] == 0x20 && frame[12] == 1 && frame[16] == 1 && frame[17] == 0);
    CHECK(crc(frame, sizeof(frame)) == 0);
    CHECK(Par_AdvanceFaultBoot() == 2 && boot_writes == 3);
    CHECK(memcmp(parameters, flash, sizeof(flash)) == 0);
}
#endif

/******************************************************************************
 * 函数名称       : check_fault_boot_store
 * 功能描述       : 核对两份首次初始化数据的续号和独立擦写。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.24
 ******************************************************************************/
static void check_fault_boot_store(void) //20261124-1 add
{
    static const u8 seed[12] = {0x42, 0x46, 0x30, 0x36, 0, 0, 0, 0, 0xff, 0xff, 0xff, 0xff};
    u8 current[sizeof(flash)];

    memcpy(current, flash, sizeof(flash));
    boot_writes = 0;
    memset(boot_flash, 0xff, sizeof(boot_flash));
#if !BF4G_BENCH_BOOT_INIT
    CHECK(Par_AdvanceFaultBoot() == 0);
    CHECK(boot_writes == 0);
#endif

    memcpy(boot_flash[0], seed, sizeof(seed));
    memcpy(boot_flash[1], seed, sizeof(seed));
    CHECK(Par_AdvanceFaultBoot() == 1);
    CHECK(Par_AdvanceFaultBoot() == 2);
    CHECK(boot_writes == 2);
    uart_init();
    uart4_fault_start(2);
    uart4_fault_scanned(0);
    uart4_fault_scanned(1);
    uart4_service();
    CHECK(boot_writes == 2);

    boot_write_limit = 8;
    CHECK(Par_AdvanceFaultBoot() == 0);
    CHECK(boot_writes == 3);
    boot_write_limit = 12;
    uart_init();
    uart4_fault_start(Par_AdvanceFaultBoot());
    uart4_fault_scanned(0);
    uart4_fault_scanned(1);
    uart4_service();
    CHECK(!uart4_sending() && boot_writes == 3);
    check_info(0);

#if !BF4G_BENCH_BOOT_INIT
    memset(boot_flash, 0xff, sizeof(boot_flash));
    CHECK(Par_AdvanceFaultBoot() == 0 && boot_writes == 3);
#endif
    CHECK(memcmp(current, flash, sizeof(flash)) == 0);
}

/******************************************************************************
 * 函数名称       : fault_ticks
 * 功能描述       : 模拟实际经过的毫秒节拍。
 * 输入参数       : count，节拍数。
 * 返回值         : None
 * 日期            : 2026.11.24
 ******************************************************************************/
static void fault_ticks(u16 count) //20261124-2 add
{
    while (count != 0) {
        uart4_fault_tick();
        uart4_service();
        count--;
    }
}

/******************************************************************************
 * 函数名称       : check_fault_retries
 * 功能描述       : 验证重发原帧、最后确认窗口及重新上电。
 * 输入参数       : ack_on_last，是否在最后窗口提供确认。
 * 返回值         : None
 * 日期            : 2026.11.24
 ******************************************************************************/
static void check_fault_retries(bit ack_on_last) //20261124-2 add
{
    u8 original[24];
    u8 frame[24];
    u8 ordinary[27];
    u8 ack[20] = {1, 0x43, 0xbf, 6, 3, 0xa0, 0, 1, 9, 0, 0, 0, 0, 1, 0, 0, 0, 1};
    u8 retry;

    uart_init();
    uart4_fault_cold_start();
    uart4_fault_start(1);
    config.Dev_Address = 1;
    Boot_Flag = 1;
    sys.Fault_Error = 0;
    uart4_fault_scanned(0);
    uart4_fault_scanned(1);
    uart4_service();
    CHECK(uart4_sending());
    fault_ticks(6000);                   // 发送还未结束，确认窗口不能提前开始。
    collect_fault(original);
    for (retry = 0; retry < 10; retry++) {
        fault_ticks(4989);
        CHECK(!uart4_sending());
        if (retry == 0) {
            send_request(info_request, sizeof(info_request));
            fault_ticks(1);
            collect_reply(ordinary, sizeof(ordinary), 0);
            fault_ticks(11);
        } else {
            fault_ticks(1);
        }
        collect_fault(frame);
        CHECK(memcmp(frame, original, sizeof(frame)) == 0);
    }

    fault_ticks(4989);
    CHECK(!uart4_sending() && uart4_fault_awaiting_ack());
    fill_crc(ack, sizeof(ack));
    if (ack_on_last) {
        send_request(ack, sizeof(ack));
        CHECK(!uart4_fault_awaiting_ack());
        return;
    }

    fault_ticks(1);
    CHECK(!uart4_sending() && !uart4_fault_awaiting_ack());
    send_request(ack, sizeof(ack));      // 停发后迟到确认不能恢复。
    sys.Fault_Error = 4;
    uart4_fault_scanned(1);
    fault_ticks(6000);
    CHECK(!uart4_sending());
    check_info(0);

    uart_init();                        // 软件复位仍属于本次通电。
    uart4_fault_start(2);
    uart4_fault_scanned(0);
    uart4_fault_scanned(1);
    uart4_service();
    CHECK(!uart4_sending());

    uart4_fault_cold_start();            // 模拟控制器失电后重新上电。
    uart4_fault_start(3);
    uart4_fault_scanned(0);
    uart4_fault_scanned(1);
    uart4_service();
    collect_fault(frame);
    CHECK(frame[12] == 3 && frame[21] == 4);
}

/******************************************************************************
 * 函数名称       : ack_fault_frame
 * 功能描述       : 按收到的事件身份发送匹配确认。
 * 输入参数       : frame，收到的主动事件。
 * 返回值         : None
 * 日期            : 2026.11.24
 ******************************************************************************/
static void ack_fault_frame(const u8 *frame) //20261124-3 add
{
    u8 ack[20];

    memcpy(ack, frame, 8);
    ack[5] = 0xa0;
    ack[8] = 9;
    ack[9] = 0;
    memcpy(ack + 10, frame + 9, 8);
    fill_crc(ack, sizeof(ack));
    send_request(ack, sizeof(ack));
}

/******************************************************************************
 * 函数名称       : check_fault_changes
 * 功能描述       : 验证普通变化、合并回到原值和无效后同值恢复。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.24
 ******************************************************************************/
static void check_fault_changes(void) //20261124-3 add
{
    u8 frame[24];

    uart_init();
    uart4_fault_cold_start();
    uart4_fault_start(7);
    config.Dev_Address = 1;
    Boot_Flag = 1;
    sys.Fault_Error = 0;
    uart4_fault_scanned(0);
    uart4_fault_scanned(1);
    uart4_service();
    collect_fault(frame);

    sys.Fault_Error = 4;
    uart4_fault_scanned(1);
    sys.Fault_Error = 0;
    uart4_fault_scanned(1);
    uart4_service();
    CHECK(!uart4_sending());
    ack_fault_frame(frame);
    uart4_service();
    collect_fault(frame);
    CHECK(frame[7] == 3 && frame[16] == 3 && frame[17] == 2 && frame[21] == 0);
    ack_fault_frame(frame);

    sys.Fault_Error = 5;
    uart4_fault_scanned(1);
    uart4_service();
    collect_fault(frame);
    CHECK(frame[16] == 4 && frame[17] == 1 && frame[21] == 5);
    sys.Fault_Error = 4;
    uart4_fault_scanned(1);
    ack_fault_frame(frame);
    uart4_service();
    collect_fault(frame);
    CHECK(frame[16] == 5 && frame[17] == 1 && frame[21] == 4);
    ack_fault_frame(frame);

    sys.Fault_Error = 0x80010000UL;
    uart4_fault_scanned(1);
    uart4_service();
    CHECK(!uart4_sending());
    sys.Fault_Error = 4;
    uart4_fault_scanned(1);
    uart4_service();
    collect_fault(frame);
    CHECK(frame[16] == 6 && frame[17] == 2 && frame[21] == 4);
    ack_fault_frame(frame);

    uart4_fault_scanned(1);
    uart4_service();
    CHECK(!uart4_sending());
    sys.Fault_Error = 0x00010000UL;
    uart4_fault_scanned(1);
    uart4_service();
    if (TEST_PUMPS == 2) {
        collect_fault(frame);
        CHECK(frame[16] == 7 && frame[17] == 1 && frame[19] == 1 && frame[21] == 0);
    } else {
        CHECK(!uart4_sending());
    }
}

/******************************************************************************
 * 函数名称       : check_fault_stop_overlap
 * 功能描述       : 在主动发送期间注入第一条停泵请求并验证唯一的最终应答。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.24
 ******************************************************************************/
static void check_fault_stop_overlap(void) //20261124-4 add
{
    u8 stop[] = {1, 0x43, 0xbf, 6, 3, 0x11, 0, 1, 2, 2, 1, 0, 0};
    u8 event[24];
    u8 reply[20];
    u8 i;

    uart_init();
    uart4_fault_cold_start();
    uart4_fault_start(7);
    config.Dev_Address = 1;
    config.Hand_Auto_timer_Flag = RUNMODE_MANUAL;
    Boot_Flag = 1;
    sys.Pump_A_ON = 1;
    sys.Pump_A_state = 1;
    sys.Fault_Error = 0;
    uart4_fault_scanned(0);
    uart4_fault_scanned(1);
    uart4_service();

    fill_crc(stop, sizeof(stop));
    for (i = 0; i < sizeof(event); i++) {
        CHECK(uart4_sending() && uart4_blocks_save());
        event[i] = S4BUF;
        if (i < sizeof(stop)) {
            receive_byte(stop[i]);      // 第一字节分别从两个方向同时开始传输。
        }
        if (i == sizeof(stop) + 9) {
            timer_expire();              // 停泵请求在故障帧结束前收全。
        }
        S4CON |= 2;
        UART4_int();
    }
    CHECK(crc(event, sizeof(event)) == 0);
    CHECK(uart4_sending());
    fault_ticks(1);
    CHECK(uart4_sending());
    fault_ticks(1);
    CHECK(!uart4_sending());
    fault_ticks(10);
    scan_control();
    CHECK(!uart4_sending());           // 未达到主动帧后的完整静默。
    fault_ticks(1);
    CHECK(uart4_blocks_save() && !uart4_sending());
    ack_fault_frame(event);
    CHECK(uart4_fault_awaiting_ack());
    scan_control();
    collect_reply(reply, sizeof(reply), 0);
    CHECK(reply[5] == 0x91 && reply[7] == 1 && reply[9] == 0);
    CHECK(reply[10] == 2 && (reply[13] & 0x10) == 0 && crc(reply, sizeof(reply)) == 0);
    CHECK(!sys.Pump_A_ON);
    timer_expire();                     // 等待期间额外数据丢弃后重新同步。
    ack_fault_frame(event);
    fault_ticks(20);
    CHECK(!uart4_sending() && !uart4_fault_awaiting_ack());
}

/******************************************************************************
 * 函数名称       : check_fault_address_change
 * 功能描述       : 验证改址后仍保留旧址原帧，普通请求和后续帧使用新址。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.24
 ******************************************************************************/
static void check_fault_address_change(void) //20261124-4 add
{
    u8 write[] = {1, 0x43, 0xbf, 6, 3, 0x10, 0, 1, 3, 11, 0, 2, 0, 0};
    u8 reply[15];
    u8 original[24];
    u8 event[24];

    uart_init();
    uart4_fault_cold_start();
    uart4_fault_start(7);
    config.Dev_Address = 1;
    Boot_Flag = 1;
    sys.Fault_Error = 4;
    uart4_fault_scanned(0);
    uart4_fault_scanned(1);
    uart4_service();
    collect_fault(original);

    fill_crc(write, sizeof(write));
    send_request(write, sizeof(write));
    collect_reply(reply, sizeof(reply), 0);
    CHECK(config.Dev_Address == 2 && reply[0] == 1 && reply[5] == 0x90);
    sys.Fault_Error = 0;
    uart4_fault_scanned(1);
    fault_ticks(4990);
    collect_fault(event);
    CHECK(memcmp(event, original, sizeof(event)) == 0);

    event[0] = 2;
    ack_fault_frame(event);
    CHECK(uart4_fault_awaiting_ack());
    query_address(1, 0);
    query_address(2, 1);
    ack_fault_frame(original);
    CHECK(!uart4_fault_awaiting_ack());
    fault_ticks(11);
    collect_fault(event);
    CHECK(event[0] == 2 && event[16] == 2 && event[17] == 1 && event[21] == 0);
    CHECK(crc(event, sizeof(event)) == 0);
}

/******************************************************************************
 * 函数名称       : check_fault_rid_wrap
 * 功能描述       : 核对关联号回卷和三十二位序号的大端编码。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.24
 ******************************************************************************/
static void check_fault_rid_wrap(void) //20261124-5 add
{
    u8 frame[24];

    bf4g_fault_encode(frame, 1, 65535UL, 1, 0);
    CHECK(frame[6] == 0xff && frame[7] == 0xff && crc(frame, sizeof(frame)) == 0);
    bf4g_fault_encode(frame, 1, 65536UL, 1, 0);
    CHECK(frame[6] == 0 && frame[7] == 1 && frame[14] == 1 && frame[16] == 0);
    bf4g_fault_encode(frame, 1, 0xffffffffUL, 2, 0);
    CHECK(frame[6] == 0xff && frame[7] == 0xff && frame[13] == 0xff && frame[16] == 0xff);
    CHECK(crc(frame, sizeof(frame)) == 0);
}

/******************************************************************************
 * 函数名称       : check_fault_mask_matrix
 * 功能描述       : 逐位验证六种机型的全部故障位适用范围。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.24
 ******************************************************************************/
static void check_fault_mask_matrix(void) //20261124-5 add
{
    u32 allowed = 0x0001ffffUL;
    u32 value;
    u32 mask;
    u8 i;

    if (TEST_PHASES == 1) {
        allowed &= ~0x00000e70UL;
    }
    if (TEST_PUMPS == 1) {
        allowed &= ~0x00014f80UL;
    }
    for (i = 0; i < 32; i++) {
        mask = (u32)1 << i;
        sys.Fault_Error = mask;
        CHECK(bf4g_fault_sample(&value) == ((mask & allowed) != 0));
        CHECK(value == mask);
    }
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
    u8 boot_writes_before_reporting; //20261124-5 add 常规上报全流程的编号写入基线。

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
    check_control_deadline();
    check_control_limits();
    check_control_contact();
    check_display_digits();
    check_main_summary_length();
    check_main_summary_fields();
    check_web_summary_length();
    check_web_summary();
    check_final_vectors();
    check_long_send_integration();
    check_rf_during_summary();
    check_sensor_switch_hysteresis();
    check_sensor_startup();
    check_fault_baseline(); //20261124-0 add 核对启动基线的串口报文。
    check_fault_ack(); //20261124-0 add 校验主动报文确认边界。
    check_fault_baseline_with_faults(); //20261124-0 add 核对带故障启机和无效位图。
#if BF4G_BENCH_BOOT_INIT
    check_bench_boot_init(); //20261124-6 add 先验证只烧程序的清空板。
#endif
    check_fault_boot_store(); //20261124-1 add 检查首次初始化和安全续号。
    boot_writes_before_reporting = boot_writes;
    check_fault_retries(0); //20261124-2 add 核对重发、停报和再次上电。
    check_fault_retries(1);
    check_fault_changes(); //20261124-3 add 核对完整活动位图及等待期间的合并。
    check_fault_stop_overlap(); //20261124-4 add 验证主动发送和首次停泵重叠。
    check_fault_address_change(); //20261124-4 add 验证改址后原事件的确认范围。
    check_fault_rid_wrap(); //20261124-5 add 确认关联号独立于完整报文序号。
    check_fault_mask_matrix();
    CHECK(boot_writes == boot_writes_before_reporting);
    puts("BF4G_UART_TEST=PASS");
    return 0;
}
