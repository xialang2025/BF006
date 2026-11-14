#include "system.h"
#include "configPar.h"
#include "RS485.h"
#include "Display.h"
#include "HT1622.h" //20260713-0 add 读取上下水池当前六段液晶显示状态
#include "user.h"
#include "Modbus_New.h"
#include "string.h"

#if (Model_OUT == Model_1C1) || (Function_RS485 == 1 && MODBUS_USE_OLD_PROTOCOL == 0)
/******************************************************************************
 * 函数名称       : BF006_Modbus_CRC
 * 功能描述       : 计算现场串口和快照共用的循环冗余校验。
 * 输入参数       : buf 帧内容，len 校验长度
 * 返回值         : 校验值
 * 日期            : 2026.11.14
 ******************************************************************************/
u16 BF006_Modbus_CRC(u8 *buf, u8 len) //20261114-2 change
{
    u8 i;
    u8 j;
    u16 crc;

    crc = 0xffff;
    for (i = 0; i < len; i++)
    {
        crc ^= buf[i];
        for (j = 0; j < 8; j++)
        {
            if ((crc & 0x0001) != 0)
            {
                crc = (crc >> 1) ^ 0xa001;
            }
            else
            {
                crc = crc >> 1;
            }
        }
    }

    return crc;
}

/******************************************************************************
 * 函数名称       : BF006_Device_Spec
 * 功能描述       : 按现行过载设定返回设备规格低字节。
 * 输入参数       : None
 * 返回值         : 设备规格编码
 * 日期            : 2026.11.14
 ******************************************************************************/
u8 BF006_Device_Spec(void) //20261114-2 change
{
    u8 spec;

    spec = 0x93;
    if (config.Set_OVERLOAD_Current1 == V1_Set_Current)
    {
        spec = 0x90;
    }
    else if (config.Set_OVERLOAD_Current1 == V2_Set_Current)
    {
        spec = 0x91;
    }
    else if (config.Set_OVERLOAD_Current1 == V3_Set_Current)
    {
        spec = 0x92;
    }
    else if (config.Set_OVERLOAD_Current1 == V5_Set_Current)
    {
        spec = 0x94;
    }

    return spec;
}

/******************************************************************************
 * 函数名称       : BF006_Fault_Code
 * 功能描述       : 查找当前故障位图中优先级最高的已定义故障。
 * 输入参数       : faults 完整故障位图
 * 返回值         : 对外故障码
 * 日期            : 2026.11.14
 ******************************************************************************/
u16 BF006_Fault_Code(u32 faults) //20261114-2 change
{
    u8 i;
    u32 mask;

    if (faults == 0)
    {
        return 0;
    }

    for (i = 0; i < ERR_NUMB; i++)
    {
        mask = (u32)1 << i;
        if ((faults & mask) != 0)
        {
            return (u16)(i + 1);
        }
    }

    return 0;
}

#endif

#if (Function_RS485 == 1)
#if (MODBUS_USE_OLD_PROTOCOL == 0)

struct Uart_Type RecData; //20260430-0 add RX buffer used by the new protocol, keeping the old UART driver reference unchanged

extern void Record_Clr_and_Reset(void);

/******************************************************************************
 * Function       : modbus_new_crc_ok
 * Description       : Check whether the CRC at the end of the received frame is valid.
 * Input       : buf,len
 * Return         : 1 valid, 0 invalid
 * Date            : 2026.04.30
 ******************************************************************************/
static u8 modbus_new_crc_ok(u8 *buf, u8 len) //20260430-0 add
{
    u16 crc;

    if (len < 4)
    {
        return 0;
    }

    crc = BF006_Modbus_CRC(buf, len - 2);
    if ((buf[len - 2] == (u8)(crc & 0x00ff)) && (buf[len - 1] == (u8)(crc >> 8)))
    {
        return 1;
    }

    return 0;
}

/******************************************************************************
 * Function       : modbus_new_send_with_crc
 * Description       : Append CRC to the TX buffer and send it.
 * Input       : buf,len
 * Return         : None
 * Date            : 2026.04.30
 ******************************************************************************/
static void modbus_new_send_with_crc(u8 *buf, u8 len) //20260430-0 add
{
    u16 crc;

    crc = BF006_Modbus_CRC(buf, len);
    buf[len] = (u8)(crc & 0x00ff);
    buf[len + 1] = (u8)(crc >> 8);
    M_Uart_Send_Block(buf, len + 2);
}

/******************************************************************************
 * Function       : modbus_new_send_err
 * Description       : Return an exception response for the new protocol.
 * Input       : adr,fun,err
 * Return         : None
 * Date            : 2026.04.30
 ******************************************************************************/
static void modbus_new_send_err(u8 adr, u8 fun, u8 err) //20260430-0 add
{
    u8 send_buf[5];

    send_buf[0] = adr;
    send_buf[1] = fun | 0x80;
    send_buf[2] = err;
    modbus_new_send_with_crc(send_buf, 3);
}

/******************************************************************************
 * Function       : modbus_new_get_u16
 * Description       : Read one U16 from a big-endian data area.
 * Input       : buf,index
 * Return         : value
 * Date            : 2026.04.30
 ******************************************************************************/
static u16 modbus_new_get_u16(u8 *buf, u8 index) //20260430-0 add
{
    u16 value;

    value = buf[index];
    value = (value << 8) + buf[index + 1];

    return value;
}

/******************************************************************************
 * Function       : modbus_new_put_u16
 * Description       : Write one U16 to the TX buffer in big-endian order.
 * Input       : buf,index,value
 * Return         : None
 * Date            : 2026.04.30
 ******************************************************************************/
static void modbus_new_put_u16(u8 *buf, u8 index, u16 value) //20260430-0 add
{
    buf[index] = (u8)(value >> 8);
    buf[index + 1] = (u8)(value & 0x00ff);
}

/******************************************************************************
 * Function       : modbus_new_sensor_max
 * Description       : Get the upper limit used when writing sensor thresholds.
 * Input       : None
 * Return         : max_value
 * Date            : 2026.04.30
 ******************************************************************************/
static u16 modbus_new_sensor_max(void) //20260430-1 change
{
    u16 max_value;

    max_value = config.Sensor_Rang;
    if (max_value == 0)
    {
        max_value = 9999;
    }

    return max_value;
}

/******************************************************************************
 * Function       : modbus_new_is_readonly_param
 * Description       : Check whether the parameter register is a read-only total-time parameter.
 * Input       : addr
 * Return         : 1 read-only, 0 writable
 * Date            : 2026.04.30
 ******************************************************************************/
static u8 modbus_new_is_readonly_param(u16 addr) //20260430-3 add
{
    if ((addr == 0x0015) || (addr == 0x0016))
    {
        return 1;
    }

    return 0;
}

/******************************************************************************
 * Function       : modbus_new_check_param
 * Description       : Check whether the new parameter value is within protocol and menu limits.
 * Input       : addr,value
 * Return         : 1 valid, 0 invalid
 * Date            : 2026.04.30
 ******************************************************************************/
static u8 modbus_new_check_param(u16 addr, u16 value) //20260430-3 change
{
    u16 sensor_max;

    sensor_max = modbus_new_sensor_max();
    switch (addr)
    {
    case 0x0000:
    case 0x0001:
        if ((value >= Value_UVP) && (value <= Value_OVP))
        {
            return 1;
        }
        break;
    case 0x0002:
    case 0x0003:
    case 0x0004:
    case 0x0005:
        if (value <= Limit_Current)
        {
            return 1;
        }
        break;
    case 0x0006:
    case 0x0007:
    case 0x0008:
    case 0x0009:
        if (value <= 9999)
        {
            return 1;
        }
        break;
    case 0x000a:
        if (value <= 999)
        {
            return 1;
        }
        break;
    case 0x000b:
        if ((value >= 1) && (value <= 254))
        {
            return 1;
        }
        break;
    case 0x000c:
        if (value <= 2)
        {
            return 1;
        }
        break;
    case 0x000d:
        if (value <= 1)
        {
            return 1;
        }
        break;
    case 0x000e:
        if (value <= 9999)
        {
            return 1;
        }
        break;
    case 0x000f:
    case 0x0010:
    case 0x0011:
        if (value <= sensor_max)
        {
            return 1;
        }
        break;
    case 0x0012:
        if (value <= 4)
        {
            return 1;
        }
        break;
    case 0x0013:
        if (value <= 4)
        {
            return 1;
        }
        break;
    case 0x0014:
        if (value <= sensor_max)
        {
            return 1;
        }
        break;
    default:
        break;
    }

    return 0;
}

/******************************************************************************
 * Function       : modbus_new_apply_run_mode
 * Description       : Write the run mode and reset timer counters when entering cycle-timer mode.
 * Input       : value
 * Return         : None
 * Date            : 2026.04.30
 ******************************************************************************/
static void modbus_new_apply_run_mode(u16 value) //20260430-0 add
{
    config.Hand_Auto_timer_Flag = (u8)value;
    if (config.Hand_Auto_timer_Flag == RUNMODE_TIMER)
    {
        sys.autoTimerFlag = 1;
        sys.Auto_timer_count = 0;
    }
}

/******************************************************************************
 * Function       : modbus_new_write_param
 * Description       : Write a valid parameter value to the BF006 configuration structure.
 * Input       : addr,value
 * Return         : None
 * Date            : 2026.04.30
 ******************************************************************************/
static void modbus_new_write_param(u16 addr, u16 value) //20260430-3 change
{
    switch (addr)
    {
    case 0x0000:
        config.OVP_Value = value;                    // P-00 over-voltage protection
        break;
    case 0x0001:
        config.UVP_Value = value;                    // P-01 under-voltage protection
        break;
    case 0x0002:
        config.Set_STANDBY_Current1 = value;         // P-02 pump A dry-run protection
        break;
    case 0x0003:
        config.Set_OVERLOAD_Current1 = value;        // P-03 pump A over-current protection
        break;
    case 0x0004:
        config.Set_STANDBY_Current2 = value;         // P-04 pump B dry-run protection
        break;
    case 0x0005:
        config.Set_OVERLOAD_Current2 = value;        // P-05 pump B over-current protection
        break;
    case 0x0006:
        config.AUTO_Timing_Run = value;              // P-06 cycle run time
        break;
    case 0x0007:
        config.AUTO_Timing_Stop = value;             // P-07 cycle stop time
        break;
    case 0x0008:
        config.Timer_Standby_Restart = value;        // P-08 dry-run auto-restart time
        break;
    case 0x0009:
        config.Timer_Unload_Restart = value;         // P-09 overload restart time
        break;
    case 0x000a:
        config.AUTO_FANGXIU = value;                 // P-10 anti-rust auto-start interval
        break;
    case 0x000b:
        config.Dev_Address = (u8)value;              // P-11 RS485 address
        break;
    case 0x000c:
        config.Pump_AB_Mode = (u8)value;             // P-12 dual-pump mode selection
        break;
    case 0x000d:
        config.Sensor_Mode = (u8)value;              // P-13 sensor mode selection
        break;
    case 0x000e:
        config.Sensor_Rang = value;                 // P-14 sensor range
        updatSysdata();
        break;
    case 0x000f:
        config.Sensor_STOP = value;                 // P-15 sensor stop level
        updatSysdata();
        break;
    case 0x0010:
        config.Sensor_ON1 = value;                  // P-16 sensor start level
        updatSysdata();
        break;
    case 0x0011:
        config.Sensor_ON2 = value;                  // P-17 dual-pump start level
        updatSysdata();
        break;
    case 0x0012:
        config.Auto_Mode = (u8)value;                // P-18 sensor type selection
        break;
    case 0x0013:
        config.fault_mask = (u8)value;               // P-19 high-speed deep-well pump inverter option
        break;
    case 0x0014:
        config.Sensor_overFlow = value;              // P-20 sensor overflow alarm value
        break;
    default:
        break;
    }

    sys.save_flag = 1;
}

/******************************************************************************
 * Function       : modbus_new_run_status
 * Description       : Build the run-status register according to protocol bit definitions.
 * Input       : None
 * Return         : status
 * Date            : 2026.04.30
 ******************************************************************************/
static u16 modbus_new_run_status(void) //20260430-0 add
{
    u16 status;

    status = 0;
    if (config.Hand_Auto_timer_Flag == RUNMODE_MANUAL)
    {
        status |= 0x0001;
    }
    if (config.Hand_Auto_timer_Flag > RUNMODE_MANUAL)
    {
        status |= 0x0002;
    }
    if (config.Hand_Auto_timer_Flag == RUNMODE_AUTO)
    {
        status |= 0x0004;
    }
    if (config.Hand_Auto_timer_Flag == RUNMODE_TIMER)
    {
        status |= 0x0008;
    }
    if (sys.Pump_A_state != 0)
    {
        status |= 0x0010;
    }
    if (sys.Pump_B_state != 0)
    {
        status |= 0x0020;
    }
    if (config.Pump_AB_Mode == 0)
    {
        status |= 0x0040;
    }
    if (config.Pump_AB_Mode == 1)
    {
        status |= 0x0080;
    }
    if (config.Pump_AB_Mode == 2)
    {
        status |= 0x0100;
    }
    if (sys.shang_shuiwei == 0)
    {
        status |= 0x0200;
    }
    else if (sys.shang_shuiwei == 1)
    {
        status |= 0x0400;
    }
    else if (sys.shang_shuiwei == 2)
    {
        status |= 0x0800;
    }
    if (sys.xia_shuiwei == 0)
    {
        status |= 0x1000;
    }
    else if (sys.xia_shuiwei == 1)
    {
        status |= 0x2000;
    }
    else if (sys.xia_shuiwei == 2)
    {
        status |= 0x4000;
    }
#if 0 //20260729-6 del 加泵位已改为 A 泵过载输入，不再上报原加泵请求状态
    if (sys.Pump_Add_Flag != 0)
    {
        status |= 0x8000;
    }
#endif

    return status;
}

/******************************************************************************
 * Function       : modbus_new_fault_status
 * Description       : Build the fault-summary register according to protocol bit definitions.
 * Input       : None
 * Return         : status
 * Date            : 2026.04.30
 ******************************************************************************/
static u16 modbus_new_fault_status(void) //20260430-0 add
{
    u16 status;

    status = 0;
    if ((sys.Fault_Error & ((u32)1 << ERR_UVP)) != 0)
    {
        status |= 0x0001;
    }
    if ((sys.Fault_Error & ((u32)1 << ERR_OVP)) != 0)
    {
        status |= 0x0002;
    }
    if ((sys.Fault_Error & (((u32)1 << ERR_OVERLOAD1) | ((u32)1 << ERR_OVERLOAD2))) != 0)
    {
        status |= 0x0004;
    }
    if ((sys.Fault_Error & (((u32)1 << ERR_STANDBY1) | ((u32)1 << ERR_STANDBY2) | ((u32)1 << ERR_NOLOAD_A) | ((u32)1 << ERR_NOLOAD_B))) != 0)
    {
        status |= 0x0008;
    }
    if ((sys.Fault_Error & (((u32)1 << ERR_LOSE_A1) | ((u32)1 << ERR_LOSE_B1) | ((u32)1 << ERR_LOSE_C1) | ((u32)1 << ERR_LOSE_A2) | ((u32)1 << ERR_LOSE_B2) | ((u32)1 << ERR_LOSE_C2))) != 0)
    {
        status |= 0x0020;
    }
    if (sys.shang_shuiwei == 2)
    {
        status |= 0x0040;
    }
    if (sys.xia_shuiwei == 0)
    {
        status |= 0x0100;
    }

    return status;
}

/******************************************************************************
 * Function       : modbus_new_read_param
 * Description       : Read a register in the new parameter area.
 * Input       : addr,value
 * Return         : 1 success, 0 invalid address
 * Date            : 2026.04.30
 ******************************************************************************/
static u8 modbus_new_read_param(u16 addr, u16 *value) //20260430-3 change
{
    switch (addr)
    {
    case 0x0000:
        *value = config.OVP_Value;                   // P-00 over-voltage protection
        break;
    case 0x0001:
        *value = config.UVP_Value;                   // P-01 under-voltage protection
        break;
    case 0x0002:
        *value = config.Set_STANDBY_Current1;        // P-02 pump A dry-run protection
        break;
    case 0x0003:
        *value = config.Set_OVERLOAD_Current1;       // P-03 pump A over-current protection
        break;
    case 0x0004:
        *value = config.Set_STANDBY_Current2;        // P-04 pump B dry-run protection
        break;
    case 0x0005:
        *value = config.Set_OVERLOAD_Current2;       // P-05 pump B over-current protection
        break;
    case 0x0006:
        *value = config.AUTO_Timing_Run;             // P-06 cycle run time
        break;
    case 0x0007:
        *value = config.AUTO_Timing_Stop;            // P-07 cycle stop time
        break;
    case 0x0008:
        *value = config.Timer_Standby_Restart;       // P-08 dry-run auto-restart time
        break;
    case 0x0009:
        *value = config.Timer_Unload_Restart;        // P-09 overload restart time
        break;
    case 0x000a:
        *value = config.AUTO_FANGXIU;                // P-10 anti-rust auto-start interval
        break;
    case 0x000b:
        *value = config.Dev_Address;                 // P-11 RS485 address
        break;
    case 0x000c:
        *value = config.Pump_AB_Mode;                // P-12 dual-pump mode selection
        break;
    case 0x000d:
        *value = config.Sensor_Mode;                 // P-13 sensor mode selection
        break;
    case 0x000e:
        *value = config.Sensor_Rang;                // P-14 sensor range
        break;
    case 0x000f:
        *value = config.Sensor_STOP;                // P-15 sensor stop level
        break;
    case 0x0010:
        *value = config.Sensor_ON1;                 // P-16 sensor start level
        break;
    case 0x0011:
        *value = config.Sensor_ON2;                 // P-17 dual-pump start level
        break;
    case 0x0012:
        *value = config.Auto_Mode;                   // P-18 sensor type selection
        break;
    case 0x0013:
        *value = config.fault_mask;                  // P-19 high-speed deep-well pump inverter option
        break;
    case 0x0014:
        *value = config.Sensor_overFlow;             // P-20 sensor overflow alarm value
        break;
    case 0x0015:                                      //20260430-3 change
        *value = config.Total_Run_Hour;               // P-21 total run time; unit is minutes in debug mode
        break;
    case 0x0016:                                      //20260430-3 change
        *value = config.Total_Power_Day;              // P-22 total power-on time; unit is minutes in debug mode
        break;
    default:
        return 0;
    }

    return 1;
}

/******************************************************************************
 * 函数名称       : modbus_new_read_monitor
 * 功能描述       : 读取监控状态区寄存器，包括传感器、水池显示和完整故障状态。
 * 输入参数       : addr,value
 * 返回值         : 1表示读取成功，0表示寄存器地址无效
 * 日期           : 2026.07.13
 ******************************************************************************/
static u8 modbus_new_read_monitor(u16 addr, u16 *value) //20260713-0 change
{
    switch (addr)
    {
    case 0x2100:
        *value = BF006_Fault_Code(sys.Fault_Error);
        break;
    case 0x2101:
        *value = modbus_new_run_status();
        break;
    case 0x2102:
        *value = modbus_new_fault_status();
        break;
    case 0x2103:
        *value = sys.Voltage_Line;
        break;
    case 0x2104:
        *value = sys.Current_A1;
        break;
    case 0x2105:
        *value = sys.Current_A2;
        break;
    case 0x2106:
        *value = 0;
        break;
    case 0x2107:
        *value = config.Sensor_ON2;
        break;
    case 0x2108:
        *value = config.Sensor_STOP;
        break;
    case 0x2109:
#if (Model_OUT == Model_1C1)
        *value = Sensor_Legacy_Reading(); //20261114-1 change 使用完整工程值，超界返回无效码
#else
        *value = sys.SensorOut;
#endif
        break;
    case 0x210a:
        *value = sys.shang_shuiwei;
        break;
    case 0x210b:
        *value = sys.xia_shuiwei;
        break;
    case 0x210c:
        *value = (sys.Pump_A_state ? 0x0001 : 0x0000) | (sys.Pump_B_state ? 0x0002 : 0x0000) | (sys.Fault_A_Flag ? 0x0100 : 0x0000) | (sys.Fault_B_Flag ? 0x0200 : 0x0000);
        break;
    case 0x210d:
        *value = Dev_Model;
        *value = (*value << 8) + BF006_Device_Spec();
        break;
    case 0x210e:
        *value = MODBUS_NEW_VERSION;
        break;
    case 0x210f: //20260713-0 add 当前运行使用的传感器类型，对应P-18的0至4选项
        *value = sys.Auto_Mode;
        break;
    case 0x2110: //20261114-1 change 连续传感器数值放大一百倍，无有效读数或超界时返回无效码
#if (Model_OUT == Model_1C1)
        *value = Sensor_Legacy_Reading();
#else
        if ((sys.Auto_Mode >= SENSORTYPE_WATER4_20MA) && (sys.Auto_Mode <= SENSORTYPE_PRESS0_5V))
        {
            *value = sys.SensorOut;
        }
        else
        {
            *value = MODBUS_NEW_SENSOR_VALUE_INVALID;
        }
#endif
        break;
    case 0x2111: //20260713-0 add 当前连续传感器数值的单位代码
        if (sys.Auto_Mode == SENSORTYPE_WATER4_20MA)
        {
            *value = MODBUS_NEW_SENSOR_UNIT_METER;
        }
        else if ((sys.Auto_Mode == SENSORTYPE_PRESS4_20MA) || (sys.Auto_Mode == SENSORTYPE_PRESS0_5V))
        {
            *value = MODBUS_NEW_SENSOR_UNIT_MPA;
        }
        else
        {
            *value = MODBUS_NEW_SENSOR_UNIT_NONE;
        }
        break;
    case 0x2112: //20260713-0 add 当前传感器作用对象，0表示上水池，1表示下水池
        *value = config.Sensor_Mode;
        break;
    case 0x2113: //20260713-0 add 模拟量水池档位，0低、1中、2高，开关量类型返回无效值
        if ((sys.Auto_Mode >= SENSORTYPE_WATER4_20MA) && (sys.Auto_Mode <= SENSORTYPE_PRESS0_5V))
        {
            *value = sys.SensorLever;
        }
        else
        {
            *value = MODBUS_NEW_SENSOR_LEVEL_INVALID;
        }
        break;
    case 0x2114: //20260713-0 add 上水池六段显示位图，位0至位5依次对应第一至第六段
        *value = (LCD_WATER_Hlevel1 ? 0x0001 : 0x0000) |
                 (LCD_WATER_Hlevel2 ? 0x0002 : 0x0000) |
                 (LCD_WATER_Hlevel3 ? 0x0004 : 0x0000) |
                 (LCD_WATER_Hlevel4 ? 0x0008 : 0x0000) |
                 (LCD_WATER_Hlevel5 ? 0x0010 : 0x0000) |
                 (LCD_WATER_Hlevel6 ? 0x0020 : 0x0000);
        break;
    case 0x2115: //20260713-0 add 下水池六段显示位图，位0至位5依次对应第一至第六段
        *value = (LCD_WATER_Llevel1 ? 0x0001 : 0x0000) |
                 (LCD_WATER_Llevel2 ? 0x0002 : 0x0000) |
                 (LCD_WATER_Llevel3 ? 0x0004 : 0x0000) |
                 (LCD_WATER_Llevel4 ? 0x0008 : 0x0000) |
                 (LCD_WATER_Llevel5 ? 0x0010 : 0x0000) |
                 (LCD_WATER_Llevel6 ? 0x0020 : 0x0000);
        break;
    case 0x2116: //20260713-0 add 传感器异常、溢流和水池闪烁状态位图
        *value = (sys.sensor_err ? MODBUS_NEW_SENSOR_STATUS_ERROR : 0x0000) |
                 (sys.Sensor_OverFlow ? MODBUS_NEW_SENSOR_STATUS_OVERFLOW : 0x0000) |
                 (sys.flowover ? MODBUS_NEW_SENSOR_STATUS_FLOWOVER : 0x0000) |
                 ((Dis7_Flash == 1) ? MODBUS_NEW_SENSOR_STATUS_UPPER_FLASH : 0x0000) |
                 ((Dis8_Flash == 1) ? MODBUS_NEW_SENSOR_STATUS_LOWER_FLASH : 0x0000);
        break;
    case 0x2117: //20260713-0 add 完整故障位图低十六位
        *value = (u16)(sys.Fault_Error & 0xffff);
        break;
    case 0x2118: //20260713-0 add 完整故障位图高十六位
        *value = (u16)(sys.Fault_Error >> 16);
        break;
    default:
        return 0;
    }

    return 1;
}

/******************************************************************************
 * Function       : modbus_new_read_history
 * Description       : Read a register in the new fault-history area.
 * Input       : addr,value
 * Return         : 1 success, 0 invalid address
 * Date            : 2026.04.30
 ******************************************************************************/
static u8 modbus_new_read_history(u16 addr, u16 *value) //20260430-0 add
{
    u16 offset;
    u8 index;
    u8 field;

    if ((addr < MODBUS_NEW_HIS_START) || (addr > MODBUS_NEW_HIS_END))
    {
        return 0;
    }

    offset = addr - MODBUS_NEW_HIS_START;
    index = (u8)(offset / 3);
    field = (u8)(offset % 3);
    if (index >= 10)
    {
        return 0;
    }

    if (field == 0)
    {
        *value = config.Record_Type[index];
    }
    else if (field == 1)
    {
        *value = config.Record_Dat1[index];
    }
    else
    {
        *value = config.Record_Dat2[index];
    }

    return 1;
}

/******************************************************************************
 * Function       : modbus_new_read_reg
 * Description       : Read one register according to the new register partitions.
 * Input       : addr,value
 * Return         : 1 success, 0 invalid address
 * Date            : 2026.04.30
 ******************************************************************************/
static u8 modbus_new_read_reg(u16 addr, u16 *value) //20260430-0 add
{
    if ((addr >= MODBUS_NEW_PARAM_START) && (addr <= MODBUS_NEW_PARAM_END))
    {
        return modbus_new_read_param(addr, value);
    }
    if ((addr >= MODBUS_NEW_MON_START) && (addr <= MODBUS_NEW_MON_END))
    {
        return modbus_new_read_monitor(addr, value);
    }
    if ((addr >= MODBUS_NEW_HIS_START) && (addr <= MODBUS_NEW_HIS_END))
    {
        return modbus_new_read_history(addr, value);
    }

    return 0;
}

/******************************************************************************
 * Function       : modbus_new_pump_control
 * Description       : Execute the 2000H pump start/stop control command.
 * Input       : value
 * Return         : 1 executed, 0 invalid data or site condition not allowed
 * Date            : 2026.04.30
 ******************************************************************************/
static u8 modbus_new_pump_control(u16 value) //20260430-0 add
{
    u8 action;
    u8 target;
    u8 need_a;
    u8 need_b;

    action = (u8)(value & 0x0003);
    target = (u8)((value >> 4) & 0x0003);
    need_a = 0;
    need_b = 0;

    if (action == MODBUS_NEW_PUMP_ACTION_NONE)
    {
        return 1;
    }
    if ((action != MODBUS_NEW_PUMP_ACTION_STOP) && (action != MODBUS_NEW_PUMP_ACTION_START))
    {
        return 0;
    }
    if (target == MODBUS_NEW_PUMP_TARGET_A)
    {
        need_a = 1;
    }
    else if (target == MODBUS_NEW_PUMP_TARGET_B)
    {
        need_b = 1;
    }
    else if (target == MODBUS_NEW_PUMP_TARGET_AB)
    {
        need_a = 1;
        need_b = 1;
    }
    else
    {
        return 0;
    }

    Speak_Con(0);
    if (action == MODBUS_NEW_PUMP_ACTION_START)
    {
        if (config.Hand_Auto_timer_Flag != RUNMODE_MANUAL)
        {
            return 0;
        }
        if ((need_a != 0) && (sys.Fault_A_Flag != 0))
        {
            return 0;
        }
        if ((need_b != 0) && (sys.Fault_B_Flag != 0))
        {
            return 0;
        }
        if (need_a != 0)
        {
            sys.Pump_A_ON = 1;
        }
        if (need_b != 0)
        {
            sys.Pump_B_ON = 1;
        }
    }
    else
    {
        if (need_a != 0)
        {
            sys.Pump_A_ON = 0;
        }
        if (need_b != 0)
        {
            sys.Pump_B_ON = 0;
        }
    }

    return 1;
}

/******************************************************************************
 * Function       : modbus_new_control
 * Description       : Execute a write command in the new control-command area.
 * Input       : addr,value
 * Return         : 1 executed, 0 invalid data
 * Date            : 2026.04.30
 ******************************************************************************/
static u8 modbus_new_control(u16 addr, u16 value) //20260430-0 add
{
    u8 i;

    switch (addr)
    {
    case 0x2000:
        return modbus_new_pump_control(value);
    case 0x2003:
        if (value <= RUNMODE_TIMER)
        {
            Speak_Con(0);
            modbus_new_apply_run_mode(value);
            sys.save_flag = 1;
            return 1;
        }
        break;
    case 0x2004:
        if (value <= 2)
        {
            Speak_Con(0);
            config.Pump_AB_Mode = (u8)value;
            sys.save_flag = 1;
            return 1;
        }
        break;
    case 0x2005:
        if (value == 0x0001)
        {
            Speak_Con(0);
            clear_A_Fault();
            clear_B_Fault();
            return 1;
        }
        break;
    case 0x2006:
        if (value == 0x0000)
        {
            Speak_Con(0);
            for (i = 0; i < 10; i++)
            {
                config.Record_Type[i] = 0;
                config.Record_Dat1[i] = 0;
                config.Record_Dat2[i] = 0;
            }
            sys.save_flag = 1;
            Record_Clr_and_Reset();
            return 1;
        }
        break;
    case 0x2007:
        if (value == 0x5aa5)
        {
            sys.save_flag = 1;
            return 1;
        }
        break;
    default:
        break;
    }

    return 0;
}

/******************************************************************************
 * Function       : modbus_new_handle_read
 * Description       : Handle the 03H read holding-register command.
 * Input       : None
 * Return         : None
 * Date            : 2026.04.30
 ******************************************************************************/
static void modbus_new_handle_read(void) //20260430-0 add
{
    u8 send_buf[3 + MODBUS_NEW_MAX_WORDS * 2 + 2];
    u8 i;
    u8 pos;
    u16 start_addr;
    u16 qty;
    u16 value;

    if (RecData.Rec_Len != 8)
    {
        modbus_new_send_err(RecData.Rec_Data[0], MODBUS_NEW_FUNC_READ, MODBUS_NEW_ERR_DATA);
        return;
    }

    start_addr = modbus_new_get_u16(RecData.Rec_Data, 2);
    qty = modbus_new_get_u16(RecData.Rec_Data, 4);
    if ((qty == 0) || (qty > MODBUS_NEW_MAX_WORDS))
    {
        modbus_new_send_err(RecData.Rec_Data[0], MODBUS_NEW_FUNC_READ, MODBUS_NEW_ERR_DATA);
        return;
    }

    send_buf[0] = RecData.Rec_Data[0];
    send_buf[1] = MODBUS_NEW_FUNC_READ;
    send_buf[2] = (u8)(qty * 2);
    pos = 3;
    for (i = 0; i < qty; i++)
    {
        if (modbus_new_read_reg(start_addr + i, &value) == 0)
        {
            modbus_new_send_err(RecData.Rec_Data[0], MODBUS_NEW_FUNC_READ, MODBUS_NEW_ERR_REG);
            return;
        }
        modbus_new_put_u16(send_buf, pos, value);
        pos += 2;
    }

    modbus_new_send_with_crc(send_buf, pos);
}

/******************************************************************************
 * Function       : modbus_new_handle_write_one
 * Description       : Handle the 06H write-single-register command.
 * Input       : None
 * Return         : None
 * Date            : 2026.04.30
 ******************************************************************************/
static void modbus_new_handle_write_one(void) //20260430-0 add
{
    u16 addr;
    u16 value;

    if (RecData.Rec_Len != 8)
    {
        modbus_new_send_err(RecData.Rec_Data[0], MODBUS_NEW_FUNC_WRITE_ONE, MODBUS_NEW_ERR_DATA);
        return;
    }

    addr = modbus_new_get_u16(RecData.Rec_Data, 2);
    value = modbus_new_get_u16(RecData.Rec_Data, 4);
    if ((addr >= MODBUS_NEW_PARAM_START) && (addr <= MODBUS_NEW_PARAM_END))
    {
        if (modbus_new_is_readonly_param(addr) != 0) //20260430-3 add total-time parameters are read-only; block 06H writes
        {
            modbus_new_send_err(RecData.Rec_Data[0], MODBUS_NEW_FUNC_WRITE_ONE, MODBUS_NEW_ERR_REG);
            return;
        }
        if (modbus_new_check_param(addr, value) == 0)
        {
            modbus_new_send_err(RecData.Rec_Data[0], MODBUS_NEW_FUNC_WRITE_ONE, MODBUS_NEW_ERR_DATA);
            return;
        }
        modbus_new_write_param(addr, value);
        M_Uart_Send_Block(RecData.Rec_Data, RecData.Rec_Len);
        return;
    }

    if ((addr >= MODBUS_NEW_CTRL_START) && (addr <= MODBUS_NEW_CTRL_END))
    {
        if (modbus_new_control(addr, value) == 0)
        {
            modbus_new_send_err(RecData.Rec_Data[0], MODBUS_NEW_FUNC_WRITE_ONE, MODBUS_NEW_ERR_DATA);
            return;
        }
        M_Uart_Send_Block(RecData.Rec_Data, RecData.Rec_Len);
        return;
    }

    modbus_new_send_err(RecData.Rec_Data[0], MODBUS_NEW_FUNC_WRITE_ONE, MODBUS_NEW_ERR_REG);
}

/******************************************************************************
 * Function       : modbus_new_handle_write_more
 * Description       : Handle the 10H write-multiple-parameter-registers command.
 * Input       : None
 * Return         : None
 * Date            : 2026.04.30
 ******************************************************************************/
static void modbus_new_handle_write_more(void) //20260430-0 add
{
    u8 send_buf[8];
    u8 i;
    u8 byte_count;
    u16 start_addr;
    u16 qty;
    u16 value;

    if (RecData.Rec_Len < 11)
    {
        modbus_new_send_err(RecData.Rec_Data[0], MODBUS_NEW_FUNC_WRITE_MORE, MODBUS_NEW_ERR_DATA);
        return;
    }

    start_addr = modbus_new_get_u16(RecData.Rec_Data, 2);
    qty = modbus_new_get_u16(RecData.Rec_Data, 4);
    byte_count = RecData.Rec_Data[6];
    if ((qty == 0) || (qty > MODBUS_NEW_MAX_WORDS) || (byte_count != (u8)(qty * 2)) || (RecData.Rec_Len != (u8)(byte_count + 9)))
    {
        modbus_new_send_err(RecData.Rec_Data[0], MODBUS_NEW_FUNC_WRITE_MORE, MODBUS_NEW_ERR_DATA);
        return;
    }

    for (i = 0; i < qty; i++)
    {
        value = modbus_new_get_u16(RecData.Rec_Data, (u8)(7 + i * 2));
        if ((start_addr + i < MODBUS_NEW_PARAM_START) || (start_addr + i > MODBUS_NEW_PARAM_END))
        {
            modbus_new_send_err(RecData.Rec_Data[0], MODBUS_NEW_FUNC_WRITE_MORE, MODBUS_NEW_ERR_REG);
            return;
        }
        if (modbus_new_is_readonly_param(start_addr + i) != 0) //20260430-3 add total-time parameters are read-only; block 10H writes
        {
            modbus_new_send_err(RecData.Rec_Data[0], MODBUS_NEW_FUNC_WRITE_MORE, MODBUS_NEW_ERR_REG);
            return;
        }
        if (modbus_new_check_param(start_addr + i, value) == 0)
        {
            modbus_new_send_err(RecData.Rec_Data[0], MODBUS_NEW_FUNC_WRITE_MORE, MODBUS_NEW_ERR_DATA);
            return;
        }
    }

    for (i = 0; i < qty; i++)
    {
        value = modbus_new_get_u16(RecData.Rec_Data, (u8)(7 + i * 2));
        modbus_new_write_param(start_addr + i, value);
    }

    send_buf[0] = RecData.Rec_Data[0];
    send_buf[1] = MODBUS_NEW_FUNC_WRITE_MORE;
    modbus_new_put_u16(send_buf, 2, start_addr);
    modbus_new_put_u16(send_buf, 4, qty);
    modbus_new_send_with_crc(send_buf, 6);
}

/******************************************************************************
 * Function       : modbus_new_broadcast_read_addr
 * Description       : Handle the private broadcast read-address command.
 * Input       : None
 * Return         : None
 * Date            : 2026.04.30
 ******************************************************************************/
static void modbus_new_broadcast_read_addr(void) //20260430-0 add
{
    u8 send_buf[7];

    send_buf[0] = config.Dev_Address;
    send_buf[1] = MODBUS_NEW_FUNC_READ;
    send_buf[2] = 0x02;
    send_buf[3] = config.Dev_Address;
    send_buf[4] = 0x00;
    modbus_new_send_with_crc(send_buf, 5);
}

/******************************************************************************
 * Function       : modbus_new_broadcast_set_addr
 * Description       : Handle the FFH 41H private broadcast set-address command.
 * Input       : None
 * Return         : None
 * Date            : 2026.04.30
 ******************************************************************************/
static void modbus_new_broadcast_set_addr(void) //20260430-0 add
{
    u16 fixed_addr;
    u8 new_addr;

    if (RecData.Rec_Len != 8)
    {
        modbus_new_send_err(MODBUS_NEW_ADDR_BROADCAST, MODBUS_NEW_FUNC_SET_ADDR, MODBUS_NEW_ERR_DATA);
        return;
    }

    fixed_addr = modbus_new_get_u16(RecData.Rec_Data, 2);
    new_addr = RecData.Rec_Data[4];
    if (fixed_addr != MODBUS_NEW_PRIVATE_REG)
    {
        modbus_new_send_err(MODBUS_NEW_ADDR_BROADCAST, MODBUS_NEW_FUNC_SET_ADDR, MODBUS_NEW_ERR_REG);
        return;
    }
    if ((new_addr < 1) || (new_addr > 254))
    {
        modbus_new_send_err(MODBUS_NEW_ADDR_BROADCAST, MODBUS_NEW_FUNC_SET_ADDR, MODBUS_NEW_ERR_DATA);
        return;
    }

    M_Uart_Send_Block(RecData.Rec_Data, RecData.Rec_Len);
    config.Dev_Address = new_addr;
    sys.save_flag = 1;
}

/******************************************************************************
 * Function       : modbus_new_private_clear_time
 * Description       : Handle the FFH 42H private total-time clear command.
 * Input       : None
 * Return         : None
 * Date            : 2026.05.01
 ******************************************************************************/
static void modbus_new_private_clear_time(void) //20260501-1 add
{
    u16 fixed_addr;
    u8 cmd;
    u8 confirm;

    if (RecData.Rec_Len != 8)
    {
        modbus_new_send_err(MODBUS_NEW_ADDR_BROADCAST, MODBUS_NEW_FUNC_CLEAR_TIME, MODBUS_NEW_ERR_DATA);
        return;
    }

    fixed_addr = modbus_new_get_u16(RecData.Rec_Data, 2);
    cmd = RecData.Rec_Data[4];
    confirm = RecData.Rec_Data[5];
    if (fixed_addr != MODBUS_NEW_PRIVATE_REG)
    {
        modbus_new_send_err(MODBUS_NEW_ADDR_BROADCAST, MODBUS_NEW_FUNC_CLEAR_TIME, MODBUS_NEW_ERR_REG);
        return;
    }
    if (confirm != MODBUS_NEW_PRIVATE_CONFIRM)
    {
        modbus_new_send_err(MODBUS_NEW_ADDR_BROADCAST, MODBUS_NEW_FUNC_CLEAR_TIME, MODBUS_NEW_ERR_DATA);
        return;
    }

    if (cmd == MODBUS_NEW_CLEAR_RUN_TIME)
    {
        Total_Run_Time_Clear();          // clear P-21 total run time and second counter
    }
    else if (cmd == MODBUS_NEW_CLEAR_POWER_TIME)
    {
        Total_Power_Time_Clear();        // clear P-22 total power-on time and second counter
    }
    else
    {
        modbus_new_send_err(MODBUS_NEW_ADDR_BROADCAST, MODBUS_NEW_FUNC_CLEAR_TIME, MODBUS_NEW_ERR_DATA);
        return;
    }

    M_Uart_Send_Block(RecData.Rec_Data, RecData.Rec_Len);
}

/******************************************************************************
 * Function       : modbus_new_handle_broadcast
 * Description       : Handle BF006 private broadcast commands.
 * Input       : None
 * Return         : None
 * Date            : 2026.04.30
 ******************************************************************************/
static void modbus_new_handle_broadcast(void) //20260430-0 add
{
    u16 fixed_addr;
    u16 qty;

    if (RecData.Rec_Data[1] == MODBUS_NEW_FUNC_SET_ADDR)
    {
        modbus_new_broadcast_set_addr();
        return;
    }

    if (RecData.Rec_Data[1] == MODBUS_NEW_FUNC_CLEAR_TIME) //20260501-1 add private total-time clear command
    {
        modbus_new_private_clear_time();
        return;
    }

    if (RecData.Rec_Data[1] == MODBUS_NEW_FUNC_READ)
    {
        if (RecData.Rec_Len != 8)
        {
            modbus_new_send_err(MODBUS_NEW_ADDR_BROADCAST, MODBUS_NEW_FUNC_READ, MODBUS_NEW_ERR_DATA);
            return;
        }
        fixed_addr = modbus_new_get_u16(RecData.Rec_Data, 2);
        qty = modbus_new_get_u16(RecData.Rec_Data, 4);
        if ((fixed_addr == MODBUS_NEW_PRIVATE_REG) && (qty == 1))
        {
            modbus_new_broadcast_read_addr();
        }
        else
        {
            modbus_new_send_err(MODBUS_NEW_ADDR_BROADCAST, MODBUS_NEW_FUNC_READ, MODBUS_NEW_ERR_REG);
        }
        return;
    }

    modbus_new_send_err(MODBUS_NEW_ADDR_BROADCAST, RecData.Rec_Data[1], MODBUS_NEW_ERR_FUNC);
}

/******************************************************************************
 * Function       : Modbus_Receive_handle
 * Description       : Receive one byte from the UART interrupt and cache it in the new protocol RX buffer.
 * Input       : rdata
 * Return         : None
 * Date            : 2026.04.30
 ******************************************************************************/
void Modbus_Receive_handle(u8 rdata) //20260430-0 add
{
    RecData.Rec_Fifo[RecData.Rec_Cnt] = rdata;
    RecData.Rec_Cnt++;
    if (RecData.Rec_Cnt >= Usart_Rec_Len)
    {
        RecData.Rec_Cnt = 0;
    }

    RecData.Rec_Idle_Time = 0;
    RecData.Rec_Start_Flag = 1;
}

/******************************************************************************
 * Function       : Modbus_Timer_handle
 * Description       : Use the 1 ms timer tick to detect the end of a serial frame.
 * Input       : None
 * Return         : None
 * Date            : 2026.04.30
 ******************************************************************************/
void Modbus_Timer_handle(void) //20260430-0 add
{
    if (RecData.Rec_Start_Flag == 1)
    {
        RecData.Rec_Idle_Time++;
        if (RecData.Rec_Idle_Time >= Usart_Idle_Time)
        {
            RecData.Rec_Start_Flag = 0;
            RecData.Rec_End_Flag = 1;
            RecData.Rec_Len = RecData.Rec_Cnt;
            RecData.Rec_Cnt = 0;

            memcpy(RecData.Rec_Data, RecData.Rec_Fifo, RecData.Rec_Len);
            memset(RecData.Rec_Fifo, 0, Usart_Rec_Len);
        }
    }
}

/******************************************************************************
 * Function       : Modbus_analysis
 * Description       : Parse and respond to one Modbus RTU frame using the new BF006 protocol.
 * Input       : None
 * Return         : None
 * Date            : 2026.04.30
 ******************************************************************************/
void Modbus_analysis(void) //20260430-0 add
{
    if (RecData.Rec_End_Flag != 1)
    {
        return;
    }

    RecData.Rec_End_Flag = 0;
    if (modbus_new_crc_ok(RecData.Rec_Data, RecData.Rec_Len) == 0)
    {
        return;
    }

    if (RecData.Rec_Data[0] == MODBUS_NEW_ADDR_BROADCAST)
    {
        modbus_new_handle_broadcast();
        return;
    }
    if (RecData.Rec_Data[0] != config.Dev_Address)
    {
        return;
    }

    switch (RecData.Rec_Data[1])
    {
    case MODBUS_NEW_FUNC_READ:
        modbus_new_handle_read();
        break;
    case MODBUS_NEW_FUNC_WRITE_ONE:
        modbus_new_handle_write_one();
        break;
    case MODBUS_NEW_FUNC_WRITE_MORE:
        modbus_new_handle_write_more();
        break;
    default:
        modbus_new_send_err(RecData.Rec_Data[0], RecData.Rec_Data[1], MODBUS_NEW_ERR_FUNC);
        break;
    }
}

#endif
#endif
