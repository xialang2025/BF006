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
#define BF4G_DATA         9
#define BF4G_OVERHEAD     11
#define BF4G_CAPABILITIES 0x0005 //20261116-3 change 已验证的参数和历史读取能力。

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
    out[1] = 0;
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
static u8 bf4g_tank_quality(void) //20261116-2 add
{
    if (!config_loaded || !water_status_ready) {
        return 2;
    }
    return (sys.shang_shuiwei > 2 || sys.xia_shuiwei > 2) ? 3 : 0;
}

/******************************************************************************
 * 函数名称       : bf4g_run_status
 * 功能描述       : 编制模式、运行、主备和两水池档位的状态位。
 * 输入参数       : quality，质量输出位置。
 * 返回值         : 运行状态位图。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u16 bf4g_run_status(u8 *quality) //20261116-2 add
{
    static const u8 code mode_bits[] = {1, 6, 10};
    u16 state;
    u8 mode = config.Hand_Auto_timer_Flag;
    u8 priority = config.Pump_AB_Mode;

    *quality = bf4g_tank_quality();
    if (*quality != 0) {
        return 0;
    }
    if (mode > RUNMODE_TIMER || (Model_OUT == Model_1C2 && priority > 2)) {
        *quality = 3;
        return 0;
    }

    state = mode_bits[mode];
    if (sys.Pump_A_state) {
        state |= 0x0010;
    }
#if (Model_OUT == Model_1C2)
    if (sys.Pump_B_state) {
        state |= 0x0020;
    }
    state |= (u16)0x0040 << priority;
#endif

    state |= (u16)0x0200 << sys.shang_shuiwei;
    state |= (u16)0x1000 << sys.xia_shuiwei;
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
static u32 bf4g_sensor_read(u8 number, u8 *quality) //20261116-2 add
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
        if (!Boot_Flag || !water_status_ready ||
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
    if (!sensor_sample_ready || (number == 0x15 && !Boot_Flag)) {
        *quality = 2;
        return 0;
    }
    if (sys.sensor_err || (number == 0x15 && sys.SensorLever > 2)) {
        *quality = 3;                // 断线等异常分支清零的结果不是有效零。
        return 0;
    }
    return number == 0x10 ? (u32)sys.SensorOut : (u32)sys.SensorLever;
}

/******************************************************************************
 * 函数名称       : bf4g_run_read
 * 功能描述       : 按运行字段号读取其值和对应质量。
 * 输入参数       : number，已经校验的字段号；quality，质量输出位置。
 * 返回值         : 无效值为零，异常故障位图保留原值。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u32 bf4g_run_read(u8 number, u8 *quality) //20261116-2 add
{
    const u16 xdata *measurement;
    u32 faults;
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
        return bf4g_run_status(quality);
    case 0x09:
    case 0x0a:
    case 0x0b:
        if (number == 0x0b) {
            *quality = bf4g_tank_quality();
            if (*quality != 0) {
                return 0;
            }
        }
        faults = sys.Fault_Error;
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
        if (!config_loaded || !water_status_ready || !Boot_Flag) {
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
        if (!config_loaded || !water_status_ready) {
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
 * 函数名称       : bf4g_read_runtime
 * 功能描述       : 校验运行字段区间并写入每项质量和三十二位值。
 * 输入参数       : out，请求及应答数据位置；length，请求数据长度。
 * 返回值         : 应答数据长度。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u8 bf4g_read_runtime(u8 xdata *out, u8 length) //20261116-2 add
{
    u8 start;
    u8 count;
    u8 i;
    u8 quality;
    u32 value;

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
    for (i = 0; i < count; i++) {
        value = bf4g_run_read(start + i, &quality);
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
 * 函数名称       : bf4g_set_parameter
 * 功能描述       : 只给通过校验的单个参数赋值。
 * 输入参数       : number，参数号；value，已经校验的新值。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void bf4g_set_parameter(u8 number, u16 value) //20261116-4 add
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
    case 12:
        config.Pump_AB_Mode = (u8)value;
        break;
    case 19:
        config.fault_mask = (u8)value;
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
    if (number > 5 && number != 12 && number != 19) {
        return bf4g_error(out, 1, number);
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
 * 函数名称       : bf4g_reply
 * 功能描述       : 校验串口请求，就地改写为应答，最后计算校验值。
 * 输入参数       : frame，共享报文缓冲；length，实收字节数。
 * 返回值         : 应答字节数，静默丢弃为零。
 * 日期            : 2026.11.16
 ******************************************************************************/
u8 bf4g_reply(u8 xdata *frame, u8 length) //20261116-4 change
{
    u8 xdata *out;
    u8 data_length;
    u8 object;
    u16 crc;

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
        object = ((frame[5] >= 2 && frame[5] <= 4) || frame[5] == 0x10) && data_length &&
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
        case 0x10:
            data_length = bf4g_write_parameter(out, data_length);
            break;
        default:
            data_length = bf4g_error(out, 1, 0xff);
            break;
        }
    }

    frame[4] = BF4G_FMT;
    frame[5] |= 0x80;
    frame[8] = data_length;
    length = BF4G_DATA + data_length;
    crc = bf4g_crc(frame, length);
    frame[length++] = crc;
    frame[length++] = crc >> 8;
    return length;
}
