#include "configPar.h"
#include "eeprom.h"
#include "system.h"
#include "user.h"
//20261113-0 del 封存旧协议头文件引用：#include "updata4G.h"
#include "uart4.h" //20261116-4 add 避开串口四的应答发送时段。

#define BIAS_ADDR 0x0000 //20260729-13 change 恢复 7 月 14 日版本的参数单副本存储地址
#define BF4G_BOOT_LEFT  0x0200 //20261124-1 add 与参数区独立的第一个编号扇区。
#define BF4G_BOOT_RIGHT 0x0400 //20261124-1 add 与参数区独立的第二个编号扇区。
#define BAN_BEN 18 //20260430-3 change update version after adding total-time remainder fields

config_t config;
bit config_loaded; //20261116-1 add 参数读取及派生量初始化完成标志。

extern u16 Limit_Current;

static u8 Total_Run_Second_Cnt = 0;   //20260501-1 add run-time second counter, reset by the clear command
static u8 Total_Power_Second_Cnt = 0; //20260501-1 add power-on second counter, reset by the clear command

/******************************************************************************
 * 函数名称       : Par_ReadFaultBoot
 * 功能描述       : 读出并校验编号记录。
 * 输入参数       : address，编号扇区首地址；value，编号输出位置。
 * 返回值         : 读到完整记录时为一。
 * 日期            : 2026.11.24
 ******************************************************************************/
static bit Par_ReadFaultBoot(u16 address, u32 *value) //20261124-1 add
{
    u8 xdata record[12];
    u8 i;

    EEPROM_read_n(address, record, sizeof(record));
    if (record[0] != 0x42 || record[1] != 0x46 || record[2] != 0x30 || record[3] != 0x36) {
        return 0;                       // 全空白或其它数据不代表首次投用。
    }
    *value = 0;
    for (i = 0; i < 4; i++) {
        if ((record[4 + i] ^ record[8 + i]) != 0xff) {
            return 0;                   // 部分写入或损坏的记录不能用于续号。
        }
        *value = (*value << 8) | record[4 + i];
    }
    return 1;
}

/******************************************************************************
 * 函数名称       : Par_WriteFaultBoot
 * 功能描述       : 只擦写指定的编号扇区，保留另一份有效记录。
 * 输入参数       : address，编号扇区首地址；value，新编号。
 * 返回值         : None
 * 日期            : 2026.11.24
 ******************************************************************************/
static void Par_WriteFaultBoot(u16 address, u32 value) //20261124-1 add
{
    u8 xdata record[12];
    u8 i;

    record[0] = 0x42;
    record[1] = 0x46;
    record[2] = 0x30;
    record[3] = 0x36;
    for (i = 0; i < 4; i++) {
        record[7 - i] = (u8)value;
        record[11 - i] = (u8)~value;
        value >>= 8;
    }
    EEPROM_SectorErase(address);
    EEPROM_write_n(address, record, sizeof(record));
}

/******************************************************************************
 * 函数名称       : Par_AdvanceFaultBoot
 * 功能描述       : 读取两份编号并将下一编号保存到较旧的扇区。
 * 输入参数       : None
 * 返回值         : 可靠读回新编号时返回该编号，否则返回零。
 * 日期            : 2026.11.24
 ******************************************************************************/
u32 Par_AdvanceFaultBoot(void) //20261124-1 add
{
    u32 left;
    u32 right;
    u32 latest;
    u32 previous;
    u32 stored;
    u16 target;

    if (!Par_ReadFaultBoot(BF4G_BOOT_LEFT, &left) || !Par_ReadFaultBoot(BF4G_BOOT_RIGHT, &right)) {
        return 0;                      // 任一份失去可信内容时可能已丢失较新编号。
    }
    if (left >= right) {
        latest = left;
        previous = right;
        target = BF4G_BOOT_RIGHT;
    } else {
        latest = right;
        previous = left;
        target = BF4G_BOOT_LEFT;
    }
    if (latest == 0xffffffffUL || (latest != 0 && latest - previous != 1)) {
        return 0;                      // 仅接受两份零初值或连续的一对编号。
    }

    latest++;
    Par_WriteFaultBoot(target, latest);
    if (!Par_ReadFaultBoot(target, &stored) || stored != latest) {
        return 0;                      // 只有写后核对成功才允许首次发送。
    }
    return latest;
}

/******************************************************************************
 * 函数名称       : Par_Init
 * 功能描述       : 按 7 月 14 日版本从 EEPROM 固定地址读取参数。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.07.29
 ******************************************************************************/
void Par_Init(void) //20260729-13 change
{
    u8 i = 0;
    EEPROM_read_n(BIAS_ADDR, (u8 *)&config, sizeof(config_t)); // 使用老版本固定地址读取完整参数结构

    if (config.banben == 17) //20260430-3 add preserve existing parameters when upgrading from the placeholder version and only initialize new remainder fields
    {
        config.banben = BAN_BEN;
        config.Total_Run_Minute = 0;
        config.Total_Power_Hour = 0;
        config.Total_Power_Minute = 0;
        sys.save_flag = 1;
    }

    if (config.banben != BAN_BEN)
    {
        config.banben = BAN_BEN;
        config.AUTO_Timing_Run = 30;
        config.AUTO_Timing_Stop = 60;
        config.Timer_Standby_Restart = 30;
        config.Timer_Unload_Restart = 30;
        config.AUTO_FANGXIU = 99;
#if (VoltageValue == Vol_380)
        config.OVP_Value = Value_OVP;
        config.UVP_Value = 320;
#else
        config.OVP_Value = 260;
        config.UVP_Value = 150;
#endif
        config.Dev_type = 0;
        config.Set_OVERLOAD_Current1 = 100;
        config.Set_STANDBY_Current1 = 0;
        config.A_Turn = 0;
        config.Set_OVERLOAD_Current2 = 100;
        config.Set_STANDBY_Current2 = 0;
        config.B_Turn = 0;
        config.Hand_Auto_timer_Flag = RUNMODE_MANUAL;
        config.Pump_AB_Mode = 0;
        config.Dev_Address = 1;

        config.Sensor_Rang = 10;
        config.Sensor_overFlow = 10;
        config.Sensor_STOP = 9;
        config.Sensor_ON1 = 5;
        config.Sensor_ON2 = 1;
        config.Sensor_Mode = CTROLTYPE_PUMP;
        config.Lock1 = 0;
        config.Lock2 = 0;
        config.Auto_Mode = 0;
        config.fault_mask = 0;
        config.Total_Run_Hour = 0;     //20260430-3 change default total run time
        config.Total_Run_Minute = 0;   //20260430-3 add default run-time minute remainder
        config.Total_Power_Day = 0;    //20260430-3 change default total power-on time
        config.Total_Power_Hour = 0;   //20260430-3 add default power-on hour remainder
        config.Total_Power_Minute = 0; //20260430-3 add default power-on minute remainder

        for (i = 0; i < 5; i++)
        {
            config.RF_Family[i] = 0;
        }

        for (i = 0; i < 10; i++)
        {
            config.Record_Type[i] = ERR_NUMB;
            config.Record_Dat1[i] = 0;
            config.Record_Dat2[i] = 0;
        }

        config.run_fen = 0;
        config.run_xiaoshi = 0;
#if 0 //20261113-0 del 恢复默认参数时停用旧协议的全量上报。
        Send_dat.updataAll = 1;
        Send_dat.GuZhangJiLu = 1;
#endif
    }

    switch (config.Dev_type)
    {

    case 1:
        Limit_Current = V2_Limit_Current;
        break;
    case 2:
        Limit_Current = V3_Limit_Current;
        break;
    case 3:
        Limit_Current = V4_Limit_Current;
        break;
    case 4:
        Limit_Current = V5_Limit_Current;
        break;
    default:
        Limit_Current = V1_Limit_Current;
        break;
    }

    updatSysdata();
    config_loaded = 1; //20261116-1 add 读入参数并更新运行配置后才宣告可用。
}

/******************************************************************************
 * Function       : Total_Time_Second_Handle
 * Description       : Maintain P-21 total run time and P-22 total power-on time once per second.
 * Input       : pump_running
 * Return         : None
 * Date            : 2026.04.30
 ******************************************************************************/
void Total_Time_Second_Handle(u8 pump_running) //20260430-3 add
{
    Total_Power_Second_Cnt++;            // accumulate power-on time while the program is running
    if (Total_Power_Second_Cnt >= 60)
    {
        Total_Power_Second_Cnt = 0;
#if (TOTAL_TIME_DEBUG_MODE == 1)
        if (config.Total_Power_Day < 9999)
        {
            config.Total_Power_Day++;    // in debug mode, P-22 increments once per minute and uses minutes as the unit
            sys.save_flag = 1;
        }
#else
        if (config.Total_Power_Day < 9999)
        {
            config.Total_Power_Minute++; // in normal mode, save the minute remainder below one day
            if (config.Total_Power_Minute >= 60)
            {
                config.Total_Power_Minute = 0;
                config.Total_Power_Hour++;
                if (config.Total_Power_Hour >= 24)
                {
                    config.Total_Power_Hour = 0;
                    config.Total_Power_Day++;
                }
            }
            sys.save_flag = 1;
        }
#endif
    }

    if (pump_running == 0)
    {
        Total_Run_Second_Cnt = 0;        // do not accumulate run time when the pump is stopped; discard less than one minute
        return;
    }

    Total_Run_Second_Cnt++;
    if (Total_Run_Second_Cnt >= 60)
    {
        Total_Run_Second_Cnt = 0;
#if (TOTAL_TIME_DEBUG_MODE == 1)
        if (config.Total_Run_Hour < 9999)
        {
            config.Total_Run_Hour++;     // in debug mode, P-21 increments once per running minute and uses minutes as the unit
            sys.save_flag = 1;
        }
#else
        if (config.Total_Run_Hour < 9999)
        {
            config.Total_Run_Minute++;   // in normal mode, save the minute remainder below one hour
            if (config.Total_Run_Minute >= 60)
            {
                config.Total_Run_Minute = 0;
                config.Total_Run_Hour++;
            }
            sys.save_flag = 1;
        }
#endif
    }
}

/******************************************************************************
 * Function       : Total_Run_Time_Clear
 * Description       : Clear P-21 total run time and the temporary counter below one minute.
 * Input       : None
 * Return         : None
 * Date            : 2026.05.01
 ******************************************************************************/
void Total_Run_Time_Clear(void) //20260501-1 add
{
    Total_Run_Second_Cnt = 0;
    config.Total_Run_Hour = 0;
    config.Total_Run_Minute = 0;
    sys.save_flag = 1;                  // write to EEPROM through the original save flow after clearing
}

/******************************************************************************
 * Function       : Total_Power_Time_Clear
 * Description       : Clear P-22 total power-on time and the temporary counter below one minute.
 * Input       : None
 * Return         : None
 * Date            : 2026.05.01
 ******************************************************************************/
void Total_Power_Time_Clear(void) //20260501-1 add
{
    Total_Power_Second_Cnt = 0;
    config.Total_Power_Day = 0;
    config.Total_Power_Hour = 0;
    config.Total_Power_Minute = 0;
    sys.save_flag = 1;                  // write to EEPROM through the original save flow after clearing
}

/******************************************************************************
 * 函数名称       : save_pro
 * 功能描述       : 按 7 月 14 日版本在保存标志有效且延时结束后擦写 EEPROM 参数。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.07.29
 ******************************************************************************/
void save_pro(void) //20260729-13 change
{
    static u8 tick = 0;
    if (sys.save_flag == 1 && tick == 0 && !uart4_blocks_save()) //20261117-7 change 控制等待和最后停止位期间延后擦写。
    {
        EEPROM_SectorErase(BIAS_ADDR);

        EEPROM_write_n(BIAS_ADDR, (u8 *)&config, sizeof(config_t));
        sys.save_flag = 0;
        tick = 200;
    }
    else
    {
        if (tick > 0)
        {
            tick--;
        }
    }
}
