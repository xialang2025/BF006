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
