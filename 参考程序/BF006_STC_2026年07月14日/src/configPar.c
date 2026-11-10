#include "configPar.h"
#include "eeprom.h"
#include "system.h"
#include "user.h"
#include "updata4G.h"

#define BIAS_ADDR 0x0000
#define BAN_BEN 18 //20260430-3 change update version after adding total-time remainder fields

config_t config;

extern u16 Limit_Current;

static u8 Total_Run_Second_Cnt = 0;   //20260501-1 add run-time second counter, reset by the clear command
static u8 Total_Power_Second_Cnt = 0; //20260501-1 add power-on second counter, reset by the clear command

void Par_Init(void)
{

    u8 i = 0;
    EEPROM_read_n(BIAS_ADDR, (u8 *)&config, sizeof(config_t));

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
        Send_dat.updataAll = 1;
        Send_dat.GuZhangJiLu = 1;
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

void save_pro(void)
{
    static u8 tick = 0;
    if ((sys.save_flag == 1)&&(tick == 0))
    {
        EEPROM_SectorErase(BIAS_ADDR);

        EEPROM_write_n(BIAS_ADDR, (u8 *)&config, sizeof(config_t));
        sys.save_flag = 0;
        tick = 200;
    }else{
        if(tick >0)
        {
            tick--;
        }
    }
}
