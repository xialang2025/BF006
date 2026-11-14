#include "snapshot4g.h"
#include "uart4.h"
#include "Modbus_New.h"
#include "Display.h"
#include "HT1622.h"
#include "user.h"

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
static u32 Snapshot_Get32(u8 offset);

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
 * 函数名称       : Snapshot_Check_Setting
 * 功能描述       : 核对单写字段、原宽度和当前允许范围。
 * 输入参数       : field 字段号，value 请求值，minimum 和 maximum 可用区间
 * 返回值         : 协议拒绝码或零
 * 日期            : 2026.11.14
 ******************************************************************************/
static u8 Snapshot_Check_Setting(u8 field, u32 value, u32 *minimum, u32 *maximum) //20261114-8 add
{
    u8 continuous;

    continuous = config.Auto_Mode >= SENSORTYPE_WATER4_20MA && config.Auto_Mode <= SENSORTYPE_PRESS0_5V;
    *minimum = 0;
    *maximum = 0;
    if (field < 16 || field > 26 || field == 20) {
        return 0x25;
    }
    if (value > 0xFFFFUL || (field == 16 && value > 1)) {
        return 0x2A;
    }
    switch (field) {
    case 16:
        if (config.Auto_Mode > SENSORTYPE_PRESS) {
            return 0x24;
        }
        *maximum = 1;
        break;
    case 17:
        if (!continuous) {
            return 0x20;
        }
        if ((config.Sensor_Mode == CTROLTYPE_PUMP && config.Sensor_ON1 >= config.Sensor_STOP) ||
            (config.Sensor_Mode == CTROLTYPE_DRAIN && config.Sensor_ON1 <= config.Sensor_STOP) ||
            config.Sensor_Mode > CTROLTYPE_DRAIN) {
            return 0x24;
        }
        *minimum = config.Sensor_ON1 > config.Sensor_STOP ? config.Sensor_ON1 : config.Sensor_STOP;
        if (*minimum == 0) {
            *minimum = 1;
        }
        if (value < config.Sensor_Rang && *minimum <= config.Sensor_overFlow) {
            *minimum = (u32)config.Sensor_overFlow + 1;
        }
        *maximum = 9999;
        break;
    case 18:
    case 19:
        if (!continuous || config.Sensor_Mode > CTROLTYPE_DRAIN) {
            return 0x20;
        }
        *maximum = config.Sensor_Rang < 9999 ? config.Sensor_Rang : 9999;
        if ((field == 18) == (config.Sensor_Mode == CTROLTYPE_PUMP)) {
            if (!(field == 18 ? config.Sensor_STOP : config.Sensor_ON1)) {
                return 0x24;
            }
            if (*maximum >= (field == 18 ? config.Sensor_STOP : config.Sensor_ON1)) {
                *maximum = (field == 18 ? config.Sensor_STOP : config.Sensor_ON1) - 1;
            }
        } else {
            *minimum = (u32)(field == 18 ? config.Sensor_STOP : config.Sensor_ON1) + 1;
        }
        break;
    case 21:
    case 22:
        *minimum = 85;
        *maximum = (VoltageValue == Vol_380) ? 480 : 280;
        if (field == 21 && *maximum >= config.OVP_Value) {
            if (!config.OVP_Value) {
                return 0x24;
            }
            *maximum = config.OVP_Value - 1;
        }
        if (field == 22 && *minimum <= config.UVP_Value) {
            *minimum = (u32)config.UVP_Value + 1;
        }
        break;
    case 23:
    case 24:
        *maximum = Limit_Current < 800 ? Limit_Current : 800;
        if (field == 23) {
            *minimum = (u32)config.Set_STANDBY_Current1 + 1;
        } else if (*maximum >= config.Set_OVERLOAD_Current1) {
            *maximum = config.Set_OVERLOAD_Current1 ? config.Set_OVERLOAD_Current1 - 1 : 0;
        }
        break;
    case 25:
    case 26:
        return 0x30; // 循环分钟上限等待实体核验
    }
    if (*minimum > *maximum || value < *minimum || value > *maximum) {
        return 0x23;
    }
    return 0;
}

/******************************************************************************
 * 函数名称       : Snapshot_Check_Water_Group
 * 功能描述       : 核对供排水组的三个最终目标值。
 * 输入参数       : None
 * 返回值         : 协议拒绝码或零，change 用于返回是否有实质变更
 * 日期            : 2026.11.14
 ******************************************************************************/
static u8 Snapshot_Check_Water_Group(u8 *change) //20261114-9 add
{
    u32 direction;
    u32 start;
    u32 stop;

    if (uart4_frame[9] != 38 || uart4_frame[32] != 3 ||
        uart4_frame[33] != 16 || uart4_frame[38] != 18 || uart4_frame[43] != 19) {
        return 3;
    }
    direction = Snapshot_Get32(34);
    start = Snapshot_Get32(39);
    stop = Snapshot_Get32(44);
    if (direction > CTROLTYPE_DRAIN || start > 0xFFFFUL || stop > 0xFFFFUL) {
        return 0x2A;
    }
    if (config.Auto_Mode <= SENSORTYPE_PRESS) {
        if (start != config.Sensor_ON1 || stop != config.Sensor_STOP) {
            return 0x24;
        }
    } else if (config.Auto_Mode > SENSORTYPE_PRESS0_5V) {
        return 0x20;
    } else if (start > config.Sensor_Rang || stop > config.Sensor_Rang || start > 9999 || stop > 9999) {
        return 0x23;
    } else if ((direction == CTROLTYPE_PUMP && start >= stop) ||
               (direction == CTROLTYPE_DRAIN && start <= stop)) {
        return 0x24;
    }
    *change = (direction != config.Sensor_Mode || start != config.Sensor_ON1 || stop != config.Sensor_STOP);
    return 0;
}

/******************************************************************************
 * 函数名称       : Snapshot_Setting_Reply
 * 功能描述       : 验证单写并返回实际值，存储尚未接入时拒绝改变配置。
 * 输入参数       : length 返回的数据区长度
 * 返回值         : 协议结果码
 * 日期            : 2026.11.14
 ******************************************************************************/
static u8 Snapshot_Setting_Reply(u8 *length) //20261114-9 change
{
    u8 field;
    u8 mode;
    u8 result;
    u8 flags;
    u8 phase;
    u8 group;
    u8 changed;
    u32 boot;
    u32 revision;
    u32 now;
    u32 deadline;
    u32 value;
    u32 minimum;
    u32 maximum;
    u32 actual;

    if (!Snapshot_Test_Context(&boot, &revision, &now)) {
        return SNAPSHOT_CONTEXT_UNAVAILABLE;
    }
    group = uart4_frame[5] == 0x12;
    field = group ? 0 : uart4_frame[31];
    mode = uart4_frame[30];
    deadline = Snapshot_Get32(26);
    value = Snapshot_Get32(32);
    flags = 1;
    phase = 1;
    minimum = maximum = 0;
    Snapshot_Write_Data(SNAPSHOT_CONTEXT_DATA);
    if (!Snapshot_Read_Field(field, &actual)) {
        flags |= 2;
    }
    if (mode > 1) {
        result = 0x33;
    } else if (Snapshot_Get32(18) != boot) {
        result = 0x27;
    } else if (!Snapshot_Get32(10) && !Snapshot_Get32(14)) {
        result = 0x2A;
    } else if (deadline <= now || deadline - now > 10000UL) {
        result = 0x2E;
    } else if (Snapshot_Get32(22) != revision) {
        result = 0x26;
    } else if (config.Hand_Auto_timer_Flag != RUNMODE_MANUAL) {
        result = 0x20;
    } else if (sys.Pump_A_state || sys.Pump_A_ON) {
        result = 0x21;
    } else if (group) {
        changed = 0;
        result = uart4_frame[31] == 1 ? Snapshot_Check_Water_Group(&changed) : 0x02;
        if (!result) {
            if (changed) {
                result = 0x30;
            } else {
                phase = 4;
            }
        }
    } else {
        result = Snapshot_Check_Setting(field, value, &minimum, &maximum);
        if ((!result || result == 0x23) && minimum <= maximum) {
            flags |= 12;
        }
        if (!result) {
            if (value == actual) {
                phase = 4;
            } else {
                result = 0x30;
            }
        }
    }
    memmove(&uart4_frame[11], &uart4_frame[10], 8);
    memset(&uart4_frame[19], 0, 29);
    Snapshot_Put32(19, boot);
    Snapshot_Put32(23, revision);
    Snapshot_Put32(27, now);
    uart4_frame[31] = phase;
    uart4_frame[32] = flags | (mode == 1 ? 16 : 0);
    uart4_frame[34] = uart4_frame[SNAPSHOT_CONTEXT_DATA + 17];
    uart4_frame[35] = field;
    Snapshot_Put32(36, actual);
    if (flags & 12) {
        Snapshot_Put32(40, minimum);
        Snapshot_Put32(44, maximum);
    }
    *length = SNAPSHOT_CONTEXT_SIZE + (mode == 1 ? SNAPSHOT_DATA_SIZE : 0);
    return result;
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

typedef struct {
    u8 request[22];
    u8 evidence[57];
    u32 accepted;
    u32 observed;
    u32 revision;
    u8 phase;
    u8 result;
} Control_Record;

static Control_Record xdata controls[2];
static u8 xdata control_high_water[8];
static u32 control_boot;

/******************************************************************************
 * 函数名称       : Snapshot_Get32
 * 功能描述       : 从共享帧读取大端四字节字段。
 * 输入参数       : offset 字段偏移
 * 返回值         : 字段值
 * 日期            : 2026.11.14
 ******************************************************************************/
static u32 Snapshot_Get32(u8 offset) //20261114-7 add
{
    u8 i;
    u32 value;

    value = 0;
    for (i = 0; i < 4; i++) {
        value = (value << 8) | uart4_frame[offset + i];
    }
    return value;
}

/******************************************************************************
 * 函数名称       : Control_Capture
 * 功能描述       : 保存本次操作确认时的有效位和业务字段。
 * 输入参数       : entry 结果槽，revision 配置版本，now 采样时刻
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
static void Control_Capture(Control_Record xdata *entry, u32 revision, u32 now) //20261114-7 add
{
    Snapshot_Write_Data(SNAPSHOT_CONTEXT_DATA);
    memcpy(entry->evidence, &uart4_frame[SNAPSHOT_CONTEXT_DATA + 4], sizeof(entry->evidence));
    entry->observed = now;
    entry->revision = revision;
}

/******************************************************************************
 * 函数名称       : Control_Observe
 * 功能描述       : 从实际运行状态收束已经触发的启停，冻结已完成证据。
 * 输入参数       : entry 结果槽，revision 配置版本，now 当前时刻
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
static void Control_Observe(Control_Record xdata *entry, u32 revision, u32 now) //20261114-7 add
{
    u8 target;

    if (entry->phase != 2) {
        return;
    }
    target = entry->request[21] == 1;
    if (now < entry->accepted || now - entry->accepted >= 5000UL) {
        if (target && !sys.Pump_A_state) {
            sys.Pump_A_ON = 0;
        }
        entry->phase = 6;
        entry->result = 0x34;
    } else if ((sys.Pump_A_state != 0) == target) {
        entry->phase = 3;
    }
    Control_Capture(entry, revision, now);
}

/******************************************************************************
 * 函数名称       : Control_Reply
 * 功能描述       : 按原编号保存或读取结果，仿真使用注入的执行上下文。
 * 输入参数       : length 返回的数据区长度
 * 返回值         : 协议结果码
 * 日期            : 2026.11.14
 ******************************************************************************/
static u8 Control_Reply(u8 *length) //20261114-7 add
{
    Control_Record xdata *entry;
    u8 i;
    u8 action;
    u8 mode;
    u8 phase;
    u8 result;
    u8 historical;
    u8 query;
    u16 pump_state;
    u32 boot;
    u32 revision;
    u32 now;
    u32 deadline;

    if (!Snapshot_Test_Context(&boot, &revision, &now) || !boot || !revision) {
        return SNAPSHOT_CONTEXT_UNAVAILABLE;
    }
    if (boot != control_boot) {
        memset(controls, 0, sizeof(controls));
        memset(control_high_water, 0, sizeof(control_high_water));
        control_boot = boot;
    }
    query = uart4_frame[5] == 0x13;
    mode = uart4_frame[query ? 22 : 30];
    action = query ? 0 : uart4_frame[31];
    deadline = query ? 0 : Snapshot_Get32(26);
    phase = query ? 6 : 1;
    result = 0;
    historical = 0;
    entry = 0;
    if (mode > 1) {
        result = 0x33;
    } else if (Snapshot_Get32(18) != boot) {
        result = 0x27;
    } else if (!query && (action < 1 || action > 6)) {
        result = 0x2A;
    } else if (!Snapshot_Get32(10) && !Snapshot_Get32(14)) {
        result = 0x2A;
    } else {
        for (i = 0; i < 2; i++) {
            Control_Observe(&controls[i], revision, now);
            if (controls[i].phase && controls[i].phase != 2 &&
                now >= controls[i].observed && now - controls[i].observed >= 600000UL) {
                controls[i].phase = 0;
            }
            if (controls[i].phase && !memcmp(controls[i].request, &uart4_frame[10], 8)) {
                entry = &controls[i];
            }
        }
        if (entry) {
            if (!query && memcmp(entry->request, &uart4_frame[10], 22)) {
                entry = 0;
                result = 0x2B;
            } else {
                historical = 1;
            }
        } else if (query || memcmp(&uart4_frame[10], control_high_water, 8) <= 0) {
            result = 0x2C;
        } else if (deadline <= now || deadline - now > 10000UL) {
            result = 0x2E;
        } else if (action != 2 && Snapshot_Get32(22) != revision) {
            result = 0x26;
        } else {
            memcpy(control_high_water, &uart4_frame[10], 8);
            entry = &controls[action == 2];
            if (entry->phase == 2) {
                entry = 0;
                result = 6;
            } else {
                memcpy(entry->request, &uart4_frame[10], sizeof(entry->request));
                entry->phase = 1;
                entry->accepted = now;
                entry->result = 0;
                if (action <= 2) {
                    if (config.Hand_Auto_timer_Flag != RUNMODE_MANUAL) {
                        entry->result = 0x20;
                    } else if (action == 1 && (sys.Fault_A_Flag || sys.Fault_Error || config.Lock1)) {
                        entry->result = 0x22;
                    } else {
                        if (action == 2 && controls[0].phase == 2 && controls[0].request[21] == 1) {
                            controls[0].phase = 5;
                            controls[0].result = 0x31;
                            Control_Capture(&controls[0], revision, now);
                        }
                        sys.Pump_A_ON = action == 1;
                        entry->phase = ((sys.Pump_A_state != 0) == (action == 1)) ? 4 : 2;
                    }
                } else if (action == 3) {
                    if (!sys.Fault_Error && !sys.Fault_A_Flag) {
                        entry->phase = 4;
                    } else {
                        clear_A_Fault();
                        entry->result = (sys.Fault_Error || sys.Fault_A_Flag) ? 0x22 : 0;
                        entry->phase = entry->result ? 5 : 3;
                    }
                } else if (config.Hand_Auto_timer_Flag == action - 4) {
                    entry->phase = 4;
                } else {
                    entry->result = 0x30; // 模式切换等待统一保存和循环能力验证
                }
                Control_Capture(entry, revision, now);
            }
        }
    }

    Snapshot_Write_Data(SNAPSHOT_CONTEXT_DATA);
    if (entry) {
        memcpy(&uart4_frame[SNAPSHOT_CONTEXT_DATA + 4], entry->evidence, sizeof(entry->evidence));
        result = entry->result;
        phase = entry->phase;
        now = entry->observed;
        revision = entry->revision;
    }
    pump_state = ((u16)uart4_frame[SNAPSHOT_CONTEXT_DATA + 16] << 8) |
                 uart4_frame[SNAPSHOT_CONTEXT_DATA + 17];
    memmove(&uart4_frame[11], &uart4_frame[10], 8);
    memset(&uart4_frame[19], 0, 29);
    Snapshot_Put32(19, boot);
    Snapshot_Put32(23, revision);
    Snapshot_Put32(27, now);
    uart4_frame[31] = phase;
    uart4_frame[32] = 1 | (mode == 1 ? 16 : 0) | (historical ? 32 : 0);
    Snapshot_Put16(33, pump_state);
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
void Snapshot4G_Process(void) //20261114-9 change
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
#ifdef BF006_CONTEXT_SIMULATION
    } else if (uart4_frame[5] == 0x11 || uart4_frame[5] == 0x12) {
        if (uart4_frame[8] || (uart4_frame[5] == 0x11 && (length != 38 || uart4_frame[9] != 26)) ||
            (uart4_frame[5] == 0x12 && (length < 35 || length != uart4_frame[9] + 12))) {
            result = 3;
        } else {
            result = Snapshot_Setting_Reply(&data_length);
        }
    } else if (uart4_frame[5] == 0x10 || uart4_frame[5] == 0x13) {
        if (uart4_frame[8] || (uart4_frame[5] == 0x10 && (length != 34 || uart4_frame[9] != 22)) ||
            (uart4_frame[5] == 0x13 && (length != 25 || uart4_frame[9] != 13))) {
            result = 3;
        } else {
            result = Control_Reply(&data_length);
        }
#endif
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
