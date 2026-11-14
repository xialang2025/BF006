#include "system.h"
#include "key.h"
#include "Display.h"
#include "HT1622.h"
#include "user.h"
#include "configPar.h"
#include "config_store.h" //20261114-4 add

#define PAR_NUMB 23 //20260430-2 change

u16 Limit_Current = 150;

#if (VoltageValue == Vol_380)
code u16 Value_UVP = 85;
code u16 Value_OVP = 480;
#else
code u16 Value_UVP = 85;
code u16 Value_OVP = 280;
#endif

code u8 modebus_Max = 255;
code u8 modebus_Min = 1;
code u8 mode_Max = 4;
code u8 fault_mask_max = 4;
code u8 PUMP_MODE_Max = 2;
code u8 SENSOR_MODE_Max = 1;
code u16 Par_Max = 9999;
code u16 Par_Max2 = 999;
code u16 Par_Min = 0;

code Par_t par[PAR_NUMB] =
    {
        {&config.OVP_Value, &Value_OVP, &Value_UVP, sizeof(u16), EM_VOl},                 // 1
        {&config.UVP_Value, &Value_OVP, &Value_UVP, sizeof(u16), EM_VOl},                 // 2
        {&config.Set_STANDBY_Current1, &Limit_Current, &Par_Min, sizeof(u16), EM_CUR},    // 3
        {&config.Set_OVERLOAD_Current1, &Limit_Current, &Par_Min, sizeof(u16), EM_CUR},   // 4
        {&config.Set_STANDBY_Current2, &Limit_Current, &Par_Min, sizeof(u16), EM_CUR},    // 5
        {&config.Set_OVERLOAD_Current2, &Limit_Current, &Par_Min, sizeof(u16), EM_CUR},   // 6
        {&config.AUTO_Timing_Run, &Par_Max, &Par_Min, sizeof(u16), EM_TIME},              // 7
        {&config.AUTO_Timing_Stop, &Par_Max, &Par_Min, sizeof(u16), EM_TIME},             // 8
        {&config.Timer_Standby_Restart, &Par_Max, &Par_Min, sizeof(u16), EM_TIME},        // 9
        {&config.Timer_Unload_Restart, &Par_Max, &Par_Min, sizeof(u16), EM_TIME},         // 10
        {&config.AUTO_FANGXIU, &Par_Max2, &Par_Min, sizeof(u16), EM_TIME},                // 11
        {&config.Dev_Address, &modebus_Max, &modebus_Min, sizeof(u8), EM_TIME},           // 12
        {&config.Pump_AB_Mode, &PUMP_MODE_Max, &Par_Min, sizeof(u8), EM_MODE},            // 13
        {&config.Sensor_Mode, &SENSOR_MODE_Max, &Par_Min, sizeof(u8), EM_MODE},           // 14
        {&config.Sensor_Rang, &Par_Max, &Par_Min, sizeof(u16), EM_SENSOR},                // 15
        {&config.Sensor_STOP, &config.Sensor_Rang, &Par_Min, sizeof(u16), EM_SENSOR},     // 16
        {&config.Sensor_ON1, &config.Sensor_Rang, &Par_Min, sizeof(u16), EM_SENSOR},      // 17
        {&config.Sensor_ON2, &config.Sensor_Rang, &Par_Min, sizeof(u16), EM_SENSOR},      // 18
        {&config.Auto_Mode, &mode_Max, &Par_Min, sizeof(u8), EM_MODE},                    // 19
        {&config.fault_mask, &fault_mask_max, &Par_Min, sizeof(u8), EM_MODE},             // 20
        {&config.Sensor_overFlow, &config.Sensor_Rang, &Par_Min, sizeof(u16), EM_SENSOR}, // 21
        {&config.Total_Run_Hour, &Par_Max, &Par_Min, sizeof(u16), EM_READONLY_TIME},      //20260430-3 change P-21 total run time is read-only
        {&config.Total_Power_Day, &Par_Max, &Par_Min, sizeof(u16), EM_READONLY_TIME},     //20260430-3 change P-22 total power-on time is read-only
};

Par_t parEidt;
u16 editingData = 0;
u16 editingMax = 0;
u16 editingMin = 0;
ParType_t editingType;
#if BF006_CONFIG_STORE_ENABLED
static u32 editing_revision; //20261114-4 add 进入编辑时保存配置版本
#endif

// 交换函数实现
void swap(u16 *xp, u16 *yp)
{
    u16 temp = *xp;
    *xp = *yp;
    *yp = temp;
}

// 冒泡排序函数实现
void bubbleSort(u16 *arr, u8 n)
{
    int i, j;
    for (i = 0; i < n - 1; i++)
    { // 外层循环控制排序趟数
        for (j = 0; j < n - i - 1; j++)
        { // 内层循环控制每一趟排序多少次
            if (arr[j] > arr[j + 1])
            { // 比较相邻元素，如果顺序错误则交换
                swap(&arr[j], &arr[j + 1]);
            }
        }
    }
}

u16 idata levle[3];

#if !BF006_CONFIG_STORE_ENABLED
void senSerModeCharge(void)
{
    

    levle[0] = config.Sensor_STOP;
    levle[1] = config.Sensor_ON1;
    levle[2] = config.Sensor_ON2;

    bubbleSort(levle, 3);

    if (config.Sensor_Mode == CTROLTYPE_PUMP)
    {
        config.Sensor_STOP = levle[2];
        config.Sensor_ON1 = levle[1];
        config.Sensor_ON2 = levle[0];
    }
    else
    {
        config.Sensor_STOP = levle[0];
        config.Sensor_ON1 = levle[1];
        config.Sensor_ON2 = levle[2];
    }
    updatSysdata();
}

#endif

void interEidt(void)
{

    switch (par[sys.Setting_Index].Size)
    {
    case 1:
        editingData = *((u8 *)par[sys.Setting_Index].pSrc);
        editingMax = *((u8 *)par[sys.Setting_Index].pMax);
        editingMin = *((u8 *)par[sys.Setting_Index].pMin);
        break;
    case 2:
        editingData = *((u16 *)par[sys.Setting_Index].pSrc);
        editingMax = *((u16 *)par[sys.Setting_Index].pMax);
        editingMin = *((u16 *)par[sys.Setting_Index].pMin);
        break;
    }

    editingType = par[sys.Setting_Index].type;
#if BF006_CONFIG_STORE_ENABLED
    editing_revision = config_revision;
#endif

    #if (Model_OUT == Model_1C2)

    if (config.Sensor_Mode == CTROLTYPE_PUMP)
    {
        if (sys.Setting_Index == 15)
        {
            editingMax = config.Sensor_Rang;
            editingMin = config.Sensor_ON1;
        }
        else if (sys.Setting_Index == 16)
        {
            
            editingMax = config.Sensor_STOP;
            editingMin = config.Sensor_ON2;
        }
        else if (sys.Setting_Index == 17)
        {
            editingMax = config.Sensor_ON1;
            editingMin = Par_Min;
        }
    }
    else
    {
        if (sys.Setting_Index == 15)
        {
            editingMax = config.Sensor_ON1;
            editingMin = Par_Min;
        }
        else if (sys.Setting_Index == 16)
        {
            editingMax = config.Sensor_ON2;
            editingMin = config.Sensor_STOP;
        }
        else if (sys.Setting_Index == 17)
        {
            editingMax = config.Sensor_Rang;
            editingMin = config.Sensor_ON1;
        }
    }

    #else
    if (config.Sensor_Mode == CTROLTYPE_PUMP)
    {
        if (sys.Setting_Index == 15)
        {
            editingMax = config.Sensor_Rang;
            editingMin = config.Sensor_ON1;
        }
        else if (sys.Setting_Index == 16)
        {
            
            editingMax = config.Sensor_STOP;
            editingMin = Par_Min;
        }
        else if (sys.Setting_Index == 17)
        {
            editingMax = config.Sensor_ON1;
            editingMin = Par_Min;
        }
    }
    else
    {
        if (sys.Setting_Index == 15)
        {
            editingMax = config.Sensor_ON1;
            editingMin = Par_Min;
        }
        else if (sys.Setting_Index == 16)
        {
            editingMax = config.Sensor_Rang;
            editingMin = config.Sensor_STOP;
        }
        else if (sys.Setting_Index == 17)
        {
            editingMax = config.Sensor_Rang;
            editingMin = config.Sensor_ON1;
        }
    }

    #endif


}

#if BF006_CONFIG_STORE_ENABLED

/******************************************************************************
 * 函数名称       : Panel_Submit_Edit
 * 功能描述       : 将面板参数及关联字节交给统一保存。
 * 输入参数       : None
 * 返回值         : 已受理或同值时返回一
 * 日期            : 2026.11.14
 ******************************************************************************/
static u8 Panel_Submit_Edit(void) //20261114-4 add
{
    u8 offset;
    u8 width;
    u8 turn_offset;

    Config_Observe_Changes();
    if (editing_revision != config_revision || !Config_Panel_Available()) {
        return 0;
    }
    width = par[sys.Setting_Index].Size;
    offset = (u8)((u8 *)par[sys.Setting_Index].pSrc - (u8 *)&config);
    if (sys.Setting_Index == 13) {
        if (config.Sensor_Mode == (u8)editingData) {
            return 1;
        }
        levle[0] = config.Sensor_STOP;
        levle[1] = config.Sensor_ON1;
        levle[2] = config.Sensor_ON2;
        bubbleSort(levle, 3);
        Config_Panel_Encode(0, CONFIG_OFFSET(Sensor_STOP),
                            levle[editingData == CTROLTYPE_PUMP ? 2 : 0], 2);
        Config_Panel_Encode(2, CONFIG_OFFSET(Sensor_ON1), levle[1], 2);
        Config_Panel_Encode(4, CONFIG_OFFSET(Sensor_ON2),
                            levle[editingData == CTROLTYPE_PUMP ? 0 : 2], 2);
        Config_Panel_Encode(6, offset, editingData, 1);
        return Config_Panel_Submit(7);
    }
    Config_Panel_Encode(0, offset, editingData, width);
    if (sys.Setting_Index >= 2 && sys.Setting_Index <= 5 &&
        *(u16 *)par[sys.Setting_Index].pSrc != editingData) {
        turn_offset = sys.Setting_Index <= 3 ? CONFIG_OFFSET(A_Turn) : CONFIG_OFFSET(B_Turn);
        Config_Panel_Encode(width, turn_offset, 1, 1);
        width++;
    }
    return Config_Panel_Submit(width);
}
#endif

void EidtEnter(void)
{

    if (editingType == EM_READONLY_TIME) //20260430-3 add read-only parameter exits view only and is not written back
    {
        return;
    }

#if BF006_CONFIG_STORE_ENABLED
    if (!Panel_Submit_Edit()) {
        Speak_Con(0); //20261114-4 add 拒绝过时值及占用期间提交
    }
#else
    switch (par[sys.Setting_Index].Size)
    {
    case 1:
        *((u8 *)par[sys.Setting_Index].pSrc) = editingData;
        break;
    case 2:
        *((u16 *)par[sys.Setting_Index].pSrc) = editingData;
        break;
    }

    if ((sys.Setting_Index == 2) || (sys.Setting_Index == 3))
    {
        config.A_Turn = 1;
    }
    else if ((sys.Setting_Index == 4) || (sys.Setting_Index == 5))
    {
        config.B_Turn = 1;
    }

    sys.save_flag = 1;

    if (sys.Setting_Index == 13)
    {
        senSerModeCharge();
    }

    updatSysdata();
#endif
}

void reset_dev(u8 ty)
{
    Speak_Con(2);
    config.B_Turn = 0;
    config.A_Turn = 0;
    config.Pump_AB_Mode = 0;

    config.Set_STANDBY_Current1 = 0;

    config.Set_STANDBY_Current2 = 0;

    config.Dev_type = ty;

    switch (config.Dev_type)
    {

    case 1:
        Limit_Current = V2_Limit_Current;
        config.Set_OVERLOAD_Current1 = V2_Set_Current;
        config.Set_OVERLOAD_Current2 = V2_Set_Current;
        break;
    case 2:
        Limit_Current = V3_Limit_Current;
        config.Set_OVERLOAD_Current1 = V3_Set_Current;
        config.Set_OVERLOAD_Current2 = V3_Set_Current;
        break;
    case 3:
        Limit_Current = V4_Limit_Current;
        config.Set_OVERLOAD_Current1 = V4_Set_Current;
        config.Set_OVERLOAD_Current2 = V4_Set_Current;
        break;

    case 4:
        Limit_Current = V5_Limit_Current;
        config.Set_OVERLOAD_Current1 = V5_Set_Current;
        config.Set_OVERLOAD_Current2 = V5_Set_Current;
        break;
    default:
        Limit_Current = V1_Limit_Current;
        config.Set_OVERLOAD_Current1 = V1_Set_Current;
        config.Set_OVERLOAD_Current2 = V1_Set_Current;
        config.Dev_type = 0;
        break;
    }

    sys.save_flag = 1;
}

/*
1 1
2
3
4
5
6
7
8
*/
// 处理6个按键程序
void KEY_Control(void)
{
#if BF006_CONFIG_STORE_ENABLED
    u8 target_mode;
    u8 submitted;
#endif
    u8 i;
    static u8 flag_mode_change = 0;

    if (Key_Code != 0)
    {
        switch (Key_Code)
        {
        case 0x01:
            if (sys.Record_Dis_Flag == 1)
            {
                sys.Record_Dis_Flag = 0;
            }
            else
            {
                if ((config.Hand_Auto_timer_Flag == RUNMODE_MANUAL) && (sys.Pump_A_ON == 0) && (sys.Pump_B_ON == 0))
                {
                    if (sys.Setting_Flag == 0)
                    {
                        sys.Setting_Flag = 1;
                        // sys.Setting_Index = 0;
                        Speak_Con(0);
                    }
                    else if (sys.Setting_Flag == 1)
                    {
                        sys.Setting_Flag = 2;
                        interEidt();
                        Speak_Con(0);
                    }
                    else if (sys.Setting_Flag == 2)
                    {
                        EidtEnter();
                        sys.Setting_Flag = 1;
                        Speak_Con(2);
                    }
                }
            }
            break;
        case 0x02:

            break;
        case 0x03:
            if (sys.Record_Dis_Flag == 1)
            {
                sys.Record_Dis_Flag = 0;
            }
            else
            {
                if (sys.Setting_Flag == 1)
                {
                    if (sys.Setting_Index < PAR_NUMB - 1)
                    {

                        sys.Setting_Index++;
#if (Model_OUT == Model_1C1)
                        if (sys.Setting_Index == 4)
                        {
                            sys.Setting_Index = 6;
                        }
                        else if (sys.Setting_Index == 5)
                        {
                            sys.Setting_Index = 6;
                        }
                        else if (sys.Setting_Index == 12)
                        {
                            sys.Setting_Index = 13;
                        }
                        else if (sys.Setting_Index == 17)
                        {
                            sys.Setting_Index = 18;
                        }
#endif
                    }
                }
                else if (sys.Setting_Flag == 2)
                {
                    if (editingType != EM_READONLY_TIME) //20260430-3 change read-only time parameter ignores the up key
                    {
                        if (editingData < editingMax)
                        {
                            editingData++;
                        }
                        else
                        {
                            editingData = editingMax;
                        }
                    }
                }
                else if (sys.Record_Dis_Flag)
                {
                    sys.Record_Dis_Flag = 0;
                }
                else if (config.Hand_Auto_timer_Flag == RUNMODE_MANUAL)
                {
                    if (sys.Fault_A_Flag == 0)
                    {
                        sys.Pump_A_ON = 1;
                    }
                }
            }

            break;

        case 0x04:
#if (Model_OUT == Model_1C2)
            if (sys.Record_Dis_Flag == 1)
            {
                sys.Record_Dis_Flag = 0;
            }
            else
            {
                if (sys.Setting_Flag == 0)
                {
                    if (config.Hand_Auto_timer_Flag == RUNMODE_MANUAL)
                    {
                        if (sys.Fault_B_Flag == 0)
                        {
                            sys.Pump_B_ON = 1;
                        }
                    }
                }
            }
#endif
            break;

        case 0x05:
            if (sys.Record_Dis_Flag == 1)
            {
                sys.Record_Dis_Flag = 0;
            }
            else
            {
                if (sys.Setting_Flag)
                {
                    sys.Setting_Flag--;
                    Speak_Con(0);
                }
                else
                {
                    Speak_Con(0);
#if BF006_CONFIG_STORE_ENABLED
                    target_mode = RUNMODE_MANUAL;
                    if (config.Hand_Auto_timer_Flag == RUNMODE_MANUAL) {
                        target_mode = flag_mode_change ? RUNMODE_TIMER : RUNMODE_AUTO;
                    }
                    submitted = Config_Stage_Mode(target_mode);
                    if (submitted == CONFIG_STORE_BUSY || submitted == CONFIG_STORE_READY) {
                        if (config.Hand_Auto_timer_Flag != RUNMODE_MANUAL) {
                            flag_mode_change = config.Hand_Auto_timer_Flag == RUNMODE_AUTO;
                        }
                        sys.Pump_Flag = 0;
                        sys.Pump_A_ON = 0;
                        sys.Pump_B_ON = 0;
                    }
#else
                    if (config.Hand_Auto_timer_Flag == RUNMODE_AUTO)
                    {
                        config.Hand_Auto_timer_Flag = RUNMODE_MANUAL;
                        flag_mode_change = 1;
                    }
                    else if (config.Hand_Auto_timer_Flag == RUNMODE_TIMER)
                    {
                        config.Hand_Auto_timer_Flag = RUNMODE_MANUAL;
                        flag_mode_change = 0;
                    }
                    else
                    {

                        if (flag_mode_change)
                        {
                            config.Hand_Auto_timer_Flag = RUNMODE_TIMER;
                        }
                        else
                        {

                            config.Hand_Auto_timer_Flag = RUNMODE_AUTO;
                        }
                    }
                    sys.Pump_Flag = 0;
                    sys.Pump_A_ON = 0;
                    sys.Pump_B_ON = 0;
                    sys.save_flag = 1;
                    if (config.Hand_Auto_timer_Flag == RUNMODE_TIMER)
                    {
                        sys.autoTimerFlag = 1;
                        sys.Auto_timer_count = 0;
                    }
#endif
                }
            }

            break;
        case 0x06:
            Speak_Con(0);
            if (sys.Setting_Flag == 0)
            {
                sys.Record_Timer = 0;
                if (sys.Record_Dis_Flag == 0)
                {
                    sys.Record_Dis_Flag = 1;
                    sys.Record_Cnt = 0;
                }
                else
                {
                    sys.Record_Cnt++;
                    if (sys.Record_Cnt >= 10)
                    {
                        sys.Record_Cnt = 0; // 一次循环

                        sys.Record_Dis_Flag = 0;
                    }
                }
            }

            break;
        case 0x07:

            if (sys.Record_Dis_Flag == 1)
            {
                sys.Record_Dis_Flag = 0;
            }
            else
            {

                if (sys.Setting_Flag == 1)
                {
                    if (sys.Setting_Index > 0)
                    {

                        sys.Setting_Index--;
                    }
#if (Model_OUT == Model_1C1)
                    if (sys.Setting_Index == 4)
                    {
                        sys.Setting_Index = 3;
                    }
                    else if (sys.Setting_Index == 5)
                    {
                        sys.Setting_Index = 3;
                    }
                    else if (sys.Setting_Index == 12)
                    {
                        sys.Setting_Index = 11;
                    }
                    else if (sys.Setting_Index == 17)
                    {
                        sys.Setting_Index = 16;
                    }

#endif
                }
                else if (sys.Setting_Flag == 2)
                {
                    if (editingType != EM_READONLY_TIME) //20260430-3 change read-only time parameter ignores the down key
                    {
                        if (editingData > editingMin)
                        {
                            editingData--;
                        }
                        else
                        {
                            editingData = editingMin;
                        }
                    }
                }
                else if (sys.Record_Dis_Flag)
                {
                    sys.Record_Dis_Flag = 0;
                }
                else if (config.Hand_Auto_timer_Flag == RUNMODE_MANUAL)
                {

                    sys.Pump_A_ON = 0;
                }
                if (sys.Fault_A_Flag)
                {
                    clear_A_Fault();
                }

                break;

            case 0x08:

                if (sys.Record_Dis_Flag == 1)
                {
                    sys.Record_Dis_Flag = 0;
                }
                else
                {
                    if (config.Hand_Auto_timer_Flag == RUNMODE_MANUAL)
                    {

                        sys.Pump_B_ON = 0;
                    }
                    if (sys.Fault_B_Flag)
                    {
                        clear_B_Fault();
                    }
                }
            }

            break;

        case 0x81:
            if (sys.Setting_Flag == 0)
            {
#if (Function_RF == 1)
                sys.RF_Key_Flag = 0;
                for (i = 0; i < 5; i++)
                {
                    config.RF_Family[i] = 0;
                }
                sys.save_flag = 1;
                Speak_Con(2);
#endif
            }
            else
            {
                reset_dev(0);
            }

            break;
        case 0x82:
            if ((config.Lock1) || (config.Lock2))
            {
                config.Lock1 = 0;
                config.Lock2 = 0;
                sys.Fault_Error &= ~((u32)1 << ERR_LOCK_A);
                sys.Fault_Error &= ~((u32)1 << ERR_LOCK_B);
                sys.save_flag = 1;
                clear_A_Fault();
                clear_B_Fault();
            }
            else if (sys.Setting_Flag == 0)
            {
                if (config.Hand_Auto_timer_Flag == RUNMODE_MANUAL)
                {
                    zhengdingA();
                    zhengdingB();
                }
            }
            else
            {

                reset_dev(1);
            }

            break;
        case 0x83:
            if (sys.Setting_Flag == 0)
            {
#if (Function_RF == 1)
                sys.RF_Key_Flag = 1; // 进入RF学习
                Speak_Con(1);
#endif
            }
            else
            {

                reset_dev(2);
                Speak_Con(0);
            }

            break;

        case 0x84:

            if (sys.Setting_Flag == 0)
            {
            }
            else
            {
            }

            break;

        case 0x85:
            if (sys.Setting_Flag == 0)
            {
                if ((sys.Auto_Mode == SENSORTYPE_POLE) && (config.Hand_Auto_timer_Flag != RUNMODE_TIMER))
                {

                    config.shuiWeiDingshi = !config.shuiWeiDingshi;
                    sys.save_flag = 1;
                    sys.Auto_timer_count = 0;
                }
            }
            else
            {

                reset_dev(3);
            }
            break;
        case 0x86:
            if (sys.Setting_Flag == 0)
            {
                Speak_Con(1);
                for (i = 0; i < 10; i++)
                {
                    config.Record_Type[i] = ERR_NUMB;
                    config.Record_Dat1[i] = 0;
                    config.Record_Dat2[i] = 0;
                }
                sys.save_flag = 1;
            }
            else
            {
                reset_dev(4);
            }
            break;
        case 0x87:
            if (config.Hand_Auto_timer_Flag == RUNMODE_MANUAL)
            {
                Clear_zhengdingA();
            }

            break;

        case 0x88:

            if (config.Hand_Auto_timer_Flag == RUNMODE_MANUAL)
            {
                Clear_zhengdingB();
            }

            break;
        }
        Key_Code = 0;
    }
}

void Dis_parEidt(void)
{
    LCD_HOUR = 0;
    LCD_MPA = 0;
    LCD_MIMITER = 0;
    LCD_CELS = 0;
    switch (editingType)
    {
    case EM_VOl:
        NUM5.All = 0;
        NUM6.All = LED_LIST[editingData / 100 % 10];
        NUM7.All = LED_LIST[editingData / 10 % 10];
        NUM8.All = LED_LIST[editingData % 10];
        LCD_P2 = 0;
        LCD_P3 = 0;
        LCD_P4 = 0;
        LCD_COL = 0;
        break;
    case EM_CUR:
        NUM5.All = 0;
        NUM6.All = LED_LIST[editingData / 100 % 10];
        NUM7.All = LED_LIST[editingData / 10 % 10];
        NUM8.All = LED_LIST[editingData % 10];
        LCD_P2 = 0;
        LCD_P3 = 0;
        LCD_P4 = 1;
        LCD_COL = 0;
        break;
    case EM_TIME:
    case EM_READONLY_TIME: //20260430-3 add read-only time parameter uses the existing 4-digit time display
        NUM5.All = LED_LIST[editingData / 1000 % 10];
        ;
        NUM6.All = LED_LIST[editingData / 100 % 10];
        NUM7.All = LED_LIST[editingData / 10 % 10];
        NUM8.All = LED_LIST[editingData % 10];
        LCD_P2 = 0;
        LCD_P3 = 0;
        LCD_P4 = 0;
        LCD_COL = 0;
        break;
    case EM_MODE:
        NUM5.All = 0;
        NUM6.All = 0;
        NUM7.All = 0;
        NUM8.All = LED_LIST[editingData];
        LCD_P2 = 0;
        LCD_P3 = 0;
        LCD_P4 = 0;
        LCD_COL = 0;

        break;
    case EM_SENSOR:
        NUM5.All = 0;
        NUM6.All = LED_LIST[editingData / 100 % 10];
        NUM7.All = LED_LIST[editingData / 10 % 10];
        NUM8.All = LED_LIST[editingData % 10];
        LCD_P2 = 0;
        LCD_P3 = 0;
        LCD_P4 = 1;
        LCD_COL = 0;
        if (sys.Auto_Mode >= SENSORTYPE_PRESS4_20MA)
        {
            LCD_MPA = 1;
        }
        else
        {

            LCD_MIMITER = 1;
        }
        break;
    }
}

void Err_Dis(u8 numb)
{
    static u8 tick = 0;
    tick++;
    switch (numb)
    {
    case ERR_UVP:
        LCD_VOLT_L = 1;
        LCD_VOLT_H = 0;
        LCD_ERROR_PHASE = 0;
        LCD_LOSS_A_PHASE = 0;
        LCD_LOSS_B_PHASE = 0;
        LCD_LOSS_C_PHASE = 0;
        LCD_OVERLOAD = 0;
        LCD_STANDBY = 0;
        LCD_ERROR_A_PUMP = 0;
        LCD_ERROR_B_PUMP = 0;
        break;
    case ERR_OVP:
        LCD_VOLT_L = 0;
        LCD_VOLT_H = 1;
        LCD_ERROR_PHASE = 0;
        LCD_LOSS_A_PHASE = 0;
        LCD_LOSS_B_PHASE = 0;
        LCD_LOSS_C_PHASE = 0;
        LCD_OVERLOAD = 0;
        LCD_STANDBY = 0;
        LCD_ERROR_A_PUMP = 0;
        LCD_ERROR_B_PUMP = 0;
        break;
    case ERR_OVERLOAD1:
        LCD_VOLT_L = 0;
        LCD_VOLT_H = 0;
        LCD_ERROR_PHASE = 0;
        LCD_LOSS_A_PHASE = 0;
        LCD_LOSS_B_PHASE = 0;
        LCD_LOSS_C_PHASE = 0;
        LCD_OVERLOAD = 1;
        LCD_STANDBY = 0;
        LCD_ERROR_A_PUMP = 1;
        LCD_ERROR_B_PUMP = 0;
        break;
    case ERR_STANDBY1:
        LCD_VOLT_L = 0;
        LCD_VOLT_H = 0;
        LCD_ERROR_PHASE = 0;
        LCD_LOSS_A_PHASE = 0;
        LCD_LOSS_B_PHASE = 0;
        LCD_LOSS_C_PHASE = 0;
        LCD_OVERLOAD = 0;
        LCD_STANDBY = 1;
        LCD_ERROR_A_PUMP = 1;
        LCD_ERROR_B_PUMP = 0;
        break;

    case ERR_LOSE_A1:
        LCD_VOLT_L = 0;
        LCD_VOLT_H = 0;
        LCD_ERROR_PHASE = 0;
        LCD_LOSS_A_PHASE = 1;
        LCD_LOSS_B_PHASE = 0;
        LCD_LOSS_C_PHASE = 0;
        LCD_OVERLOAD = 0;
        LCD_STANDBY = 0;
        LCD_ERROR_A_PUMP = 1;
        LCD_ERROR_B_PUMP = 0;
        break;

    case ERR_LOSE_B1:
        LCD_VOLT_L = 0;
        LCD_VOLT_H = 0;
        LCD_ERROR_PHASE = 0;
        LCD_LOSS_A_PHASE = 0;
        LCD_LOSS_B_PHASE = 1;
        LCD_LOSS_C_PHASE = 0;
        LCD_OVERLOAD = 0;
        LCD_STANDBY = 0;
        LCD_ERROR_A_PUMP = 1;
        LCD_ERROR_B_PUMP = 0;
        break;
    case ERR_LOSE_C1:
        LCD_VOLT_L = 0;
        LCD_VOLT_H = 0;
        LCD_ERROR_PHASE = 0;
        LCD_LOSS_A_PHASE = 0;
        LCD_LOSS_B_PHASE = 0;
        LCD_LOSS_C_PHASE = 1;
        LCD_OVERLOAD = 0;
        LCD_STANDBY = 0;
        LCD_ERROR_A_PUMP = 1;
        LCD_ERROR_B_PUMP = 0;
        break;
    case ERR_OVERLOAD2:
        LCD_VOLT_L = 0;
        LCD_VOLT_H = 0;
        LCD_ERROR_PHASE = 0;
        LCD_LOSS_A_PHASE = 0;
        LCD_LOSS_B_PHASE = 0;
        LCD_LOSS_C_PHASE = 0;
        LCD_OVERLOAD = 1;
        LCD_STANDBY = 0;
        LCD_ERROR_A_PUMP = 0;
        LCD_ERROR_B_PUMP = 1;
        break;
    case ERR_STANDBY2:
        LCD_VOLT_L = 0;
        LCD_VOLT_H = 0;
        LCD_ERROR_PHASE = 0;
        LCD_LOSS_A_PHASE = 0;
        LCD_LOSS_B_PHASE = 0;
        LCD_LOSS_C_PHASE = 0;
        LCD_OVERLOAD = 0;
        LCD_STANDBY = 1;
        LCD_ERROR_A_PUMP = 0;
        LCD_ERROR_B_PUMP = 1;
        break;

    case ERR_LOSE_A2:
        LCD_VOLT_L = 0;
        LCD_VOLT_H = 0;
        LCD_ERROR_PHASE = 0;
        LCD_LOSS_A_PHASE = 1;
        LCD_LOSS_B_PHASE = 0;
        LCD_LOSS_C_PHASE = 0;
        LCD_OVERLOAD = 0;
        LCD_STANDBY = 0;
        LCD_ERROR_A_PUMP = 0;
        LCD_ERROR_B_PUMP = 1;
        break;

    case ERR_LOSE_B2:
        LCD_VOLT_L = 0;
        LCD_VOLT_H = 0;
        LCD_ERROR_PHASE = 0;
        LCD_LOSS_A_PHASE = 0;
        LCD_LOSS_B_PHASE = 1;
        LCD_LOSS_C_PHASE = 0;
        LCD_OVERLOAD = 0;
        LCD_STANDBY = 0;
        LCD_ERROR_A_PUMP = 0;
        LCD_ERROR_B_PUMP = 1;
        break;
    case ERR_LOSE_C2:
        LCD_VOLT_L = 0;
        LCD_VOLT_H = 0;
        LCD_ERROR_PHASE = 0;
        LCD_LOSS_A_PHASE = 0;
        LCD_LOSS_B_PHASE = 0;
        LCD_LOSS_C_PHASE = 1;
        LCD_OVERLOAD = 0;
        LCD_STANDBY = 0;
        LCD_ERROR_A_PUMP = 0;
        LCD_ERROR_B_PUMP = 1;
        break;
    case ERR_NOLOAD_A:
        LCD_VOLT_L = 0;
        LCD_VOLT_H = 0;
        LCD_ERROR_PHASE = 0;
        LCD_LOSS_A_PHASE = 0;
        LCD_LOSS_B_PHASE = 0;
        LCD_LOSS_C_PHASE = 0;
        LCD_OVERLOAD = 0;
        LCD_STANDBY = 0;
        LCD_ERROR_A_PUMP = 0;
        LCD_ERROR_B_PUMP = 0;
        if (tick & 0x04)
        {
            LCD_PA_X52 = 1;
            LCD_PA_X53 = 1;
        }
        else
        {
            LCD_PA_X52 = 0;
            LCD_PA_X53 = 0;
        }
        break;

    case ERR_NOLOAD_B:
        LCD_VOLT_L = 0;
        LCD_VOLT_H = 0;
        LCD_ERROR_PHASE = 0;
        LCD_LOSS_A_PHASE = 0;
        LCD_LOSS_B_PHASE = 0;
        LCD_LOSS_C_PHASE = 0;
        LCD_OVERLOAD = 0;
        LCD_STANDBY = 0;
        LCD_ERROR_A_PUMP = 0;
        LCD_ERROR_B_PUMP = 0;
        if (tick & 0x04)
        {
            LCD_PB_X50 = 1;
            LCD_PB_X51 = 1;
        }
        else
        {
            LCD_PB_X50 = 0;
            LCD_PB_X51 = 0;
        }
        break;
    case ERR_LOCK_A:
        LCD_VOLT_L = 1;
        LCD_VOLT_H = 1;
        LCD_ERROR_PHASE = 1;
        LCD_LOSS_A_PHASE = 1;
        LCD_LOSS_B_PHASE = 1;
        LCD_LOSS_C_PHASE = 1;
        LCD_OVERLOAD = 1;
        LCD_STANDBY = 1;
        LCD_ERROR_A_PUMP = 1;
        break;
    case ERR_LOCK_B:
        LCD_VOLT_L = 1;
        LCD_VOLT_H = 1;
        LCD_ERROR_PHASE = 1;
        LCD_LOSS_A_PHASE = 1;
        LCD_LOSS_B_PHASE = 1;
        LCD_LOSS_C_PHASE = 1;
        LCD_OVERLOAD = 1;
        LCD_STANDBY = 1;
        LCD_ERROR_B_PUMP = 1;
        break;
    default:
        LCD_VOLT_L = 0;
        LCD_VOLT_H = 0;
        LCD_ERROR_PHASE = 0;
        LCD_LOSS_A_PHASE = 0;
        LCD_LOSS_B_PHASE = 0;
        LCD_LOSS_C_PHASE = 0;
        LCD_OVERLOAD = 0;
        LCD_STANDBY = 0;
        LCD_ERROR_A_PUMP = 0;
        LCD_ERROR_B_PUMP = 0;
        break;
    }
}

void Err_Check_Dis(void)
{
    u8 i = 0;
    if (sys.Fault_Error)
    {
        for (i = 0; i < ERR_NUMB; i++)
        {
            if (sys.Fault_Error & ((u32)1 << i))
            {
                break;
            }
        }

        if (i >= ERR_NUMB)
        {
            sys.Fault_Error = 0;
        }

        Err_Dis(i);
        //20261113-0 del 停用旧协议的故障上报码：sys.up4G_err = i;
    }
    else
    {

        LCD_VOLT_L = 0;
        LCD_VOLT_H = 0;
        LCD_ERROR_PHASE = 0;
        LCD_LOSS_A_PHASE = 0;
        LCD_LOSS_B_PHASE = 0;
        LCD_LOSS_C_PHASE = 0;
        LCD_OVERLOAD = 0;
        LCD_STANDBY = 0;
        LCD_ERROR_A_PUMP = 0;
        LCD_ERROR_B_PUMP = 0;

#if 0 //20261113-0 del 封存仅供旧协议使用的传感器和溢流上报码。
        if (sys.sensor_err)
        {
            sys.up4G_err = ERR_NUMB + 1;
        }
        else if (sys.flowover)
        {
            sys.up4G_err = ERR_NUMB + 2;
        }
        else
        {

            sys.up4G_err = ERR_NUMB;
        }
#endif
    }
}

#if (Model_OUT == Model_1C1)
/******************************************************************************
 * 函数名称       : Display_Continuous_Sensor
 * 功能描述       : 用四位数字显示连续量，超出显示范围时保留单位并显示横线。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
static void Display_Continuous_Sensor(void) //20261114-1 add
{
    u16 display_value;

    LCD_MIMITER = (sys.Auto_Mode == SENSORTYPE_WATER4_20MA);
    LCD_MPA = !LCD_MIMITER;
    if (sys.SensorOut > 9999UL) {
        NUM5.All = 0x40;
        NUM6.All = 0x40;
        NUM7.All = 0x40;
        NUM8.All = 0x40;
        LCD_P3 = 0;
        return;
    }

    display_value = (u16)sys.SensorOut; // 已检查显示范围，保留原两位小数
    NUM5.All = LED_LIST[display_value / 1000];
    NUM6.All = LED_LIST[display_value / 100 % 10];
    NUM7.All = LED_LIST[display_value / 10 % 10];
    NUM8.All = LED_LIST[display_value % 10];
    LCD_P3 = 1;
}
#endif

void DIS_Control(void)
{
    u16 RemainTime;
    u16 DisCurrent;
    static u8 flag_Phash = 0;
    static u8 tick = 0;

    if (++tick > 30)
    {

        tick = 0;
        if (++flag_Phash > 2)
        {
            flag_Phash = 0;
        }
    }

    Dis2_Flash = 0;
    Dis6_Flash = 0;

#if (Model_OUT == Model_1C2)

    LCD_V_X1 = 1;
    LCD_V_X2 = 1;
    LCD_A_X22 = 1;
    LCD_A_X27 = 1;
    LCD_A_X33 = 1;

    LCD_PA_X23 = 1;
    LCD_PB_X29 = 1;
    Dis5_Flash = 2;

    if (config.A_Turn == 0)
    {
        LCD_A_UNTURN = 1;
    }
    else
    {
        LCD_A_UNTURN = 0;
    }

    if (config.B_Turn == 0)
    {
        LCD_B_UNTURN = 1;
    }
    else
    {
        LCD_B_UNTURN = 0;
    }

#elif (Model_OUT == Model_1C1)
    Dis5_Flash = 0;

    LCD_V_X1 = 1;
    LCD_V_X2 = 1;
    LCD_A_X22 = 1;
    LCD_A_X27 = 1;
    LCD_A_X33 = 0;

    LCD_PA_X23 = 1;
    LCD_PB_X29 = 0;
    if (config.A_Turn == 0)
    {
        LCD_A_UNTURN = 1;
    }
    else
    {
        LCD_A_UNTURN = 0;
    }
    LCD_B_UNTURN = 0;
    LCD_P6 = 0;
    LCD_PB_X50 = 0;
    LCD_PB_X51 = 0;
#endif

    if (sys.Pump_A_state)
    {
        LCD_PA_X52 = 1;
        LCD_PA_X53 = 1;
    }
    else
    {
        LCD_PA_X52 = 0;
        LCD_PA_X53 = 0;
    }

#if (Model_OUT == Model_1C2)
    if (sys.Pump_B_state)
    {
        LCD_PB_X50 = 1;
        LCD_PB_X51 = 1;
    }
    else
    {
        LCD_PB_X50 = 0;
        LCD_PB_X51 = 0;
    }
#endif

    LCD_SINGLE_RUN = 0;
    LCD_ALL_RUN = 0;
    LCD_CALIBRATE = 0;
    LCD_RANGE = 0;
    LCD_TIMER_ON = 0;
    LCD_TIMER_OFF = 0;

    if ((sys.Auto_Mode == 0) && (config.shuiWeiDingshi))
    {

        LCD_TIMER_ON = 0;
    }

    if (sys.Record_Dis_Flag == 0)
    {
        NUM1.All = LED_LIST[sys.Voltage_Line / 100 % 10];
        NUM2.All = LED_LIST[sys.Voltage_Line / 10 % 10];
        NUM3.All = LED_LIST[sys.Voltage_Line / 1 % 10];

        LCD_P1 = 0;

        LCD_AA_X24 = 0;
        LCD_AB_X25 = 0;
        LCD_AC_X26 = 0;

        LCD_BA_X30 = 0;
        LCD_BB_X31 = 0;
        LCD_BC_X32 = 0;

        if ((sys.Pump_A_state == 0) && (sys.Pump_B_state == 0))
        {
            flag_Phash = 0;
            tick = 0;
        }
#if (Model_Voltage == Model_3PH)
        if (sys.Pump_A_state)
        {

            if (flag_Phash == 1)
            {
                LCD_AB_X25 = 1;
                DisCurrent = sys.Current_B1;
            }
            else if (flag_Phash == 2)
            {
                LCD_AC_X26 = 1;
                DisCurrent = sys.Current_C1;
            }
            else
            {
                LCD_AA_X24 = 1;
                DisCurrent = sys.Current_A1;
            }
        }
        else
#endif
        {

            LCD_AA_X24 = 1;
            DisCurrent = sys.Current_A1;
        }

        NUM9.All = LED_LIST[DisCurrent / 100 % 10];
        NUM10.All = LED_LIST[DisCurrent / 10 % 10];
        NUM11.All = LED_LIST[DisCurrent / 1 % 10];
        LCD_P5 = 1;

#if (Model_OUT == Model_1C2)
#if (Model_Voltage == Model_3PH)
        if (sys.Pump_B_state)
        {
            if (flag_Phash == 1)
            {
                DisCurrent = sys.Current_B2;
                LCD_BB_X31 = 1;
            }
            else if (flag_Phash == 2)
            {
                DisCurrent = sys.Current_C2;
                LCD_BC_X32 = 1;
            }
            else
            {
                LCD_BA_X30 = 1;
                DisCurrent = sys.Current_A2;
            }
        }
        else
#endif
        {

            LCD_BA_X30 = 1;
            DisCurrent = sys.Current_A2;
        }

        NUM12.All = LED_LIST[DisCurrent / 100 % 10];
        NUM13.All = LED_LIST[DisCurrent / 10 % 10];
        NUM14.All = LED_LIST[DisCurrent / 1 % 10];
        LCD_P6 = 1;

#endif

        Err_Check_Dis();
        Dis6_Flash = 0;

        if (sys.Setting_Flag == 1)
        {
            NUM5.All = 0x73;
            NUM6.All = 0x40;
            NUM7.All = LED_LIST[sys.Setting_Index / 10 % 10];
            NUM8.All = LED_LIST[sys.Setting_Index % 10];
            LCD_P2 = 0;
            LCD_P3 = 0;
            LCD_P4 = 0;
            LCD_COL = 0;
            LCD_RUN = 0;
            LCD_STOP = 0;
            LCD_HOUR = 0;
            LCD_MPA = 0;
            LCD_MIMITER = 0;
            LCD_CELS = 0;
        }
        else if (sys.Setting_Flag == 2)
        {
            Dis_parEidt();
        }
        else
        {

            LCD_P2 = 0;
            LCD_P3 = 0;
            LCD_P4 = 0;
            LCD_COL = 0;
            LCD_RUN = 0;
            LCD_STOP = 0;
            LCD_HOUR = 0;
            LCD_MPA = 0;
            LCD_MIMITER = 0;
            LCD_CELS = 0;
            if (config.Hand_Auto_timer_Flag == RUNMODE_TIMER)
            {
                if (sys.autoTimerFlag)
                {
                    RemainTime = sys.Auto_timer_count / 60;
                    if (RemainTime < config.AUTO_Timing_Run)
                    {
                        RemainTime = config.AUTO_Timing_Run - RemainTime;
                    }
                    else
                    {

                        RemainTime = 0;
                    }
                    LCD_RUN = 1;
                }
                else
                {

                    RemainTime = sys.Auto_timer_count / 60;
                    if (RemainTime < config.AUTO_Timing_Stop)
                    {
                        RemainTime = config.AUTO_Timing_Stop - RemainTime;
                    }
                    else
                    {

                        RemainTime = 0;
                    }
                    LCD_STOP = 1;
                }

                NUM5.All = LED_LIST[RemainTime / 1000 % 10];
                NUM6.All = LED_LIST[RemainTime / 100 % 10];
                NUM7.All = LED_LIST[RemainTime / 10 % 10];
                NUM8.All = LED_LIST[RemainTime % 10];
            }
            else if (sys.Auto_Mode == SENSORTYPE_PRESS) // 压力
            {
                NUM5.All = 0;
                NUM6.All = 0;
                NUM7.All = 0;
                NUM8.All = 0;
            }
#if (Model_OUT == Model_1C1)
            else if (sys.Auto_Mode >= SENSORTYPE_WATER4_20MA &&
                     sys.Auto_Mode <= SENSORTYPE_PRESS0_5V) { //20261114-1 change
                Display_Continuous_Sensor();
            }
#else
            else if (sys.Auto_Mode == SENSORTYPE_WATER4_20MA) // 液位
            {
                NUM5.All = LED_LIST[sys.SensorOut / 1000 % 10];
                ;
                NUM6.All = LED_LIST[sys.SensorOut / 100 % 10];
                NUM7.All = LED_LIST[sys.SensorOut / 10 % 10];
                NUM8.All = LED_LIST[sys.SensorOut % 10];
                LCD_MIMITER = 1;
                LCD_P3 = 1;
            }
            else if (sys.Auto_Mode == SENSORTYPE_PRESS4_20MA) // 压力传感器
            {
                NUM5.All = LED_LIST[sys.SensorOut / 1000 % 10];
                ;
                NUM6.All = LED_LIST[sys.SensorOut / 100 % 10];
                NUM7.All = LED_LIST[sys.SensorOut / 10 % 10];
                NUM8.All = LED_LIST[sys.SensorOut % 10];
                LCD_MPA = 1;
                LCD_P3 = 1;
            }
            else if (sys.Auto_Mode == SENSORTYPE_PRESS0_5V) // 远传压力
            {
                NUM5.All = LED_LIST[sys.SensorOut / 1000 % 10];
                NUM6.All = LED_LIST[sys.SensorOut / 100 % 10];
                NUM7.All = LED_LIST[sys.SensorOut / 10 % 10];
                NUM8.All = LED_LIST[sys.SensorOut % 10];
                LCD_MPA = 1;
                LCD_P3 = 1;
            }
#endif
            else // 水位
            {
                //			if(config.shuiWeiDingshi)
                //			{
                //
                //
                //			}else{

                NUM5.All = 0;
                NUM6.All = 0;
                NUM7.All = 0;
                NUM8.All = 0;
                //			}
            }
        }
    }
    else
    {

        LCD_AA_X24 = 0;
        LCD_AB_X25 = 0;
        LCD_AC_X26 = 0;

        LCD_BA_X30 = 0;
        LCD_BB_X31 = 0;
        LCD_BC_X32 = 0;

        LCD_PA_X23 = 0;
        LCD_PB_X29 = 0;
        LCD_A_UNTURN = 0;
        LCD_B_UNTURN = 0;
        LCD_A_X33 = 0;
        Dis6_Flash = 2;
        NUM15.All = LED_LIST[sys.Record_Cnt];

        NUM1.All = LED_LIST[config.Record_Dat1[sys.Record_Cnt] / 100 % 10];
        NUM2.All = LED_LIST[config.Record_Dat1[sys.Record_Cnt] / 10 % 10];
        NUM3.All = LED_LIST[config.Record_Dat1[sys.Record_Cnt] / 1 % 10];

        LCD_P1 = 0;

        NUM9.All = LED_LIST[config.Record_Dat2[sys.Record_Cnt] / 100 % 10];
        NUM10.All = LED_LIST[config.Record_Dat2[sys.Record_Cnt] / 10 % 10];
        NUM11.All = LED_LIST[config.Record_Dat2[sys.Record_Cnt] / 1 % 10];
        LCD_P5 = 1;

        NUM12.All = 0;
        NUM13.All = 0;
        NUM14.All = 0;
        LCD_P6 = 0;
        Err_Dis(config.Record_Type[sys.Record_Cnt]);

        NUM5.All = LED_LIST[config.run_xiaoshi / 1000 % 10];
        NUM6.All = LED_LIST[config.run_xiaoshi / 100 % 10];
        NUM7.All = LED_LIST[config.run_xiaoshi / 10 % 10];
        NUM8.All = LED_LIST[config.run_xiaoshi % 10];
    }

    // 模式
    if (config.Hand_Auto_timer_Flag == RUNMODE_MANUAL)
    {
        LCD_HAND = 1;
        LCD_AUTO = 0;
        LCD_TIMER = 0;
    }
    else if (config.Hand_Auto_timer_Flag == RUNMODE_AUTO)
    {
        LCD_HAND = 0;
        LCD_AUTO = 1;
        LCD_TIMER = 0;
    }
    else
    {
        LCD_HAND = 0;
        LCD_AUTO = 0;
        LCD_TIMER = 1;
    }

    // 自动 传感器
    if (sys.Auto_Mode == SENSORTYPE_POLE)
    {
        LCD_SEN_WATER = 1;
        LCD_SEN_PRESS = 0;
        LCD_SEN_WATER_MA = 0;
        LCD_SEN_PRESS_MA = 0;
        LCD_SEN_PRESS_V = 0;
    }
    else if (sys.Auto_Mode == SENSORTYPE_PRESS)
    {
        LCD_SEN_WATER = 0;
        LCD_SEN_PRESS = 1;
        LCD_SEN_WATER_MA = 0;
        LCD_SEN_PRESS_MA = 0;
        LCD_SEN_PRESS_V = 0;
    }
    else if (sys.Auto_Mode == SENSORTYPE_WATER4_20MA)
    {
        LCD_SEN_WATER = 0;
        LCD_SEN_PRESS = 0;
        LCD_SEN_WATER_MA = 1;
        LCD_SEN_PRESS_MA = 0;
        LCD_SEN_PRESS_V = 0;
    }
    else if (sys.Auto_Mode == SENSORTYPE_PRESS4_20MA)
    {

        LCD_SEN_WATER = 0;
        LCD_SEN_PRESS = 0;
        LCD_SEN_WATER_MA = 0;
        LCD_SEN_PRESS_MA = 1;
        LCD_SEN_PRESS_V = 0;
    }
    else
    {

        LCD_SEN_WATER = 0;
        LCD_SEN_PRESS = 0;
        LCD_SEN_WATER_MA = 0;
        LCD_SEN_PRESS_MA = 0;
        LCD_SEN_PRESS_V = 1;
    }

    LCD_WATER_HFLAG = 1;
    LCD_WATER_LFLAG = 1;
    Dis7_Flash = 2;
    Dis8_Flash = 2;
    if (config.Sensor_Mode == CTROLTYPE_PUMP) // 传感器在上水池
    {

        // 水位
        if (sys.xia_shuiwei == 0)
        {
            LCD_WATER_Llevel1 = 1;
            LCD_WATER_Llevel2 = 1;
            LCD_WATER_Llevel3 = 0;
            LCD_WATER_Llevel4 = 0;
            LCD_WATER_Llevel5 = 0;
            LCD_WATER_Llevel6 = 0;
        }
        else if (sys.xia_shuiwei == 1)
        {
            if (sys.xia_gao_Flag == 1)
            {
                LCD_WATER_Llevel6 = 0;
                LCD_WATER_Llevel5 = 0;
                LCD_WATER_Llevel4 = 1;
                LCD_WATER_Llevel3 = 1;
            }
            else if (sys.xia_zhong_Flag == 1)
            {
                LCD_WATER_Llevel6 = 1;
                LCD_WATER_Llevel5 = 1;
                LCD_WATER_Llevel4 = 0;
                LCD_WATER_Llevel3 = 0;
            }

            LCD_WATER_Llevel1 = 1;
            LCD_WATER_Llevel2 = 1;
        }
        else if (sys.xia_shuiwei == 2)
        {
            LCD_WATER_Llevel1 = 1;
            LCD_WATER_Llevel2 = 1;
            LCD_WATER_Llevel3 = 1;
            LCD_WATER_Llevel4 = 1;
            LCD_WATER_Llevel5 = 1;
            LCD_WATER_Llevel6 = 1;
        }

        if (sys.sensor_err == 0)
        {
            if (sys.Auto_Mode >= SENSORTYPE_WATER4_20MA) // 液位
            {
                if (sys.SensorLever == 2)
                {
                    LCD_WATER_Hlevel1 = 1;
                    LCD_WATER_Hlevel2 = 1;
                    LCD_WATER_Hlevel3 = 1;
                    LCD_WATER_Hlevel4 = 1;
                    LCD_WATER_Hlevel5 = 1;
                    LCD_WATER_Hlevel6 = 1;
                }
                else if (sys.SensorLever == 1)
                {
                    LCD_WATER_Hlevel1 = 1;
                    LCD_WATER_Hlevel2 = 1;
                    LCD_WATER_Hlevel3 = 1;
                    LCD_WATER_Hlevel4 = 1;
                    LCD_WATER_Hlevel5 = 0;
                    LCD_WATER_Hlevel6 = 0;
                }
                else
                {
                    LCD_WATER_Hlevel1 = 1;
                    LCD_WATER_Hlevel2 = 1;
                    LCD_WATER_Hlevel3 = 0;
                    LCD_WATER_Hlevel4 = 0;
                    LCD_WATER_Hlevel5 = 0;
                    LCD_WATER_Hlevel6 = 0;
                }
            }
            else // 水位
            {
                // 上水池显示
                if (sys.shang_shuiwei == 0)
                {
                    LCD_WATER_Hlevel1 = 1;
                    LCD_WATER_Hlevel2 = 1;
                    LCD_WATER_Hlevel3 = 0;
                    LCD_WATER_Hlevel4 = 0;
                    LCD_WATER_Hlevel5 = 0;
                    LCD_WATER_Hlevel6 = 0;
                }
                else if (sys.shang_shuiwei == 1)
                {
                    if (sys.shang_gao_Flag == 1)
                    {
                        LCD_WATER_Hlevel6 = 0;
                        LCD_WATER_Hlevel5 = 0;
                        LCD_WATER_Hlevel4 = 1;
                        LCD_WATER_Hlevel3 = 1;
                    }
                    else if (sys.shang_zhong_Flag == 1)
                    {
                        LCD_WATER_Hlevel6 = 1;
                        LCD_WATER_Hlevel5 = 1;
                        LCD_WATER_Hlevel4 = 0;
                        LCD_WATER_Hlevel3 = 0;
                    }
                    LCD_WATER_Hlevel2 = 1;
                    LCD_WATER_Hlevel1 = 1;
                }
                else if (sys.shang_shuiwei == 2)
                {
                    LCD_WATER_Hlevel1 = 1;
                    LCD_WATER_Hlevel2 = 1;
                    LCD_WATER_Hlevel3 = 1;
                    LCD_WATER_Hlevel4 = 1;
                    LCD_WATER_Hlevel5 = 1;
                    LCD_WATER_Hlevel6 = 1;
                }
            }
        }
        else
        {
            Dis7_Flash = 1;

            LCD_WATER_Hlevel1 = 1;
            LCD_WATER_Hlevel2 = 1;
            LCD_WATER_Hlevel3 = 1;
            LCD_WATER_Hlevel4 = 1;
            LCD_WATER_Hlevel5 = 1;
            LCD_WATER_Hlevel6 = 1;
        }
    }
    else
    { // 传感器在下水池

        if (sys.Auto_Mode >= SENSORTYPE_WATER4_20MA) // 液位
        {

            if (sys.sensor_err == 0)
            {
                if (sys.SensorLever == 2)
                {
                    LCD_WATER_Llevel1 = 1;
                    LCD_WATER_Llevel2 = 1;
                    LCD_WATER_Llevel3 = 1;
                    LCD_WATER_Llevel4 = 1;
                    LCD_WATER_Llevel5 = 1;
                    LCD_WATER_Llevel6 = 1;
                }
                else if (sys.SensorLever == 1)
                {
                    LCD_WATER_Llevel1 = 1;
                    LCD_WATER_Llevel2 = 1;
                    LCD_WATER_Llevel3 = 1;
                    LCD_WATER_Llevel4 = 1;
                    LCD_WATER_Llevel5 = 0;
                    LCD_WATER_Llevel6 = 0;
                }
                else
                {
                    LCD_WATER_Llevel1 = 1;
                    LCD_WATER_Llevel2 = 1;
                    LCD_WATER_Llevel3 = 0;
                    LCD_WATER_Llevel4 = 0;
                    LCD_WATER_Llevel5 = 0;
                    LCD_WATER_Llevel6 = 0;
                }
            }
            else
            {
                Dis8_Flash = 1;

                LCD_WATER_Llevel1 = 1;
                LCD_WATER_Llevel2 = 1;
                LCD_WATER_Llevel3 = 1;
                LCD_WATER_Llevel4 = 1;
                LCD_WATER_Llevel5 = 1;
                LCD_WATER_Llevel6 = 1;
            }
        }
        else
        {

            // 水位
            if (sys.xia_shuiwei == 0)
            {
                LCD_WATER_Llevel1 = 1;
                LCD_WATER_Llevel2 = 1;
                LCD_WATER_Llevel3 = 0;
                LCD_WATER_Llevel4 = 0;
                LCD_WATER_Llevel5 = 0;
                LCD_WATER_Llevel6 = 0;
            }
            else if (sys.xia_shuiwei == 1)
            {
                if (sys.xia_gao_Flag == 1)
                {
                    LCD_WATER_Llevel6 = 0;
                    LCD_WATER_Llevel5 = 0;
                    LCD_WATER_Llevel4 = 1;
                    LCD_WATER_Llevel3 = 1;
                }
                else if (sys.xia_zhong_Flag == 1)
                {
                    LCD_WATER_Llevel6 = 1;
                    LCD_WATER_Llevel5 = 1;
                    LCD_WATER_Llevel4 = 0;
                    LCD_WATER_Llevel3 = 0;
                }

                LCD_WATER_Llevel1 = 1;
                LCD_WATER_Llevel2 = 1;
            }
            else if (sys.xia_shuiwei == 2)
            {
                LCD_WATER_Llevel1 = 1;
                LCD_WATER_Llevel2 = 1;
                LCD_WATER_Llevel3 = 1;
                LCD_WATER_Llevel4 = 1;
                LCD_WATER_Llevel5 = 1;
                LCD_WATER_Llevel6 = 1;
            }
        }

        // 上水池显示
        if (sys.shang_shuiwei == 0)
        {
            LCD_WATER_Hlevel1 = 1;
            LCD_WATER_Hlevel2 = 1;
            LCD_WATER_Hlevel3 = 0;
            LCD_WATER_Hlevel4 = 0;
            LCD_WATER_Hlevel5 = 0;
            LCD_WATER_Hlevel6 = 0;
        }
        else if (sys.shang_shuiwei == 1)
        {
            if (sys.shang_gao_Flag == 1)
            {
                LCD_WATER_Hlevel6 = 0;
                LCD_WATER_Hlevel5 = 0;
                LCD_WATER_Hlevel4 = 1;
                LCD_WATER_Hlevel3 = 1;
            }
            else if (sys.shang_zhong_Flag == 1)
            {
                LCD_WATER_Hlevel6 = 1;
                LCD_WATER_Hlevel5 = 1;
                LCD_WATER_Hlevel4 = 0;
                LCD_WATER_Hlevel3 = 0;
            }
            LCD_WATER_Hlevel2 = 1;
            LCD_WATER_Hlevel1 = 1;
        }
        else if (sys.shang_shuiwei == 2)
        {
            LCD_WATER_Hlevel1 = 1;
            LCD_WATER_Hlevel2 = 1;
            LCD_WATER_Hlevel3 = 1;
            LCD_WATER_Hlevel4 = 1;
            LCD_WATER_Hlevel5 = 1;
            LCD_WATER_Hlevel6 = 1;
        }
    }

#if (Model_OUT == Model_1C2)

    if (sys.Sensor_OverFlow) //20260729-7 change 一控二溢出位只上报 B 泵过载
    {

        LCD_OVERFLOW = 1;
        sys.flowover = 1;
        Speak_Con(0);
    }
    else
    {

        LCD_OVERFLOW = 0;
        sys.flowover = 0;
    }
#else
    if (sys.Sensor_OverFlow) //20260729-5 change 一控一加泵位只上报 A 泵过载
    {

        LCD_OVERFLOW = 1;
        sys.flowover = 1;
        Speak_Con(0);
    }
    else
    {

        LCD_OVERFLOW = 0;
        sys.flowover = 0;
    }
#endif

    if (sys.Setting_Flag > 0)
    {
        LCD_SETUP = 1;
    }
    else
    {

        LCD_SETUP = 0;
    }
}

void SW_Read(void)
{
    //    u8 state = 0;
    //    static u8 cnt = 0;
    //    static u8 state_old = 0;

    ////    state = SW3;
    ////    state <<=1;
    ////    state |= SW2;
    ////    state <<=1;
    ////    state |= SW1;
    //
    //	state = 0;//固定水位传感器

    //    if(state_old !=state)
    //    {
    //        cnt = 0;
    //        state_old =state;
    //    }

    //    if(sys.SW_State != state_old)
    //    {
    //        if(cnt < 5)
    //        {
    //            cnt = 0;
    //            sys.SW_State = state_old;

    //        } else {

    //            cnt++;
    //        }

    //    }

    //    if(sys.SW_State == 1)
    //    {
    //        sys.Auto_Mode = 1;
    //    } else if(sys.SW_State == 2)
    //    {
    //        sys.Auto_Mode = 2;
    //    } else if(sys.SW_State == 4)
    //    {
    //        sys.Auto_Mode = 3;
    //    } else if(sys.SW_State == 7)
    //    {
    //        sys.Auto_Mode = 4;
    //    } else {
    //        sys.Auto_Mode = 0;
    //    }

#if (Model_OUT == Model_1C1)
    if (sys.Auto_Mode != config.Auto_Mode) { //20261114-1 add 换型后等待下一批采样
        sys.sensor_adc_ready = 0;
        sys.sensor_value_valid = 0;
    }
#endif
    sys.Auto_Mode = config.Auto_Mode;
}
