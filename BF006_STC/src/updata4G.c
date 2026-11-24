#if 0 //20261113-0 del 封存旧模块的初始化、报文解析、远程命令和上报状态。
#include "updata4G.h"
#include "uart4.h"
#include "string.h"
#include "system.h"
#include <stdio.h>
#include <stdlib.h>

extern void Speak_Stop(void);
extern void senSerModeCharge(void);

#define CMD_LEN 50

Send_dat_t Send_dat;
extern void reset_dev(u8 ty);

void updata4G_init(void)
{

    uart_init();
    memset(&Send_dat, 0, sizeof(Send_dat_t));
    sys.up4G_err = ERR_NUMB;
}

void Hand_set(void)
{
    if (Send_dat.Hand_set == 0)
    {
        Send_dat.Hand_set = 1;
        printf("{\"params\":{");
    }
}
void Updata_4G(void)
{

    int errCode;
    u8 Temp;
    Send_dat.Hand_set = 0;

    // pump A status
    if ((Send_dat.Pump_A_state != sys.Pump_A_state) ||
        (Send_dat.updataAll))
    {
        Send_dat.Pump_A_state = sys.Pump_A_state;
        Hand_set();
        printf("\"A_Power\":%d,", (int)Send_dat.Pump_A_state);
        Send_dat.send_out_A = 3;
        Send_dat.send_delay_A = 2; // first delay: 2 seconds
    }

    if (Send_dat.send_delay_A == 0)
    {

        if (Send_dat.send_out_A)
        {
            Send_dat.send_out_A--;
            if (Send_dat.send_out_A == 2)
            {
                Send_dat.send_delay_A = 60; // second delay: 60 seconds
            }
            else if (Send_dat.send_out_A == 1)
            {
                Send_dat.send_delay_A = 1800; // third delay: 1800 seconds
            }

            if (Send_dat.updataAll)
            {
                Send_dat.send_out_A = 0;
            }

            // under-voltage
            if (Send_dat.Voltage_Line != sys.Voltage_Line || Send_dat.updataAll)
            {

                Send_dat.Voltage_Line = sys.Voltage_Line;
                Hand_set();
                printf("\"Light_Volt\":%d,", (int)Send_dat.Voltage_Line);
            }

            // pump A current phase A
            if (Send_dat.Current_A_A != sys.Current_A1 || Send_dat.updataAll)
            {

                Send_dat.Current_A_A = sys.Current_A1;
                Hand_set();
                printf("\"A_Current1\":%.1f,", (float)Send_dat.Current_A_A / 10.0);
            }
#if (Model_Voltage == Model_3PH)
            // pump A current phase B
            if (Send_dat.Current_A_B != sys.Current_B1 || Send_dat.updataAll)
            {

                Send_dat.Current_A_B = sys.Current_B1;
                Hand_set();
                printf("\"B_Current1\":%.1f,", (float)Send_dat.Current_A_B / 10.0);
            }

            // pump A current phase C
            if (Send_dat.Current_A_C != sys.Current_C1 || Send_dat.updataAll)
            {

                Send_dat.Current_A_C = sys.Current_C1;
                Hand_set();
                printf("\"C_Current1\":%.1f,", (float)Send_dat.Current_A_C / 10.0);
            }
#endif
        }
    }
    else
    {
        Send_dat.send_delay_A--;
    }
#if (Model_OUT == Model_1C2)

    // pump B status
    if ((Send_dat.Pump_B_state != sys.Pump_B_state) ||
        (Send_dat.updataAll))
    {
        Send_dat.Pump_B_state = sys.Pump_B_state;
        Hand_set();
        printf("\"B_Power\":%d,", (int)Send_dat.Pump_B_state);
        Send_dat.send_out_B = 3;
        Send_dat.send_delay_B = 2;
    }

    if (Send_dat.send_delay_B == 0)
    {
        if (Send_dat.send_out_B)
        {
            Send_dat.send_out_B--;
            if (Send_dat.send_out_B == 2)
            {
                Send_dat.send_delay_B = 60; // second delay: 60 seconds
            }
            else if (Send_dat.send_out_B == 1)
            {
                Send_dat.send_delay_B = 1800; // third delay: 1800 seconds
            }

            if (Send_dat.updataAll)
            {
                Send_dat.send_out_B = 0;
            }
            // under-voltage
            if (Send_dat.Voltage_Line != sys.Voltage_Line)
            {

                Send_dat.Voltage_Line = sys.Voltage_Line;
                Hand_set();
                printf("\"Light_Volt\":%d,", (int)Send_dat.Voltage_Line);
            }
            // pump B current phase A
            if (Send_dat.Current_B_A != sys.Current_A2 || Send_dat.updataAll)
            {
                Send_dat.Current_B_A = sys.Current_A2;
                Hand_set();
                printf("\"A_Current2\":%.1f,", (float)Send_dat.Current_B_A / 10.0);
            }
#if (Model_Voltage == Model_3PH)
            // pump B current phase B
            if (Send_dat.Current_B_B != sys.Current_B2 || Send_dat.updataAll)
            {
                Send_dat.Current_B_B = sys.Current_B2;
                Hand_set();
                printf("\"B_Current2\":%.1f,", (float)Send_dat.Current_B_B / 10.0);
            }

            // pump B current phase C
            if (Send_dat.Current_B_C != sys.Current_C2 || Send_dat.updataAll)
            {
                Send_dat.Current_B_C = sys.Current_C2;
                Hand_set();
                printf("\"C_Current2\":%.1f,", (float)Send_dat.Current_B_C / 10.0);
            }
#endif
        }
    }
    else
    {
        Send_dat.send_delay_B--;
    }
#endif

    // water level data
    if ((Send_dat.Sensor_Mode != config.Sensor_Mode) ||
        (Send_dat.updataAll))
    {
        Send_dat.Sensor_Mode = config.Sensor_Mode;
        Hand_set();
        printf("\"Sensor_Mode\":%d,", (int)Send_dat.Sensor_Mode);
    }

    // Hand_Auto_timer_Flag
    if ((Send_dat.Hand_Auto_timer_Flag != config.Hand_Auto_timer_Flag) ||
        (Send_dat.updataAll))
    {
        Send_dat.Hand_Auto_timer_Flag = config.Hand_Auto_timer_Flag;
        Hand_set();
        printf("\"RunMode\":%d,", (int)Send_dat.Hand_Auto_timer_Flag);
    }

    // Pump_AB_Mode
    if ((Send_dat.Pump_AB_Mode != config.Pump_AB_Mode) ||
        (Send_dat.updataAll))
    {
        Send_dat.Pump_AB_Mode = config.Pump_AB_Mode;
        Hand_set();
        printf("\"Config_Mode\":%d,", (int)Send_dat.Pump_AB_Mode);
    }

    // under-voltage
    if ((Send_dat.UVP_Value != config.UVP_Value) ||
        (Send_dat.updataAll))
    {
        Send_dat.UVP_Value = config.UVP_Value;
        Hand_set();
        printf("\"Unvolt_guard\":%d,", (int)Send_dat.UVP_Value);
    }

    // over-voltage
    if ((Send_dat.OVP_Value != config.OVP_Value) ||
        (Send_dat.updataAll))
    {
        Send_dat.OVP_Value = config.OVP_Value;
        Hand_set();
        printf("\"Overvolt_guard\":%d,", (int)Send_dat.OVP_Value);
    }

    // overload A
    if ((Send_dat.Set_OVERLOAD_CurrentA != config.Set_OVERLOAD_Current1) ||
        (Send_dat.updataAll))
    {
        Send_dat.Set_OVERLOAD_CurrentA = config.Set_OVERLOAD_Current1;
        Hand_set();
        printf("\"Overload_guard_A\":%0.1f,", (float)Send_dat.Set_OVERLOAD_CurrentA / 10.0);
    }

    // dry-run A
    if ((Send_dat.Set_STANDBY_CurrentA != config.Set_STANDBY_Current1) ||
        (Send_dat.updataAll))
    {
        Send_dat.Set_STANDBY_CurrentA = config.Set_STANDBY_Current1;
        Hand_set();
        printf("\"Noload_guard_A\":%0.1f,", (float)Send_dat.Set_STANDBY_CurrentA / 10.0);
    }

    // overload B
    if ((Send_dat.Set_OVERLOAD_CurrentB != config.Set_OVERLOAD_Current2) ||
        (Send_dat.updataAll))
    {
        Send_dat.Set_OVERLOAD_CurrentB = config.Set_OVERLOAD_Current2;
        Hand_set();
        printf("\"Overload_guard_B\":%0.1f,", (float)Send_dat.Set_OVERLOAD_CurrentB / 10.0);
    }

    // dry-run B
    if ((Send_dat.Set_STANDBY_CurrentB != config.Set_STANDBY_Current2) ||
        (Send_dat.updataAll))
    {
        Send_dat.Set_STANDBY_CurrentB = config.Set_STANDBY_Current2;
        Hand_set();
        printf("\"Noload_guard_B\":%0.1f,", (float)Send_dat.Set_STANDBY_CurrentB / 10.0);
    }

    // sensor fault
    if ((Send_dat.Sensor_Rang != config.Sensor_Rang) ||
        (Send_dat.updataAll))
    {
        Send_dat.Sensor_Rang = config.Sensor_Rang;
        Hand_set();
        printf("\"Content_Gauge_Range\":%0.1f,", (float)Send_dat.Sensor_Rang / 10.0);
    }

    // run or stop state
    if ((Send_dat.Sensor_STOP != config.Sensor_STOP) ||
        (Send_dat.updataAll))
    {
        Send_dat.Sensor_STOP = config.Sensor_STOP;
        Hand_set();
        printf("\"Content_Gauge_Stop\":%0.1f,", (float)Send_dat.Sensor_STOP / 10.0);
    }
    if (Send_dat.sensorUpdate)
    {
        Send_dat.sensorUpdate = 0;

        if (Send_dat.sensorOut != sys.SensorOut)
        {
            Send_dat.sensorOut = sys.SensorOut;
            Hand_set();
            printf("\"Senser_Data\":%0.2f,", (float)sys.SensorOut / 100.0);
        }

        // low water level
        if ((Send_dat.UpWaterLever != sys.shang_shuiwei) ||
            (Send_dat.updataAll))
        {

            Send_dat.UpWaterLever = sys.shang_shuiwei;
            Hand_set();
            printf("\"Water_Level_A\":%d,", (int)Send_dat.UpWaterLever);
        }

        // high water level
        if ((Send_dat.BottomWaterLever != sys.xia_shuiwei) ||
            (Send_dat.updataAll))
        {

            Send_dat.BottomWaterLever = sys.xia_shuiwei;
            Hand_set();
            printf("\"Water_Level_B\":%d,", (int)Send_dat.BottomWaterLever);
        }
        Hand_set();
        printf("\"sensorUpdate\":1,");
    }

    // parameter area 1
    if (Send_dat.Sensor_ON1 != config.Sensor_ON1)
    {
        Send_dat.Sensor_ON1 = config.Sensor_ON1;
        Hand_set();
        printf("\"Content_Gauge_Start\":%0.1f,", (float)Send_dat.Sensor_ON1 / 10.0);
    }
    // parameter area 2
    if (Send_dat.Sensor_ON2 != config.Sensor_ON2)
    {
        Send_dat.Sensor_ON2 = config.Sensor_ON2;
        Hand_set();
        printf("\"Content_Gauge_Synt\":%0.1f,", (float)Send_dat.Sensor_ON2 / 10.0);
    }

    // cycle run time
    if ((Send_dat.AUTO_Timing_Run != config.AUTO_Timing_Run) ||
        (Send_dat.updataAll))
    {
        Send_dat.AUTO_Timing_Run = config.AUTO_Timing_Run;
        Hand_set();
        printf("\"Cyclic_timing_start\":%d,", (int)Send_dat.AUTO_Timing_Run);
    }

    // cycle stop time
    if ((Send_dat.AUTO_Timing_Stop != config.AUTO_Timing_Stop) ||
        (Send_dat.updataAll))
    {
        Send_dat.AUTO_Timing_Stop = config.AUTO_Timing_Stop;
        Hand_set();
        printf("\"Cyclic_timing_stop\":%d,", (int)Send_dat.AUTO_Timing_Stop);
    }

    // gate switch
    if ((Send_dat.GateSW != sys.GateSW) ||
        (Send_dat.updataAll))
    {
        Send_dat.GateSW = sys.GateSW;
        Hand_set();
        printf("\"GateSW\":%d,", (int)Send_dat.GateSW);
    }
    // fault mask
    if ((Send_dat.ErrCode != sys.up4G_err) ||
        (Send_dat.updataAll))
    {
        Send_dat.ErrCode = sys.up4G_err;

        switch (Send_dat.ErrCode)
        {
        case ERR_UVP:
            errCode = -2;
            break;
        case ERR_OVP:
            errCode = -3;
            break;
        case ERR_OVERLOAD1:
            errCode = -8;
            break;
        case ERR_STANDBY1:
            errCode = -7;
            break;
        case ERR_LOSE_A1:
            errCode = -4;
            break;
        case ERR_LOSE_B1:
            errCode = -5;
            break;
        case ERR_LOSE_C1:
            errCode = -6;
            break;
        case ERR_OVERLOAD2:
            errCode = -13;
            break;
        case ERR_STANDBY2:
            errCode = -12;
            break;
        case ERR_LOSE_A2:
            errCode = -9;
            break;
        case ERR_LOSE_B2:
            errCode = -10;
            break;
        case ERR_LOSE_C2:
            errCode = -11;
            break;
        default:
            if (sys.sensor_err)
            {
                errCode = -14;
            }
            else if (sys.flowover)
            {
                errCode = -15;
            }
            else
            {

                errCode = 0;
            }
            break;
        }

        Hand_set();
        printf("\"Light_Status\":%d,", errCode);
    }

    // sensor threshold and current water level
    if ((Send_dat.Auto_Mode != sys.Auto_Mode) ||
        (Send_dat.updataAll))
    {
        Send_dat.Auto_Mode = sys.Auto_Mode;
        Hand_set();
        printf("\"Work_Mode\":%d,", (int)Send_dat.Auto_Mode);

        if (Send_dat.Auto_Mode == 2 || Send_dat.Auto_Mode == 4)
        {
            Temp = 1;
        }
        else
        {
            Temp = 0;
        }
        printf("\"Senser_Mode\":%d,", (int)Temp);
    }
    // if (++Send_dat.sensorOut_Timer >= 120 || (Send_dat.updataAll))
    // {
    //     Send_dat.sensorOut_Timer = 0;

    // parameter area 2
    //     if ((Send_dat.sensorOut != sys.SensorOut) ||
    //         (Send_dat.updataAll))
    //     {
    //         Send_dat.sensorOut = sys.SensorOut;
    //         Hand_set();
    //         printf("\"Senser_Data\":%0.2f,", (float)sys.SensorOut / 100.0);
    //     }
    // }
    // if (++Send_dat.GuZhangJiLu_Timer >= 120)
    // {
    //     Send_dat.GuZhangJiLu_Timer = 0;
    // fault record
    //     if (Send_dat.GuZhangJiLu)
    //     {
    //         Send_dat.GuZhangJiLu = 0;
    //         Hand_set();
    //         if (config.Record_Type[0] == ERR_NUMB)
    //         {
    //             printf("\"ErrorCode_1\":0,");
    //             printf("\"Errorvolt_1\":0,");
    //             printf("\"Errorcurrent_1\":0,");
    //         }
    //         else
    //         {

    //             printf("\"ErrorCode_1\":%d,", (int)config.Record_Type[0] + 1);
    //             printf("\"Errorvolt_1\":%d,", (int)config.Record_Dat1[0]);
    //             printf("\"Errorcurrent_1\":%0.1f,", (float)config.Record_Dat2[0] / 10.0);
    //         }

    //         if (config.Record_Type[1] == ERR_NUMB)
    //         {
    //             printf("\"ErrorCode_2\":0,");
    //             printf("\"Errorvolt_2\":0,");
    //             printf("\"Errorcurrent_2\":0,");
    //         }
    //         else
    //         {

    //             printf("\"ErrorCode_2\":%d,", (int)config.Record_Type[1] + 1);
    //             printf("\"Errorvolt_2\":%d,", (int)config.Record_Dat1[1]);
    //             printf("\"Errorcurrent_2\":%0.1f,", (float)config.Record_Dat2[1] / 10.0);
    //         }

    //         if (config.Record_Type[2] == ERR_NUMB)
    //         {
    //             printf("\"ErrorCode_3\":0,");
    //             printf("\"Errorvolt_3\":0,");
    //             printf("\"Errorcurrent_3\":0,");
    //         }
    //         else
    //         {

    //             printf("\"ErrorCode_3\":%d,", (int)config.Record_Type[2] + 1);
    //             printf("\"Errorvolt_3\":%d,", (int)config.Record_Dat1[2]);
    //             printf("\"Errorcurrent_3\":%0.1f,", (float)config.Record_Dat2[2] / 10.0);
    //         }
    //         if (config.Record_Type[3] == ERR_NUMB)
    //         {
    //             printf("\"ErrorCode_4\":0,");
    //             printf("\"Errorvolt_4\":0,");
    //             printf("\"Errorcurrent_4\":0,");
    //         }
    //         else
    //         {

    //             printf("\"ErrorCode_4\":%d,", (int)config.Record_Type[3] + 1);
    //             printf("\"Errorvolt_4\":%d,", (int)config.Record_Dat1[3]);
    //             printf("\"Errorcurrent_4\":%0.1f,", (float)config.Record_Dat2[3] / 10.0);
    //         }
    //         if (config.Record_Type[4] == ERR_NUMB)
    //         {
    //             printf("\"ErrorCode_5\":0,");
    //             printf("\"Errorvolt_5\":0,");
    //             printf("\"Errorcurrent_5\":0,");
    //         }
    //         else
    //         {

    //             printf("\"ErrorCode_5\":%d,", (int)config.Record_Type[4] + 1);
    //             printf("\"Errorvolt_5\":%d,", (int)config.Record_Dat1[4]);
    //             printf("\"Errorcurrent_5\":%0.1f,", (float)config.Record_Dat2[4] / 10.0);
    //         }
    //         if (config.Record_Type[5] == ERR_NUMB)
    //         {
    //             printf("\"ErrorCode_6\":0,");
    //             printf("\"Errorvolt_6\":0,");
    //             printf("\"Errorcurrent_6\":0,");
    //         }
    //         else
    //         {

    //             printf("\"ErrorCode_6\":%d,", (int)config.Record_Type[5] + 1);
    //             printf("\"Errorvolt_6\":%d,", (int)config.Record_Dat1[5]);
    //             printf("\"Errorcurrent_6\":%0.1f,", (float)config.Record_Dat2[5] / 10.0);
    //         }
    //         if (config.Record_Type[6] == ERR_NUMB)
    //         {
    //             printf("\"ErrorCode_7\":0,");
    //             printf("\"Errorvolt_7\":0,");
    //             printf("\"Errorcurrent_7\":0,");
    //         }
    //         else
    //         {

    //             printf("\"ErrorCode_7\":%d,", (int)config.Record_Type[6] + 1);
    //             printf("\"Errorvolt_7\":%d,", (int)config.Record_Dat1[6]);
    //             printf("\"Errorcurrent_7\":%0.1f,", (float)config.Record_Dat2[6] / 10.0);
    //         }
    //         if (config.Record_Type[7] == ERR_NUMB)
    //         {
    //             printf("\"ErrorCode_8\":0,");
    //             printf("\"Errorvolt_8\":0,");
    //             printf("\"Errorcurrent_8\":0,");
    //         }
    //         else
    //         {

    //             printf("\"ErrorCode_8\":%d,", (int)config.Record_Type[7] + 1);
    //             printf("\"Errorvolt_8\":%d,", (int)config.Record_Dat1[7]);
    //             printf("\"Errorcurrent_8\":%0.1f,", (float)config.Record_Dat2[7] / 10.0);
    //         }
    //         if (config.Record_Type[8] == ERR_NUMB)
    //         {
    //             printf("\"ErrorCode_9\":0,");
    //             printf("\"Errorvolt_9\":0,");
    //             printf("\"Errorcurrent_9\":0,");
    //         }
    //         else
    //         {

    //             printf("\"ErrorCode_9\":%d,", (int)config.Record_Type[8] + 1);
    //             printf("\"Errorvolt_9\":%d,", (int)config.Record_Dat1[8]);
    //             printf("\"Errorcurrent_9\":%0.1f,", (float)config.Record_Dat2[8] / 10.0);
    //         }
    //     }
    // }

    if (Send_dat.Hand_set)
    {
        printf("}}");
    }
    if (Send_dat.updataAll)
    {
        Send_dat.GuZhangJiLu = 1;
        Send_dat.GuZhangJiLu_Timer = 120;
    }
    Send_dat.updataAll = 0;

    //	if(times < 5)
    //	{
    //		times++;
    //
    //	}else
    //	{

    //    Send_dat.updataAll = 0;
    //	}
}

// char xdata rx_command_buf[RX_LEN] = {};

char *Analyse_pos = 0;
u8 commd_len = 0;

char commd_str[CMD_LEN];
char value_str[CMD_LEN];

void command_pro(void)
{
    u16 vl;
    float v2;
    if (strcmp(commd_str, "RunMode") == 0)
    {
        vl = atoi(value_str);
        if (vl < 3)
        {
            config.Hand_Auto_timer_Flag = vl;
            //            Send_dat.Hand_Auto_timer_Flag = config.Hand_Auto_timer_Flag;
            sys.save_flag = 1;
        }
    }
    else if (strcmp(commd_str, "A_Power") == 0)
    {
        vl = atoi(value_str);
        if (vl < 2)
        {
            //			if(sys.Fault_Error >0)
            //			{
            //				sys.Fault_Error = 0;
            //			}
            //			else
            if (config.Hand_Auto_timer_Flag == RUNMODE_MANUAL)
            {
                if (vl)
                {
                    if (sys.Fault_A_Flag == 0)
                    {
                        sys.Pump_A_ON = 1;
                    }
                }
                else
                {

                    sys.Pump_A_ON = 0;
                }
            }
            //            Send_dat.Pump_A_state = vl;
        }
    }
    else if (strcmp(commd_str, "B_Power") == 0)
    {
        vl = atoi(value_str);
        if (vl < 2)
        {
            //			if(sys.Fault_Error >0)
            //			{
            //				sys.Fault_Error = 0;
            //			}
            //			else
            if (config.Hand_Auto_timer_Flag == RUNMODE_MANUAL)
            {
                if (vl)
                {
                    if (sys.Fault_B_Flag == 0)
                    {
                        sys.Pump_B_ON = 1;
                    }
                }
                else
                {

                    sys.Pump_B_ON = 0;
                }
            }
            //            Send_dat.Pump_B_state = vl;
        }
    }
    else if (strcmp(commd_str, "Config_Mode") == 0)
    {
        vl = atoi(value_str);
        if (vl < 4)
        {
            config.Pump_AB_Mode = vl;
            //            Send_dat.Pump_AB_Mode = config.Pump_AB_Mode;
            sys.save_flag = 1;
        }
    }
    else if (strcmp(commd_str, "Unvolt_guard") == 0)
    {
        vl = atoi(value_str);
        if (vl >= 260 && vl <= 350)
        {
            config.UVP_Value = vl;
            //            Send_dat.UVP_Value = config.UVP_Value;
            sys.save_flag = 1;
        }
    }
    else if (strcmp(commd_str, "Overvolt_guard") == 0)
    {
        vl = atoi(value_str);
        if (vl >= 350 && vl <= 480)
        {
            config.OVP_Value = vl;
            //            Send_dat.OVP_Value = config.OVP_Value;
            sys.save_flag = 1;
        }
    }
    else if (strcmp(commd_str, "Overload_guard_A") == 0)
    {
        v2 = atof(value_str);
        vl = (int)(v2 * 10);
        if (vl >= 0 && vl <= 999)
        {
            config.Set_OVERLOAD_Current1 = vl;
            //			Send_dat.Set_OVERLOAD_CurrentA = config.Set_OVERLOAD_Current1;
            sys.save_flag = 1;
        }
    }
    else if (strcmp(commd_str, "Noload_guard_A") == 0)
    {
        v2 = atof(value_str);
        vl = (int)(v2 * 10);
        if (vl >= 0 && vl <= 999)
        {
            config.Set_STANDBY_Current1 = vl;
            //			Send_dat.Set_STANDBY_CurrentA = config.Set_STANDBY_Current1;
            sys.save_flag = 1;
        }
    }
    else if (strcmp(commd_str, "Overload_guard_B") == 0)
    {
        v2 = atof(value_str);
        vl = (int)(v2 * 10);
        if (vl >= 0 && vl <= 999)
        {
            config.Set_OVERLOAD_Current2 = vl;
            //			Send_dat.Set_OVERLOAD_CurrentB = config.Set_OVERLOAD_Current2;
            sys.save_flag = 1;
        }
    }
    else if (strcmp(commd_str, "Noload_guard_B") == 0)
    {
        v2 = atof(value_str);
        vl = (int)(v2 * 10);
        if (vl >= 0 && vl <= 999)
        {
            config.Set_STANDBY_Current2 = vl;
            //			Send_dat.Set_STANDBY_CurrentB = config.Set_STANDBY_Current2;
            sys.save_flag = 1;
        }
    }
    else if (strcmp(commd_str, "Cyclic_timing_start") == 0)
    {
        vl = atoi(value_str);
        if (vl <= 999)
        {
            config.AUTO_Timing_Run = vl;
            //            Send_dat.AUTO_Timing_Run = config.AUTO_Timing_Run;
            sys.save_flag = 1;
        }
    }
    else if (strcmp(commd_str, "Cyclic_timing_stop") == 0)
    {
        vl = atoi(value_str);
        if (vl <= 999)
        {
            config.AUTO_Timing_Stop = vl;
            //            Send_dat.AUTO_Timing_Stop = config.AUTO_Timing_Stop;
            sys.save_flag = 1;
        }
    }
    else if (strcmp(commd_str, "Sensor_Mode") == 0)
    {
        vl = atoi(value_str);
        if (vl <= 1)
        {
            config.Sensor_Mode = vl;
            sys.save_flag = 1;
            senSerModeCharge();
        }
    }
    else if (strcmp(commd_str, "Content_Gauge_Range") == 0)
    {
        v2 = atof(value_str);
        vl = (int)(v2 * 10);
        if (vl >= 0 && vl <= 999)
        {
            config.Sensor_Rang = vl;
            sys.save_flag = 1;
            updatSysdata();
        }
    }
    else if (strcmp(commd_str, "Content_Gauge_Start") == 0)
    {
        v2 = atof(value_str);
        vl = (int)(v2 * 10);
        if (vl >= 0 && vl <= 999)
        {
            config.Sensor_ON1 = vl;
            sys.save_flag = 1;
            if (config.Sensor_Mode == CTROLTYPE_PUMP)
            {
                if (config.Sensor_ON1 > config.Sensor_STOP)
                {
                    config.Sensor_ON1 = config.Sensor_STOP - 1;
                }
                else if (config.Sensor_ON1 < config.Sensor_ON2)
                {
                    config.Sensor_ON1 = config.Sensor_ON2 + 1;
                }
            }
            else
            {
                if (config.Sensor_ON1 > config.Sensor_ON2)
                {
                    config.Sensor_ON1 = config.Sensor_ON2 - 1;
                }
                else if (config.Sensor_ON1 < config.Sensor_STOP)
                {
                    config.Sensor_ON1 = config.Sensor_STOP + 1;
                }
            }
            updatSysdata();

        }
    }
    else if (strcmp(commd_str, "Content_Gauge_Synt") == 0)
    {
        v2 = atof(value_str);
        vl = (int)(v2 * 10);
        if (vl >= 0 && vl <= 999)
        {
            config.Sensor_ON2 = vl;
            sys.save_flag = 1;
            if (config.Sensor_Mode == CTROLTYPE_PUMP)
            {
                if (config.Sensor_ON2 > config.Sensor_ON1)
                {
                    config.Sensor_ON2 = config.Sensor_ON1 - 1;
                }
            }
            else
            {
                if (config.Sensor_ON2 < config.Sensor_ON1)
                {
                    config.Sensor_ON2 = config.Sensor_ON1 + 1;
                }
            }
            updatSysdata();            
        }
    }
    else if (strcmp(commd_str, "Content_Gauge_Stop") == 0)
    {
        v2 = atof(value_str);
        vl = (int)(v2 * 10);
        if (vl >= 0 && vl <= 999)
        {
            config.Sensor_STOP = vl;
            sys.save_flag = 1;
            if (config.Sensor_Mode == CTROLTYPE_PUMP)
            {
                if (config.Sensor_STOP < config.Sensor_ON1)
                {
                    config.Sensor_STOP = config.Sensor_ON1 + 1;
                }
            }
            else
            {
                if (config.Sensor_STOP > config.Sensor_ON1)
                {
                    config.Sensor_STOP = config.Sensor_ON1 - 1;
                }
            } 
            updatSysdata();           
        }
    }
    else if (strcmp(commd_str, "Restore_Factory") == 0)
    {
        vl = atoi(value_str);
        if (vl == 1)
        {
            reset_dev(config.Dev_type);
        }
    }
    else if (strcmp(commd_str, "resetErr") == 0)
    {
        vl = atoi(value_str);
        if (vl == 1)
        {
            if (sys.Fault_Error > 0)
            {
                clear_A_Fault(); //20260729-5 change 由统一清除入口保留仍接通的触点过载
                clear_B_Fault();
            }
        }
    }
    else if (strcmp(commd_str, "GateSW") == 0)
    {
        vl = atoi(value_str);
        if (vl < 2)
        {
            if (vl)
            {
                sys.GateSW = 1;
            }
            else
            {
                sys.GateSW = 0;
            }
        }
    }
    else if (strcmp(commd_str, "sensorUpdate") == 0)
    {
        Send_dat.sensorUpdate = 1;
    }
}

void Analyse_4G(void)
{
    char *p;

    if (COM4.B_RX_OK)
    {
        Analyse_pos = strstr(RX4_Buffer, "\"params\":{");
        if (Analyse_pos != NULL)
        {
            Analyse_pos += 11; // skip literal string params
            p = Analyse_pos;

            Analyse_pos = strchr(p, '"');

            commd_len = Analyse_pos - p;
            if (commd_len > 0 && commd_len < CMD_LEN)
            {
                strncpy(commd_str, p, commd_len);
                commd_str[commd_len] = 0; // terminate command string

                Analyse_pos += 2; // skip delimiter

                p = Analyse_pos;
                Analyse_pos = strchr(p, '}');
                commd_len = Analyse_pos - p;
                if (commd_len > 0 && commd_len < CMD_LEN)
                {
                    strncpy(value_str, p, commd_len);
                    value_str[commd_len] = 0; // terminate value string

                    command_pro();
                }
            }
        }
        COM4.B_RX_OK = 0;
    }
}
#endif

#include "uart4.h"
#include "system.h"
#include "Modbus_New.h"
#include "ad.h"
#include "Display.h"
#include "HT1622.h"

#define BF4G_FMT          0x03 //20261116-0 add 新格式使用独立版本号。
#define BF4G_PARAM_SOURCE(number) (0x80 | (number)) //20261116-10 add 紧凑布局中的参数来源标记。
#define BF4G_DATA         9
#define BF4G_OVERHEAD     11
#define BF4G_CAPABILITIES 0x001f //20261116-8 change 打开已验证的控制能力。

/******************************************************************************
 * 函数名称       : bf4g_crc
 * 功能描述       : 计算从地址开始的反射循环冗余校验值。
 * 输入参数       : frame，报文起始位置；length，参与计算的字节数。
 * 返回值         : 校验值。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u16 bf4g_crc(const u8 xdata *frame, u8 length) //20261116-0 add
{
    u16 crc = 0xffff;
    u8 i;
    u8 n;

    for (i = 0; i < length; i++) {
        crc ^= frame[i];
        for (n = 0; n < 8; n++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xa001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

/******************************************************************************
 * 函数名称       : bf4g_put_u16
 * 功能描述       : 将十六位数据按高字节在前写入应答。
 * 输入参数       : out，写入位置；value，要写入的值。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void bf4g_put_u16(u8 xdata *out, u16 value) //20261116-0 add
{
    out[0] = value >> 8;
    out[1] = value;
}

/******************************************************************************
 * 函数名称       : bf4g_model_word
 * 功能描述       : 按当前过载设置生成兼容显示用的旧型号字。
 * 输入参数       : None
 * 返回值         : 由旧机型宏和规格码组成的型号字。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u16 bf4g_model_word(void) //20261116-0 add
{
    u8 spec = 0x93;

    if (!config_loaded) {
        return 0;
    }
    switch (config.Set_OVERLOAD_Current1) {
    case V1_Set_Current:
        spec = 0x90;
        break;
    case V2_Set_Current:
        spec = 0x91;
        break;
    case V3_Set_Current:
        spec = 0x92;
        break;
    }
    return ((u16)Dev_Model << 8) | spec;
}

/******************************************************************************
 * 函数名称       : bf4g_device_info
 * 功能描述       : 编码设备信息，相数、泵数和额定电压由编译机型确定。
 * 输入参数       : out，应答的数据起始位置。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void bf4g_device_info(u8 xdata *out) //20261116-0 add
{
    out[0] = 0;
    out[1] = 1; //20261117-7 change 标识控制应答使用扫描确认后的结果。
    bf4g_put_u16(out + 2, bf4g_model_word());
    bf4g_put_u16(out + 4, MODBUS_NEW_VERSION);
    out[6] = Model_Voltage == Model_1PH ? 1 : 3;
    out[7] = Model_OUT == Model_1C1 ? 1 : 2;
    bf4g_put_u16(out + 8, VoltageValue == Vol_380 ? 380 : 220);
    bf4g_put_u16(out + 10, BF4G_CAPABILITIES);
    out[12] = BF4G_FRAME_MAX;
    out[13] = 16;
    out[14] = 9;
    out[15] = 8;
}

/******************************************************************************
 * 函数名称       : bf4g_param_value
 * 功能描述       : 读取参数号对应的原配置值。
 * 输入参数       : number，已经校验的参数号。
 * 返回值         : 原始值。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u16 bf4g_param_value(u8 number) //20261116-1 add
{
    switch (number) {
    case 0:
        return config.OVP_Value;
    case 1:
        return config.UVP_Value;
    case 2:
        return config.Set_STANDBY_Current1;
    case 3:
        return config.Set_OVERLOAD_Current1;
    case 4:
        return config.Set_STANDBY_Current2;
    case 5:
        return config.Set_OVERLOAD_Current2;
    case 6:
        return config.AUTO_Timing_Run;
    case 7:
        return config.AUTO_Timing_Stop;
    case 8:
        return config.Timer_Standby_Restart;
    case 9:
        return config.Timer_Unload_Restart;
    case 10:
        return config.AUTO_FANGXIU;
    case 11:
        return config.Dev_Address;
    case 12:
        return config.Pump_AB_Mode;
    case 13:
        return config.Sensor_Mode;
    case 14:
        return config.Sensor_Rang;
    case 15:
        return config.Sensor_STOP;
    case 16:
        return config.Sensor_ON1;
    case 17:
        return config.Sensor_ON2;
    case 18:
        return config.Auto_Mode;
    case 19:
        return config.fault_mask;
    case 20:
        return config.Sensor_overFlow;
    case 21:
        return config.Total_Run_Hour;
    case 22:
        return config.Total_Power_Day;
    }
    return 0;
}

/******************************************************************************
 * 函数名称       : bf4g_param_applicable
 * 功能描述       : 判断参数是否适用于编译机型。
 * 输入参数       : number，参数号。
 * 返回值         : 适用时为一。
 * 日期            : 2026.11.16
 ******************************************************************************/
static bit bf4g_param_applicable(u8 number) //20261116-1 add
{
    return Model_OUT == Model_1C2 ||
           (number != 4 && number != 5 && number != 12 && number != 17);
}

/******************************************************************************
 * 函数名称       : bf4g_param_minimum
 * 功能描述       : 查询参数下限。
 * 输入参数       : number，已经校验的参数号。
 * 返回值         : 参数下限。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u16 bf4g_param_minimum(u8 number) //20261116-1 add
{
    if (number <= 1) {
        return 85;
    }
    return number == 11 ? 1 : 0;
}

/******************************************************************************
 * 函数名称       : bf4g_param_maximum
 * 功能描述       : 查询参数上限，连续量阈值使用当前量程。
 * 输入参数       : number，已经校验的参数号。
 * 返回值         : 参数上限。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u16 bf4g_param_maximum(u8 number) //20261116-1 add
{
    static const u16 code limits[23] = {
        0, 0, 900, 900, 900, 900, 9999, 9999, 9999, 9999, 999, 254,
        2, 1, 9999, 0, 0, 0, 4, 4, 0, 9999, 9999
    };

    if (number <= 1) {
        return VoltageValue == Vol_380 ? 480 : 280;
    }
    if (number == 15 || number == 16 || number == 17 || number == 20) {
        return config.Sensor_Rang;     // 零量程时只有零阈值合法。
    }
    return limits[number];
}

/******************************************************************************
 * 函数名称       : bf4g_param_read
 * 功能描述       : 根据装载、机型及数值范围生成单项质量和原值。
 * 输入参数       : number，已经校验的参数号；quality，质量输出位置。
 * 返回值         : 原值，未装载时为零。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u16 bf4g_param_read(u8 number, u8 *quality) //20261116-1 add
{
    u16 value;

    if (!config_loaded) {
        *quality = 2;
        return 0;
    }
    value = bf4g_param_value(number);
    if (!bf4g_param_applicable(number)) {
        *quality = 1;                 // 保留不适用参数原值，优先于异常值判定。
    } else if (value < bf4g_param_minimum(number) || value > bf4g_param_maximum(number)) {
        *quality = 3;
    } else {
        *quality = 0;
    }
    return value;
}

/******************************************************************************
 * 函数名称       : bf4g_error
 * 功能描述       : 在共享帧内填写统一错误应答数据。
 * 输入参数       : out，应答数据位置；status，结果码；object，目标。
 * 返回值         : 错误数据长度。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u8 bf4g_error(u8 xdata *out, u8 status, u8 object) //20261116-1 add
{
    u8 i;

    out[0] = status;
    out[1] = object;
    for (i = 2; i < 10; i++) {
        out[i] = 0;                  // 无范围信息时两个边界均写零。
    }
    return 10;
}

/******************************************************************************
 * 函数名称       : bf4g_read_parameters
 * 功能描述       : 校验区间并逐项写入参数质量和原值。
 * 输入参数       : out，请求及应答数据位置；length，请求数据长度。
 * 返回值         : 应答数据长度。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u8 bf4g_read_parameters(u8 xdata *out, u8 length) //20261116-1 add
{
    u8 start;
    u8 count;
    u8 i;
    u8 quality;
    u16 value;

    if (length != 2) {
        return bf4g_error(out, 3, length ? out[0] : 0xff);
    }
    start = out[0];
    count = out[1];
    if (start >= 23) {
        return bf4g_error(out, 4, start);
    }
    if (count == 0 || count > 16 || count > 23 - start) {
        return bf4g_error(out, 3, start);
    }

    out[0] = 0;
    out[1] = start;
    out[2] = count;
    for (i = 0; i < count; i++) {
        value = bf4g_param_read(start + i, &quality);
        out[3 + i * 3] = quality;
        bf4g_put_u16(out + 4 + i * 3, value);
    }
    return 3 + count * 3;
}


/******************************************************************************
 * 函数名称       : bf4g_adc_value
 * 功能描述       : 原子读取由采样中断发布的一个十六位测量值。
 * 输入参数       : source，测量值位置。
 * 返回值         : 完整的测量值。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u16 bf4g_adc_value(const u16 xdata *source) //20261116-2 add
{
    bit interrupts;
    u16 value;

    interrupts = EA;
    EA = 0;
    value = *source;
    EA = interrupts;
    return value;
}

/******************************************************************************
 * 函数名称       : bf4g_fault_valid
 * 功能描述       : 检查故障原位图的保留位和机型适用位。
 * 输入参数       : faults，原位图。
 * 返回值         : 合法时为一。
 * 日期            : 2026.11.16
 ******************************************************************************/
static bit bf4g_fault_valid(u32 faults) //20261116-2 add
{
    if (faults & 0xfffe0000UL) {
        return 0;
    }
#if (Model_OUT == Model_1C1)
    if (faults & 0x00014f80UL) {
        return 0;
    }
#endif
#if (Model_Voltage == Model_1PH)
    return (faults & 0x00000e70UL) == 0;
#else
    return 1;
#endif
}

/******************************************************************************
 * 函数名称       : bf4g_first_fault
 * 功能描述       : 从完整位图得到优先故障代码。
 * 输入参数       : faults，通过机型校验的故障位图。
 * 返回值         : 最低有效位加一，零表示没有故障。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u8 bf4g_first_fault(u32 faults) //20261116-2 add
{
    u8 number = 1;

    if (faults == 0) {
        return 0;
    }
    while ((faults & 1) == 0) {
        number++;
        faults >>= 1;
    }
    return number;
}

/******************************************************************************
 * 函数名称       : bf4g_tank_quality
 * 功能描述       : 判断构造水池状态所需的液位是否已经建立。
 * 输入参数       : None
 * 返回值         : 读数质量。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u8 bf4g_tank_quality(void) //20261116-6 change
{
    if (!config_loaded || !water_status_ready || sensor_pending_tanks) { //20261116-6 change
        return 2;
    }
    return (sys.shang_shuiwei > 2 || sys.xia_shuiwei > 2) ? 3 : 0;
}

typedef struct {
    u32 faults;
    u8 mode;
} bf4g_diagnostics_t; //20261116-12 add 只暂存遥控中断也能更新的诊断来源。

/******************************************************************************
 * 函数名称       : bf4g_capture_diagnostics
 * 功能描述       : 一次读取遥控中断可能更新的模式和故障位图。
 * 输入参数       : snapshot，局部诊断数据位置。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void bf4g_capture_diagnostics(bf4g_diagnostics_t xdata *snapshot) //20261116-12 add
{
    bit interrupts = EA;

    EA = 0;
    snapshot->mode = config.Hand_Auto_timer_Flag;
    snapshot->faults = sys.Fault_Error;
    EA = interrupts;
}

/******************************************************************************
 * 函数名称       : bf4g_run_status
 * 功能描述       : 编制模式、运行、主备和两水池档位的状态位。
 * 输入参数       : mode，已捕获的模式；quality，质量输出位置。
 * 返回值         : 运行状态位图。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u16 bf4g_run_status(u8 mode, u8 *quality) //20261116-12 change
{
    static const u8 code mode_bits[] = {1, 6, 10};
    u16 state = 0;
    u8 tanks = bf4g_tank_quality();
#if (Model_OUT == Model_1C2)
    u8 priority = config.Pump_AB_Mode;
#endif

    *quality = tanks;
    if (mode <= RUNMODE_TIMER) {
        state = mode_bits[mode];
    } else if (*quality == 0) {
        *quality = 3;
    }
    if (sys.Pump_A_state) {
        state |= 0x0010;
    }
#if (Model_OUT == Model_1C2)
    if (sys.Pump_B_state) {
        state |= 0x0020;
    }
    if (priority <= 2) {
        state |= (u16)0x0040 << priority;
    } else if (*quality == 0) {
        *quality = 3;
    }
#endif

    if (tanks == 0) {
        state |= (u16)0x0200 << sys.shang_shuiwei;
        state |= (u16)0x1000 << sys.xia_shuiwei;
    }
    return state;
}

/******************************************************************************
 * 函数名称       : bf4g_alarm
 * 功能描述       : 从故障位图和水池档位生成报警摘要。
 * 输入参数       : faults，已经校验的故障位图。
 * 返回值         : 报警摘要位图。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u16 bf4g_alarm(u32 faults) //20261116-2 add
{
    u16 alarm = faults & 3;

    if (faults & 0x00000084UL) {
        alarm |= 0x0004;             // 任一泵过载。
    }
    if (faults & 0x00006108UL) {
        alarm |= 0x0008;             // 任一泵空载或干转。
    }
    if (faults & 0x00000e70UL) {
        alarm |= 0x0020;             // 任一泵缺相。
    }
    if (sys.shang_shuiwei == 2) {
        alarm |= 0x0040;
    }
    if (sys.xia_shuiwei == 0) {
        alarm |= 0x0100;
    }
    return alarm;
}

/******************************************************************************
 * 函数名称       : bf4g_sensor_type_quality
 * 功能描述       : 验证实际运行类型与当前配置是否一致。
 * 输入参数       : type，实际运行类型。
 * 返回值         : 读数质量。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u8 bf4g_sensor_type_quality(u8 type) //20261116-2 add
{
    if (!config_loaded) {
        return 2;
    }
    if (type > SENSORTYPE_PRESS0_5V || config.Auto_Mode > SENSORTYPE_PRESS0_5V) {
        return 3;
    }
    return type == config.Auto_Mode ? 0 : 2;
}

/******************************************************************************
 * 函数名称       : bf4g_sensor_read
 * 功能描述       : 生成同一运行类型下的传感器工程值、标志、单位和档位。
 * 输入参数       : number，字段号；quality，质量输出位置。
 * 返回值         : 传感器字段值。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u32 bf4g_sensor_read(u8 number, u8 *quality) //20261116-6 change
{
    u8 type = sys.Auto_Mode;
    u8 flags;

    *quality = bf4g_sensor_type_quality(type);
    if (*quality != 0) {
        return 0;
    }
    if (number == 0x12) {
        return type;
    }
    if (number == 0x14) {
        if (type < SENSORTYPE_WATER4_20MA) {
            return 0;
        }
        return type == SENSORTYPE_WATER4_20MA ? 1 : 2;
    }
    if (number == 0x11) {
        if (!Boot_Flag || !water_status_ready || sensor_pending_tanks ||
            sensor_alarm_pending ||
            (type >= SENSORTYPE_WATER4_20MA && !sensor_sample_ready)) {
            *quality = 2;
            return 0;
        }
        if (sys.shang_shuiwei > 2) {
            *quality = 3;
            return 0;
        }
        flags = (sys.sensor_err ? 1 : 0) | (sys.Sensor_OverFlow ? 2 : 0) |
                (sys.flowover ? 4 : 0) | (Dis8_Flash == 1 ? 16 : 0);
        if (Dis7_Flash == 1 || sys.shang_shuiwei == 2) {
            flags |= 8;
        }
        return flags;
    }

    if (type < SENSORTYPE_WATER4_20MA) {
        *quality = 1;
        return 0;
    }
    if (!sensor_sample_ready || (number == 0x15 && (!Boot_Flag || sensor_pending_tanks))) {
        *quality = 2;
        return 0;
    }
    if (sys.sensor_err || (number == 0x15 && sys.SensorLever > 2)) {
        *quality = 3;                // 断线等异常分支清零的结果不是有效零。
        return 0;
    }
    return number == 0x10 ? sys.SensorOut : (u32)sys.SensorLever;
}

/******************************************************************************
 * 函数名称       : bf4g_run_read
 * 功能描述       : 按运行字段号读取其值和对应质量。
 * 输入参数       : number，字段号；quality，质量输出位置；snapshot，本次诊断来源。
 * 返回值         : 无效值为零，异常故障位图保留原值。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u32 bf4g_run_read(u8 number, u8 *quality,
                         const bf4g_diagnostics_t xdata *snapshot) //20261116-12 change
{
    const u16 xdata *measurement;
    u32 faults;
    u16 status;
    u8 level;

    *quality = 0;
    if (number <= 0x07) {
        switch (number) {
        case 1:
            measurement = &sys.Voltage_Line;
            break;
        case 2:
            measurement = &sys.Current_A1;
            break;
#if (Model_OUT == Model_1C2)
        case 5:
            measurement = &sys.Current_A2;
            break;
#endif
#if (Model_Voltage == Model_3PH)
        case 3:
            measurement = &sys.Current_B1;
            break;
        case 4:
            measurement = &sys.Current_C1;
            break;
#if (Model_OUT == Model_1C2)
        case 6:
            measurement = &sys.Current_B2;
            break;
        case 7:
            measurement = &sys.Current_C2;
            break;
#endif
#endif
        default:
            *quality = 1;
            return 0;
        }

        if (number == 1 ? !voltage_sample_ready : !current_sample_ready) {
            *quality = 2;
            return 0;
        }
        return bf4g_adc_value(measurement);
    }

    switch (number) {
    case 0x08:
        status = bf4g_run_status(snapshot->mode, quality);
        return *quality ? 0 : status; //20261116-8 change 带质量的监测项在异常时传零。
    case 0x09:
    case 0x0a:
    case 0x0b:
        if (number == 0x0b) {
            *quality = bf4g_tank_quality();
            if (*quality != 0) {
                return 0;
            }
        }
        faults = snapshot->faults;
        if (!bf4g_fault_valid(faults)) {
            *quality = 3;
            return number == 0x0a ? faults : 0;
        }
        if (number == 0x0a) {
            return faults;
        }
        return number == 0x09 ? bf4g_first_fault(faults) : bf4g_alarm(faults);
    case 0x0c:
    case 0x0d:
        if (!config_loaded || !water_status_ready || !Boot_Flag ||
            (sensor_pending_tanks & (number == 0x0c ? SENSOR_PENDING_UPPER : SENSOR_PENDING_LOWER))) {
            *quality = 2;
            return 0;
        }
        if (number == 0x0c) {
            return (LCD_WATER_Hlevel1 ? 1 : 0) | (LCD_WATER_Hlevel2 ? 2 : 0) |
                   (LCD_WATER_Hlevel3 ? 4 : 0) | (LCD_WATER_Hlevel4 ? 8 : 0) |
                   (LCD_WATER_Hlevel5 ? 16 : 0) | (LCD_WATER_Hlevel6 ? 32 : 0);
        }
        return (LCD_WATER_Llevel1 ? 1 : 0) | (LCD_WATER_Llevel2 ? 2 : 0) |
               (LCD_WATER_Llevel3 ? 4 : 0) | (LCD_WATER_Llevel4 ? 8 : 0) |
               (LCD_WATER_Llevel5 ? 16 : 0) | (LCD_WATER_Llevel6 ? 32 : 0);
    case 0x0e:
    case 0x0f:
        if (!config_loaded || !water_status_ready ||
            (sensor_pending_tanks & (number == 0x0e ? SENSOR_PENDING_UPPER : SENSOR_PENDING_LOWER))) {
            *quality = 2;
            return 0;
        }
        level = number == 0x0e ? sys.shang_shuiwei : sys.xia_shuiwei;
        if (level > 2) {
            *quality = 3;
            return 0;
        }
        return level;
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x14:
    case 0x15:
        return bf4g_sensor_read(number, quality);
    case 0x13:
        if (!config_loaded) {
            *quality = 2;
            return 0;
        }
        if (config.Sensor_Mode > CTROLTYPE_DRAIN) {
            *quality = 3;
            return 0;
        }
        return config.Sensor_Mode;
    case 0x16:
        if (!config_loaded) {
            *quality = 2;
            return 0;
        }
        return bf4g_model_word();
    case 0x17:
        return MODBUS_NEW_VERSION;
    default:
        *quality = 1;
        return 0;                   // 功率没有采样来源。
    }
}

/******************************************************************************
 * 函数名称       : bf4g_put_u32
 * 功能描述       : 将三十二位数据按高字节在前写入应答。
 * 输入参数       : out，写入位置；value，要写入的值。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void bf4g_put_u32(u8 xdata *out, u32 value) //20261116-2 add
{
    bf4g_put_u16(out, (u16)(value >> 16));
    bf4g_put_u16(out + 2, (u16)value);
}

/******************************************************************************
 * 函数名称       : bf4g_fault_sample
 * 功能描述       : 读取与普通查询相同的活动故障位图及质量。
 * 输入参数       : faults，位图输出位置。
 * 返回值         : 样本有效时为一。
 * 日期            : 2026.11.24
 ******************************************************************************/
bit bf4g_fault_sample(u32 *faults) //20261124-3 add
{
    bf4g_diagnostics_t xdata snapshot;
    u8 quality;

    bf4g_capture_diagnostics(&snapshot);
    *faults = bf4g_run_read(0x0a, &quality, &snapshot);
    return quality == 0;
}

/******************************************************************************
 * 函数名称       : bf4g_fault_encode
 * 功能描述       : 封装全量活动故障事件，保留创建时的地址。
 * 输入参数       : frame，上报缓冲；boot，启动编号；seq，报文序号；kind，类别；faults，有效位图。
 * 返回值         : None
 * 日期            : 2026.11.24
 ******************************************************************************/
void bf4g_fault_encode(u8 xdata *frame, u32 boot, u32 seq, u8 kind, u32 faults) //20261124-3 add
{
    u16 crc;

    frame[0] = config.Dev_Address;
    frame[1] = 0x43;
    frame[2] = 0xbf;
    frame[3] = 0x06;
    frame[4] = BF4G_FMT;
    frame[5] = 0x20;
    bf4g_put_u16(frame + 6, (u16)(1 + (seq - 1) % 65535UL)); // 序号跨过关联号上限后重新从一开始。
    frame[8] = 13;
    bf4g_put_u32(frame + BF4G_DATA, boot);
    bf4g_put_u32(frame + BF4G_DATA + 4, seq);
    frame[17] = kind;
    bf4g_put_u32(frame + 18, faults);
    crc = bf4g_crc(frame, BF4G_FAULT_FRAME_LENGTH - 2);
    frame[22] = crc;
    frame[23] = crc >> 8;
}

/******************************************************************************
 * 函数名称       : bf4g_fault_ack_matches
 * 功能描述       : 完整核对接收确认是否对应唯一待确认的主动帧。
 * 输入参数       : frame，收到的报文；length，实收长度；event，待确认原帧。
 * 返回值         : 全部字段匹配时为一。
 * 日期            : 2026.11.24
 ******************************************************************************/
bit bf4g_fault_ack_matches(const u8 xdata *frame, u8 length, const u8 xdata *event) //20261124-0 add
{
    u8 i;

    if (length != 20 || frame[5] != 0xa0 || frame[8] != 9 || frame[9] != 0 ||
        frame[6] != event[6] || frame[7] != event[7]) {
        return 0;
    }
    for (i = 0; i < 5; i++) {
        if (frame[i] != event[i]) {
            return 0;                   // 同时校验地址、公共头和格式。
        }
    }
    for (i = 0; i < 8; i++) {
        if (frame[10 + i] != event[BF4G_DATA + i]) {
            return 0;                   // 回显的启动编号和序号必须完全相同。
        }
    }
    return bf4g_crc(frame, length) == 0;
}

/******************************************************************************
 * 函数名称       : bf4g_read_runtime
 * 功能描述       : 校验运行字段区间并写入每项质量和三十二位值。
 * 输入参数       : out，请求及应答数据位置；length，请求数据长度。
 * 返回值         : 应答数据长度。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u8 bf4g_read_runtime(u8 xdata *out, u8 length) //20261116-12 change
{
    u8 start;
    u8 count;
    u8 i;
    u8 quality;
    u32 value;
    bf4g_diagnostics_t xdata snapshot;

    if (length != 2) {
        return bf4g_error(out, 3, length ? out[0] : 0xff);
    }
    start = out[0];
    count = out[1];
    if (start == 0 || start > 0x18) {
        return bf4g_error(out, 4, start);
    }
    if (count == 0 || count > 9 || count > 0x19 - start) {
        return bf4g_error(out, 3, start);
    }

    out[0] = 0;
    out[1] = start;
    out[2] = count;
    bf4g_capture_diagnostics(&snapshot);
    for (i = 0; i < count; i++) {
        value = bf4g_run_read(start + i, &quality, &snapshot);
        out[3 + i * 5] = quality;
        bf4g_put_u32(out + 4 + i * 5, value);
    }
    return 3 + count * 5;
}

/******************************************************************************
 * 函数名称       : bf4g_read_history
 * 功能描述       : 按区间读取故障存储槽，保留欠压零号和异常原值。
 * 输入参数       : out，请求及应答数据位置；length，请求数据长度。
 * 返回值         : 应答数据长度。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u8 bf4g_read_history(u8 xdata *out, u8 length) //20261116-3 add
{
    u8 start;
    u8 count;
    u8 slot;
    u8 number;
    u8 quality;
    u16 voltage;
    u16 current;
    u8 xdata *entry;

    if (length != 2) {
        return bf4g_error(out, 3, length ? out[0] : 0xff);
    }
    start = out[0];
    count = out[1];
    if (start >= 10) {
        return bf4g_error(out, 4, start);
    }
    if (count == 0 || count > 8 || count > 10 - start) {
        return bf4g_error(out, 3, start);
    }

    out[0] = 0;
    out[1] = start;
    out[2] = count;
    entry = out + 3;
    for (slot = start; slot < start + count; slot++) {
        number = ERR_NUMB;
        voltage = 0;
        current = 0;
        quality = 2;
        if (config_loaded) {
            number = config.Record_Type[slot];
            if (number == ERR_NUMB) {
                quality = 4;         // 仅原故障号十七代表空槽。
            } else {
                quality = number > ERR_NUMB ? 3 : 0;
                voltage = config.Record_Dat1[slot];
                current = config.Record_Dat2[slot];
            }
        }
        entry[0] = quality;
        entry[1] = number;
        bf4g_put_u16(entry + 2, voltage);
        bf4g_put_u16(entry + 4, current);
        entry += 6;
    }
    return 3 + count * 6;
}

/******************************************************************************
 * 函数名称       : bf4g_invalidate_sensor_level
 * 功能描述       : 在连续类型下标记本次阈值改动所影响的水池。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void bf4g_invalidate_sensor_level(void) //20261116-6 add
{
    if (config.Auto_Mode >= SENSORTYPE_WATER4_20MA &&
        config.Auto_Mode <= SENSORTYPE_PRESS0_5V) {
        sensor_pending_tanks |= config.Sensor_Mode == CTROLTYPE_PUMP ?
                                SENSOR_PENDING_UPPER : SENSOR_PENDING_LOWER;
    }
}

/******************************************************************************
 * 函数名称       : bf4g_set_parameter
 * 功能描述       : 只给通过校验的单个参数赋值。
 * 输入参数       : number，参数号；value，已经校验的新值。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void bf4g_set_parameter(u8 number, u16 value) //20261116-6 change
{
    switch (number) {
    case 0:
        config.OVP_Value = value;
        break;
    case 1:
        config.UVP_Value = value;
        break;
    case 2:
        config.Set_STANDBY_Current1 = value;
        break;
    case 3:
        config.Set_OVERLOAD_Current1 = value;
        break;
    case 4:
        config.Set_STANDBY_Current2 = value;
        break;
    case 5:
        config.Set_OVERLOAD_Current2 = value;
        break;
    case 6: //20261116-5 add 五项新计时参数直接更新，保留已累计时间。
        config.AUTO_Timing_Run = value;
        break;
    case 7:
        config.AUTO_Timing_Stop = value;
        break;
    case 8:
        config.Timer_Standby_Restart = value;
        break;
    case 9:
        config.Timer_Unload_Restart = value;
        break;
    case 10:
        config.AUTO_FANGXIU = value;
        break;
    case 11: //20261116-7 add 应答仍使用共享帧中的原请求地址。
        config.Dev_Address = (u8)value;
        break;
    case 12:
        config.Pump_AB_Mode = (u8)value;
        break;
    case 13: //20261116-6 add 方向变化后两侧水池的旧状态均须重算。
        config.Sensor_Mode = (u8)value;
        sensor_pending_tanks = SENSOR_PENDING_UPPER | SENSOR_PENDING_LOWER;
        break;
    case 14:
        config.Sensor_Rang = value;
        sys.Sensor_Rang = (u32)value * 10UL;
        sensor_sample_ready = 0;    // 原始采样仍可用于按新量程换算。
        bf4g_invalidate_sensor_level();
        break;
    case 15:
        config.Sensor_STOP = value;
        sys.Sensor_STOP = (u32)value * 10UL;
        bf4g_invalidate_sensor_level();
        break;
    case 16:
        config.Sensor_ON1 = value;
        sys.Sensor_ON1 = (u32)value * 10UL;
        bf4g_invalidate_sensor_level();
        break;
    case 17:
        config.Sensor_ON2 = value;
        sys.Sensor_ON2 = (u32)value * 10UL;
        bf4g_invalidate_sensor_level();
        break;
    case 18:
        sensor_adc_fresh = 0;       // 下一批采样到达前保持旧工程值失效。
        sensor_sample_ready = 0;
        sensor_pending_tanks = SENSOR_PENDING_UPPER | SENSOR_PENDING_LOWER;
        sys.SensorLever = 0;
        sys.Sensor_OverFlow = 0;
        config.Auto_Mode = (u8)value;
        break;
    case 19:
        config.fault_mask = (u8)value;
        break;
    case 20:
        config.Sensor_overFlow = value;
        if (sensor_sample_ready && !sys.sensor_err && sys.Auto_Mode == config.Auto_Mode) {
            sensorOverFlowCheck(); // 已有有效工程值，立即按新阈值更新报警。
            sensor_alarm_pending = 0;
        } else {
            sensor_alarm_pending = 1;
        }
        break;
    }
}

/******************************************************************************
 * 函数名称       : bf4g_write_parameter
 * 功能描述       : 校验单写请求，发布目标参数并返回当前内存值。
 * 输入参数       : out，请求及应答数据位置；length，请求数据长度。
 * 返回值         : 应答数据长度。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u8 bf4g_write_parameter(u8 xdata *out, u8 length) //20261116-4 add
{
    u8 number;
    u16 value;
    u16 minimum;
    u16 maximum;
    bit interrupts;

    if (length != 3) {
        return bf4g_error(out, 3, length ? out[0] : 0xff);
    }
    number = out[0];
    if (number >= 23) {
        return bf4g_error(out, 4, number);
    }
    if (number >= 21) {
        return bf4g_error(out, 5, number);
    }
    if (!bf4g_param_applicable(number)) {
        return bf4g_error(out, 6, number);
    }
    if (!config_loaded) {
        return bf4g_error(out, 0x0e, number);
    }

    value = ((u16)out[1] << 8) | out[2];
    minimum = bf4g_param_minimum(number);
    maximum = bf4g_param_maximum(number);
    if (value < minimum || value > maximum) {
        bf4g_error(out, 7, number);
        bf4g_put_u32(out + 2, minimum);
        bf4g_put_u32(out + 6, maximum);
        return 10;
    }

    if (value != bf4g_param_value(number)) {
        // 在一次短临界区发布单个参数及其保存标志。
        interrupts = EA;
        EA = 0;
        bf4g_set_parameter(number, value);
        sys.save_flag = 1;
        EA = interrupts;
    }
    out[0] = 0;
    out[1] = number;
    bf4g_put_u16(out + 2, bf4g_param_value(number));
    return 4;
}

/******************************************************************************
 * 函数名称       : bf4g_start_blocked
 * 功能描述       : 按现有故障、锁定及滤波触点输入判断启动条件。
 * 输入参数       : target，实际目标泵位图。
 * 返回值         : 有任何目标不能启动时为一。
 * 日期            : 2026.11.16
 ******************************************************************************/
static bit bf4g_start_blocked(u8 target) //20261116-8 add
{
    if ((target & 1) &&
        (config.Lock1 || sys.Fault_A_Flag || check_A_Fault() ||
         sys.Pump_Add_Flag || sys.Contact_Overload_A_Flag)) {
        return 1;
    }
#if (Model_OUT == Model_1C2)
    if ((target & 2) &&
        (config.Lock2 || sys.Fault_B_Flag || check_B_Fault() ||
         sys.Flag_OverFlow || sys.Contact_Overload_B_Flag)) {
        return 1;
    }
#endif
    return 0;
}

/******************************************************************************
 * 函数名称       : bf4g_control
 * 功能描述       : 校验并执行控制动作，交由正常扫描确认结果。
 * 输入参数       : out，请求及应答数据位置；length，请求数据长度。
 * 返回值         : 拒绝应答的数据长度，或待确认标记。
 * 日期            : 2026.11.17
 ******************************************************************************/
static u8 bf4g_control(u8 xdata *out, u8 length) //20261117-7 change
{
    u8 action;
    u8 target;
    u8 pumps;
    u8 mode;
    bit interrupts;

    if (length != 2) {
        return bf4g_error(out, 3, length ? out[0] : 0xff);
    }
    action = out[0];
    target = out[1];
    if (action == 0 || action > 6 ||
        (action <= 2 ? (target == 0 || target > 3) : target != 0)) {
        return bf4g_error(out, 4, action);
    }
    if (!config_loaded || !Boot_Flag) {
        return bf4g_error(out, 0x0e, action); //20261117-7 change 正常控制扫描尚不可用时不执行。
    }

    if (action <= 2) {
        pumps = target;
#if (Model_OUT == Model_1C1)
        if (target == 2) {
            return bf4g_error(out, 6, action);
        }
        pumps = 1;                   // 一控一的全部目标等同于唯一水泵。
#endif
        interrupts = EA;
        EA = 0;
        if (config.Hand_Auto_timer_Flag != RUNMODE_MANUAL) {
            EA = interrupts;
            return bf4g_error(out, 8, action);
        }
        if (action == 1 && bf4g_start_blocked(pumps)) {
            EA = interrupts;
            return bf4g_error(out, 9, action);
        }
        // 所有目标通过检查后再一次发布意图，继电器由原本地扫描控制。
        if (pumps & 1) {
            sys.Pump_A_ON = action == 1;
        }
#if (Model_OUT == Model_1C2)
        if (pumps & 2) {
            sys.Pump_B_ON = action == 1;
        }
#endif
        EA = interrupts;
    } else if (action == 6) {
        interrupts = EA;
        EA = 0;                      // 同一故障清除例程也由遥控中断调用。
        clear_A_Fault();
#if (Model_OUT == Model_1C2)
        clear_B_Fault();
#endif
        Contact_Overload_Control(); // 持续输入及刚确认的触点状态仍应锁存。
        EA = interrupts;
    } else {
        mode = action - 3;
        interrupts = EA;
        EA = 0;
        if (config.Hand_Auto_timer_Flag != mode) {
            config.Hand_Auto_timer_Flag = mode;
            if (mode == RUNMODE_TIMER) {
                sys.autoTimerFlag = 1;
                sys.Auto_timer_count = 0;
            }
            sys.save_flag = 1;
        }
        EA = interrupts;
    }

    return BF4G_REPLY_PENDING; //20261117-7 add 统一在正常扫描后确认，故障清除返回剩余故障。
}

/******************************************************************************
 * 函数名称       : bf4g_summary_put
 * 功能描述       : 读取单项来源并就地编码对应的质量与紧凑值。
 * 输入参数       : qualities，质量区；entry，值位置；index，项目序号；width，值宽度；source，来源编号；snapshot，本次诊断来源。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void bf4g_summary_put(u8 xdata *qualities, u8 xdata *entry, u8 index,
                             u8 width, u8 source,
                             const bf4g_diagnostics_t xdata *snapshot) //20261116-12 change
{
    u8 quality;
    u32 value;

    if (source & 0x80) {
        value = bf4g_param_read(source & 0x7f, &quality);
    } else {
        value = bf4g_run_read(source, &quality, snapshot);
    }
    qualities[index >> 2] |= quality << ((index & 3) * 2);
    if (width == 1) {
        entry[0] = (u8)value;
    } else if (width == 2) {
        bf4g_put_u16(entry, (u16)value);
    } else {
        bf4g_put_u32(entry, value);
    }
}

/******************************************************************************
 * 函数名称       : bf4g_main_fields
 * 功能描述       : 按紧凑布局编码主控页二十四项，复用分段读取的数据源。
 * 输入参数       : qualities，质量区；values，紧凑值区；snapshot，本次诊断来源。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void bf4g_main_fields(u8 xdata *qualities, u8 xdata *values,
                             const bf4g_diagnostics_t xdata *snapshot) //20261116-12 change
{
    // 组帧复用本次捕获的故障和模式；其他关联状态在主循环中保持一致。
    // 电压及各相电流分别读取。
    bf4g_summary_put(qualities, values +  0,  0, 2, 0x01, snapshot);
    bf4g_summary_put(qualities, values +  2,  1, 2, 0x02, snapshot);
    bf4g_summary_put(qualities, values +  4,  2, 2, 0x03, snapshot);
    bf4g_summary_put(qualities, values +  6,  3, 2, 0x04, snapshot);
    bf4g_summary_put(qualities, values +  8,  4, 2, 0x08, snapshot);
    bf4g_summary_put(qualities, values + 10,  5, 4, 0x0a, snapshot);
    bf4g_summary_put(qualities, values + 14,  6, 1, 0x0c, snapshot);
    bf4g_summary_put(qualities, values + 15,  7, 1, 0x0d, snapshot);
    bf4g_summary_put(qualities, values + 16,  8, 1, 0x0e, snapshot);
    bf4g_summary_put(qualities, values + 17,  9, 1, 0x0f, snapshot);
    bf4g_summary_put(qualities, values + 18, 10, 4, 0x10, snapshot);
    bf4g_summary_put(qualities, values + 22, 11, 1, 0x11, snapshot);
    bf4g_summary_put(qualities, values + 23, 12, 1, 0x12, snapshot);
    bf4g_summary_put(qualities, values + 24, 13, 1, 0x13, snapshot);
    bf4g_summary_put(qualities, values + 25, 14, 2, BF4G_PARAM_SOURCE(14), snapshot);
    bf4g_summary_put(qualities, values + 27, 15, 2, BF4G_PARAM_SOURCE(16), snapshot);
    bf4g_summary_put(qualities, values + 29, 16, 2, BF4G_PARAM_SOURCE(15), snapshot);
    bf4g_summary_put(qualities, values + 31, 17, 2, BF4G_PARAM_SOURCE(20), snapshot);
    bf4g_summary_put(qualities, values + 33, 18, 2, BF4G_PARAM_SOURCE(0), snapshot);
    bf4g_summary_put(qualities, values + 35, 19, 2, BF4G_PARAM_SOURCE(1), snapshot);
    bf4g_summary_put(qualities, values + 37, 20, 2, BF4G_PARAM_SOURCE(3), snapshot);
    bf4g_summary_put(qualities, values + 39, 21, 2, BF4G_PARAM_SOURCE(2), snapshot);
    bf4g_summary_put(qualities, values + 41, 22, 2, BF4G_PARAM_SOURCE(6), snapshot);
    bf4g_summary_put(qualities, values + 43, 23, 2, BF4G_PARAM_SOURCE(7), snapshot);
}

/******************************************************************************
 * 函数名称       : bf4g_read_summary
 * 功能描述       : 按命令组装主控或网页查询应答，复用前二十四项的紧凑布局。
 * 输入参数       : out，应答数据位置；length，请求数据长度；extended，是否追加双泵及诊断项。
 * 返回值         : 应答数据长度。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u8 bf4g_read_summary(u8 xdata *out, u8 length, bit extended) //20261116-12 change
{
    u8 i;
    u8 value_start = extended ? 11 : 8;
    u8 reserve_start = extended ? 76 : 53;
    u8 xdata *qualities = out + 2;
    u8 xdata *values = out + value_start;
    bf4g_diagnostics_t xdata snapshot;

    if (length != 0) {
        return bf4g_error(out, 3, 0xff);
    }
    out[0] = 0;
    out[1] = (Model_Voltage == Model_3PH ? 1 : 0) |
             (Model_OUT == Model_1C2 ? 2 : 0) |
             (VoltageValue == Vol_380 ? 4 : 0);
    for (i = 2; i < value_start; i++) {
        out[i] = 0;                 // 较长格式最后一个质量字节的保留位也从零开始。
    }
    bf4g_capture_diagnostics(&snapshot);
    bf4g_main_fields(qualities, values, &snapshot);
    if (extended) {
        // 两组都取主循环现有的相同配置和诊断来源；各相采样逐项读取。
        bf4g_summary_put(qualities, values + 45, 24, 2, 0x05, &snapshot);
        bf4g_summary_put(qualities, values + 47, 25, 2, 0x06, &snapshot);
        bf4g_summary_put(qualities, values + 49, 26, 2, 0x07, &snapshot);
        bf4g_summary_put(qualities, values + 51, 27, 2, BF4G_PARAM_SOURCE(4), &snapshot);
        bf4g_summary_put(qualities, values + 53, 28, 2, BF4G_PARAM_SOURCE(5), &snapshot);
        bf4g_summary_put(qualities, values + 55, 29, 1, BF4G_PARAM_SOURCE(12), &snapshot);
        bf4g_summary_put(qualities, values + 56, 30, 2, BF4G_PARAM_SOURCE(17), &snapshot);
        bf4g_summary_put(qualities, values + 58, 31, 2, 0x16, &snapshot);
        bf4g_summary_put(qualities, values + 60, 32, 2, 0x17, &snapshot);
        bf4g_summary_put(qualities, values + 62, 33, 2, 0x0b, &snapshot);
        bf4g_summary_put(qualities, values + 64, 34, 1, 0x15, &snapshot);
    }
    for (i = reserve_start; i < reserve_start + 10; i++) {
        out[i] = 0;                 // 每次发送前在共享帧内覆盖保留字节。
    }
    return reserve_start + 10;
}

/******************************************************************************
 * 函数名称       : bf4g_seal_reply
 * 功能描述       : 保留请求的地址和关联号，填写应答长度和校验。
 * 输入参数       : frame，共享报文；data_length，数据字节数。
 * 返回值         : 应答帧字节数。
 * 日期            : 2026.11.17
 ******************************************************************************/
static u8 bf4g_seal_reply(u8 xdata *frame, u8 data_length) //20261117-7 add
{
    u8 length = BF4G_DATA + data_length;
    u16 crc;

    frame[4] = BF4G_FMT;
    frame[5] |= 0x80;
    frame[8] = data_length;
    crc = bf4g_crc(frame, length);
    frame[length++] = crc;
    frame[length++] = crc >> 8;
    return length;
}

/******************************************************************************
 * 函数名称       : bf4g_control_result
 * 功能描述       : 从冻结请求及扫描后的诊断状态生成最终控制应答。
 * 输入参数       : frame，共享报文；timed_out，等待是否已到期。
 * 返回值         : 应答帧字节数；仍需等待时为零。
 * 日期            : 2026.11.17
 ******************************************************************************/
u8 bf4g_control_result(u8 xdata *frame, bit timed_out) //20261117-7 add
{
    u8 xdata *out = frame + BF4G_DATA;
    u8 action = out[0];
    u8 target = out[1];
    u8 mask;
    u8 quality;
    u16 state;
    bit interrupts = EA;
    bf4g_diagnostics_t xdata snapshot;

    if (timed_out) {
        return bf4g_seal_reply(frame, bf4g_error(out, 0x0a, action));
    }

    EA = 0;
    bf4g_capture_diagnostics(&snapshot);
    state = bf4g_run_status(snapshot.mode, &quality);
    EA = interrupts;

    if (action != 6 && snapshot.mode != (action <= 2 ? RUNMODE_MANUAL : action - 3)) {
        return bf4g_seal_reply(frame, bf4g_error(out, 0x0a, action));
    }
    if (action <= 2) {
#if (Model_OUT == Model_1C2)
        mask = target << 4;
#else
        mask = 0x10;                   // 一控一的全部目标只检查唯一水泵。
#endif
        if ((state & mask) != (action == 1 ? mask : 0)) {
            return 0;                  // 目标位尚未全部达到要求，等待后续扫描。
        }
    }

    out[0] = 0;
    out[1] = action;
    out[2] = target;
    bf4g_put_u16(out + 3, state);
    bf4g_put_u32(out + 5, snapshot.faults);
    return bf4g_seal_reply(frame, 9);
}

/******************************************************************************
 * 函数名称       : bf4g_reply
 * 功能描述       : 校验串口请求，立即组帧或保留已受理的待确认控制。
 * 输入参数       : frame，共享报文缓冲；length，实收字节数。
 * 返回值         : 应答字节数，静默为零，待确认返回专用标记。
 * 日期            : 2026.11.16
 ******************************************************************************/
u8 bf4g_reply(u8 xdata *frame, u8 length) //20261117-7 change
{
    u8 xdata *out;
    u8 data_length;
    u8 object;

    if (length < BF4G_OVERHEAD || frame[1] != 0x43 ||
        frame[2] != 0xbf || frame[3] != 0x06 || (frame[5] & 0x80) ||
        bf4g_crc(frame, length) != 0) {
        return 0;                         // 头或边界无法确认时静默丢弃。
    }

    out = frame + BF4G_DATA;
    data_length = frame[8];
    if (frame[4] != BF4G_FMT) {
        if (data_length != length - BF4G_OVERHEAD) {
            return 0;                    // 异格式只应答能按公共头辨认的帧。
        }
        data_length = bf4g_error(out, 2, 0xff);
    } else if (data_length != length - BF4G_OVERHEAD || (frame[6] | frame[7]) == 0) {
        object = ((frame[5] >= 2 && frame[5] <= 4) || frame[5] == 0x10 || frame[5] == 0x11) &&
                 data_length &&
                 length > BF4G_OVERHEAD ? out[0] : 0xff;
        data_length = bf4g_error(out, 3, object);
    } else {
        switch (frame[5]) {
        case 1:
            if (data_length != 0) {
                data_length = bf4g_error(out, 3, 0xff);
            } else {
                bf4g_device_info(out);
                data_length = 16;
            }
            break;
        case 2:
            data_length = bf4g_read_parameters(out, data_length);
            break;
        case 3:
            data_length = bf4g_read_runtime(out, data_length);
            break;
        case 4:
            data_length = bf4g_read_history(out, data_length);
            break;
        case 5:
        case 6:
            data_length = bf4g_read_summary(out, data_length, frame[5] == 6);
            break;
        case 0x10:
            data_length = bf4g_write_parameter(out, data_length);
            break;
        case 0x11:
            data_length = bf4g_control(out, data_length);
            break;
        default:
            data_length = bf4g_error(out, 1, 0xff);
            break;
        }
    }

    if (data_length == BF4G_REPLY_PENDING) {
        return BF4G_REPLY_PENDING;
    }
    return bf4g_seal_reply(frame, data_length);
}
