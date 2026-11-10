#include "configPar.h"
#include "eeprom.h"
#include "system.h"
#include "user.h"
#include "updata4G.h"

#define BIAS_ADDR 0x0000
#define BAN_BEN 16

config_t config;

extern u16 Limit_Current;

void Par_Init(void)
{

    u8 i = 0;
    EEPROM_read_n(BIAS_ADDR, (u8 *)&config, sizeof(config_t));

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
