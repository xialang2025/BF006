#include "eeprom.h"
#include "system.h"
#include "configPar.h"
#include "config_store.h" //20261114-4 add
#include "Display.h"
#include "user.h"
//20261113-0 del 封存旧协议头文件引用：#include "updata4G.h"

#define BIAS_ADDR 0x0000
#define Fault_LOSEPHASE_Tdelay 15   // loss-phase protection delay
#define Fault_UNLOAD_Tdelay 3       // unload protection delay
#define Fault_UVP_Time 15           //
#define Fault_UVP_Recovery_Time 180 // s
#define Fault_NOLOAD_Tdelay 30      // s
#define Fault_NOLOAD_LIMET 1        // s

void Fault_ReStart(void);

sys_t sys;

bit Boot_Flag = 0;

#if (CUSTOMER_TEST_LIMIT_ENABLE == 1)
static bit Customer_Test_Limit_Fault = 0; //20260501-0 add customer test time-limit fault lock flag for the current power cycle
#endif

void sys_init(void)
{
    memset(&sys, 0, sizeof(sys_t));
    sys.xia_DIR = 1;
}
#if (Function_RS485 == 1)
void Record_Clr_and_Reset(void)
{
    u8 test_limit_active; //20260501-0 add customer test time-limit fault state before record clearing

    test_limit_active = Customer_Test_Limit_IsFaultActive();
    sys.Record_Dis_Flag = 0;
    Contact_Overload_Clear(sys.Pump_Add_Flag, &sys.Contact_Overload_A_Flag); //20260729-6 change 清除记录时同步处理 A 泵触点锁存
#if (Model_OUT == Model_1C2)
    Contact_Overload_Clear(sys.Flag_OverFlow, &sys.Contact_Overload_B_Flag); //20260729-7 add 清除记录时同步处理 B 泵触点锁存
#endif

    //    Timer_Standby_Cnt  = 0; // clear fault timer
    if (sys.Fault_Error)
    {
        sys.Pump_master--; // other flows add 1 and point to the next pump by default, so restore the previous pump here
    }
    sys.Fault_Error = 0; // clear fault code
    sys.Fault_A_Flag = 0;
    sys.Fault_B_Flag = 0;
    if (test_limit_active != 0) //20260501-3 change keep the ERR_OVP time-limit fault when record clearing runs
    {
        sys.Fault_Error |= (u32)1 << ERR_OVP;
        sys.Fault_A_Flag = 1;
        sys.Fault_B_Flag = 1;
    }
    if (config.Hand_Auto_timer_Flag == RUNMODE_MANUAL) // 手动
    {
        sys.Pump_Flag = 0;
        sys.Pump_A_ON = 0;
        sys.Pump_B_ON = 0;
    }
    else
    {
        sys.Pump_A_ON = 0;
        sys.Pump_B_ON = 0;
        sys.Pump_Flag = 0;
    }
}
#endif

/******************************************************************************
 * 函数名称       : updatSysdata
 * 功能描述       : 将配置量程和启停位换算为运行态的百分之一单位。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void updatSysdata(void) //20261114-2 change
{
#if (Model_OUT == Model_1C1)
    u32 range;

    range = (u32)config.Sensor_Rang * 10UL;
    if (range != sys.Sensor_Rang) { //20261114-2 add 换算所用量程变化后等待工程值重新发布
        sys.sensor_value_valid = 0;
    }
    sys.Sensor_Rang = range;
    sys.Sensor_STOP = (u32)config.Sensor_STOP * 10UL;
    sys.Sensor_ON1 = (u32)config.Sensor_ON1 * 10UL;
    sys.Sensor_ON2 = (u32)config.Sensor_ON2 * 10UL;
#else
    sys.Sensor_Rang = config.Sensor_Rang * 10;
    sys.Sensor_STOP = config.Sensor_STOP * 10;
    sys.Sensor_ON1 = config.Sensor_ON1 * 10;
    sys.Sensor_ON2 = config.Sensor_ON2 * 10;
#endif
}

#if (Model_OUT == Model_1C1)
/******************************************************************************
 * 函数名称       : Sensor_Legacy_Value
 * 功能描述       : 将连续量转换成旧寄存器编码，保留全一无效码。
 * 输入参数       : value 百分之一单位的连续量
 * 返回值         : 旧寄存器值
 * 日期            : 2026.11.14
 ******************************************************************************/
u16 Sensor_Legacy_Value(u32 value) //20261114-1 add
{
    if (value >= 0xFFFFUL) {
        return 0xFFFF;
    }
    return (u16)value;
}

/******************************************************************************
 * 函数名称       : Sensor_Legacy_Reading
 * 功能描述       : 读取当前类型的有效工程值并转换成旧寄存器编码。
 * 输入参数       : None
 * 返回值         : 旧寄存器值
 * 日期            : 2026.11.14
 ******************************************************************************/
u16 Sensor_Legacy_Reading(void) //20261114-1 add
{
    if (!sys.sensor_value_valid || config.Auto_Mode != sys.Auto_Mode) {
        return 0xFFFF;
    }
    return Sensor_Legacy_Value(sys.SensorOut);
}
#endif

void Write_Fault_Mess(u8 dat, u16 dat1, u16 dat2)
{

    u8 i = 0;

    for (i = 0; i < 9; i++)
    {
        config.Record_Type[9 - i] = config.Record_Type[8 - i];
        config.Record_Dat1[9 - i] = config.Record_Dat1[8 - i];
        config.Record_Dat2[9 - i] = config.Record_Dat2[8 - i];
    }

    config.Record_Type[0] = dat;
    config.Record_Dat1[0] = dat1;
    config.Record_Dat2[0] = dat2;

    sys.save_flag = 1;
    //20261113-0 del 停用旧协议的故障历史上报标志：Send_dat.GuZhangJiLu = 1;
}

void Water_Level(void)
{
    static u8 i = 0;
    if (i > 120)
    {
        if (LowWater_M == 1) // low water middle input
        {
            sys.xia_zhong_count++;
            if (sys.xia_zhong_count > 10)
            {
                sys.xia_zhong_count = 0;
                sys.xia_zhong_Flag = 1; //
            }
        }
        else
        {
            sys.xia_zhong_count = 0;
            sys.xia_zhong_Flag = 0;
        }

        if (LowWater_H == 1) // low water high input
        {
            sys.xia_gao_count++;
            if (sys.xia_gao_count > 10)
            {
                sys.xia_gao_Flag = 1;
                sys.xia_gao_count = 0;
            }
        }
        else
        {
            sys.xia_gao_count = 0;
            sys.xia_gao_Flag = 0;
        }

        // low water level check
        if (HighWater_M == 1) // high water middle input
        {
            sys.shang_zhong_count++;
            if (sys.shang_zhong_count > 10)
            {
                sys.shang_zhong_count = 0;
                sys.shang_zhong_Flag = 1;
            }
        }
        else
        {
            sys.shang_zhong_count = 0;
            sys.shang_zhong_Flag = 0;
        }
        //============
        if (HighWater_H == 1) // high water high input
        {
            sys.shang_gao_count++;
            if (sys.shang_gao_count > 10)
            {
                sys.shang_gao_Flag = 1;
                sys.shang_gao_count = 0;
            }
        }
        else
        {
            sys.shang_gao_count = 0;
            sys.shang_gao_Flag = 0;
        }

        if (PUMP_ADD == 1) // pump-add request input check
        {
            sys.Pump_Add_count++;
            if (sys.Pump_Add_count > 10)
            {
                sys.Pump_Add_Flag = 1; // pump add is required
                sys.Pump_Add_count = 0;
            }
        }
        else
        {
            sys.Pump_Add_count = 0;
            sys.Pump_Add_Flag = 0; // pump add is not required
        }

        if (OverFlow_P == 1) // overflow pump-add input check
        {
            sys.Flag_Over_count++;
            if (sys.Flag_Over_count > 10)
            {
                sys.Flag_OverFlow = 1; // overflow pump add is required
                sys.Flag_Over_count = 0;
            }
        }
        else
        {
            sys.Flag_Over_count = 0;
            sys.Flag_OverFlow = 0; // overflow pump add is not required
        }
    }

    if (i > 100)
    {
        P34 = 1;
    }
    else
    {
        P34 = 0;
    }

    if (++i > 150)
    {
        i = 0;
    }
}

void shangDianJiShuiwei_Check(void)
{
    static u16 shang1_timer = 0;
    static u16 shang2_timer = 0;
    if (sys.shang_zhong_Flag == 0 && sys.shang_gao_Flag == 0)
    {
        shang2_timer = 0;

        if (shang1_timer >= 2)
        {
            sys.shang_shuiwei = 0;
            sys.shang_DIR = 0;
        }
        else
        {
            shang1_timer++;
        }
    }
    else if (sys.shang_zhong_Flag == 1 && sys.shang_gao_Flag == 1)
    {
        shang1_timer = 0;

        if (shang2_timer >= 2)
        {
            sys.shang_shuiwei = 2;
            sys.shang_DIR = 1;
        }
        else
        {
            shang2_timer++;
        }
    }
    else
    {
        sys.shang_shuiwei = 1;
        shang1_timer = 0;
        shang2_timer = 0;
    }
}

void xiaDianJiShuiwei_Check(void)
{
    static u16 xia1_timer = 0;
    static u16 xia2_timer = 0;
    // low water tank
    if (sys.xia_zhong_Flag == 0 && sys.xia_gao_Flag == 0) // dry-run protection condition
    {
        xia2_timer = 0;
        xia1_timer++;
        if (xia1_timer >= 2)
        {
            xia1_timer = 0;
            sys.xia_shuiwei = 0;
            sys.xia_DIR = 0;
        }
    }
    else if (sys.xia_zhong_Flag == 1 && sys.xia_gao_Flag == 1) // normal water condition
    {
        xia1_timer = 0;
        xia2_timer++;
        if (xia2_timer >= 2)
        {
            xia2_timer = 0;

            sys.xia_shuiwei = 2;
            sys.xia_DIR = 1;
        }
    }
    else
    {
        sys.xia_shuiwei = 1;

        xia1_timer = 0;
        xia2_timer = 0;
    }
}

void YaLiCtr_Check(void)
{
    if (sys.shang_zhong_Flag == 1 && sys.shang_gao_Flag == 0)
    {
        sys.shang_shuiwei = 1;
        sys.shang_DIR = 0; //
    }
    else if (sys.shang_zhong_Flag == 0 && sys.shang_gao_Flag == 1)
    {
        sys.shang_shuiwei = 1;
        sys.shang_DIR = 1; //
    }
    else if (sys.shang_zhong_Flag == 0 && sys.shang_gao_Flag == 0)
    {
        sys.shang_shuiwei = 0;
    }
    else
    {
        sys.shang_shuiwei = 2;
    }
}

#if (Model_OUT == Model_1C1)
/******************************************************************************
 * 函数名称       : Sensor_Read_ADC
 * 功能描述       : 读取本类型已发布的完整转换计数。
 * 输入参数       : adc 转换计数地址
 * 返回值         : 转换计数
 * 日期            : 2026.11.14
 ******************************************************************************/
static u16 Sensor_Read_ADC(volatile u16 *adc) //20261114-1 add
{
    bit interrupt_enabled;
    u16 sample;

    interrupt_enabled = EA;
    EA = 0;
    sample = *adc; // 中断更新两个字节时，主循环只读取完整的计数
    EA = interrupt_enabled;

    return sample;
}
#endif

/******************************************************************************
 * 函数名称       : CUR4_20Ma_Check
 * 功能描述       : 计算电流型传感器的工程值和断线状态。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void CUR4_20Ma_Check(void) //20261114-1 change
{
    u32 temp;
#if (Model_OUT == Model_1C1)
    u16 sample;

    sample = Sensor_Read_ADC(&sys.Cur_AD);
    sys.sensor_err = (sample < 100);
    sys.SensorOut = 0;
    if (sample >= 196) {
        if (sample > 983) {
            sample = 983; // 夹住过量程的转换计数，保留原上边界
        }
        temp = ((u32)(sample - 196) * sys.Sensor_Rang + 393UL) / 786UL;
        sys.SensorOut = temp;
    }
#else
    if (sys.Cur_AD < 100)
    {
        sys.sensor_err = 1;
        sys.SensorOut = 0;
    }
    else if (sys.Cur_AD < 196)
    {
        sys.SensorOut = 0;
        sys.sensor_err = 0;
    }
    else
    {
        if (sys.Cur_AD > 983)
        {
            sys.Cur_AD = 983;
        }
        sys.sensor_err = 0;
        temp = sys.Cur_AD - 196;
        temp *= sys.Sensor_Rang;
        temp += 393;
        sys.SensorOut = temp / 786;
    }
#endif
}

/******************************************************************************
 * 函数名称       : Vol0_5V_Check
 * 功能描述       : 计算电压型传感器的工程值和断线状态。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void Vol0_5V_Check(void) //20261114-1 change
{
    u32 temp;
#if (Model_OUT == Model_1C1)
    u16 sample;

    sample = Sensor_Read_ADC(&sys.Vol_AD);
    sys.sensor_err = (sample < 2);
    sys.SensorOut = 0;
    if (sample > 10) {
        temp = ((u32)(sample - 10) * sys.Sensor_Rang + 506UL) / 1013UL;
        sys.SensorOut = temp;
    }
#else
    if (sys.Vol_AD < 2)
    {
        sys.SensorOut = 0;
        sys.sensor_err = 1;
    }
    else if (sys.Vol_AD > 10)
    {

        sys.sensor_err = 0;
        temp = sys.Sensor_Rang;
        temp *= (sys.Vol_AD - 10);
        temp += 506;
        sys.SensorOut = temp / 1013;
    }
    else
    {
        sys.sensor_err = 0;
        sys.SensorOut = 0;
    }
#endif
}

void sensorOverFlowCheck(void)
{
#if (Model_OUT == Model_1C1)
    //20261114-1 change 溢出阈值与工程值采用相同单位和位宽
    if (sys.SensorOut > (u32)config.Sensor_overFlow * 10UL)
#else
    if (sys.SensorOut > config.Sensor_overFlow * 10)
#endif
    {
        sys.Sensor_OverFlow = 1;
    }
    else
    {
        sys.Sensor_OverFlow = 0;
    }
}

// water level check
void shuiwei_check(void)
{
#if (Model_OUT == Model_1C1)
    u8 analog_mode;

    analog_mode = (sys.Auto_Mode >= SENSORTYPE_WATER4_20MA &&
                   sys.Auto_Mode <= SENSORTYPE_PRESS0_5V);
    if (analog_mode && !sys.sensor_adc_ready) { //20261114-1 add 等待采样时不报断线
        sys.SensorOut = 0;
        sys.sensor_err = 0;
        sys.Sensor_OverFlow = 0;
        sys.sensor_value_valid = 0;
        return;
    }
#endif
    if (config.Sensor_Mode == CTROLTYPE_PUMP) // sensor is installed on upper tank
    {

        xiaDianJiShuiwei_Check();

        if (sys.Auto_Mode == SENSORTYPE_PRESS) // pressure mode
        {
            sys.sensor_err = 0;
            YaLiCtr_Check();
            sys.Sensor_OverFlow = 0;
        }
        else if (sys.Auto_Mode == SENSORTYPE_WATER4_20MA) // ?λ
        {
            CUR4_20Ma_Check();
            sensorOverFlowCheck();
        }
        else if (sys.Auto_Mode == SENSORTYPE_PRESS4_20MA) // pressure 4-20 mA mode
        {

            CUR4_20Ma_Check();
            sensorOverFlowCheck();
        }
        else if (sys.Auto_Mode == SENSORTYPE_PRESS0_5V) // remote pressure mode
        {

            Vol0_5V_Check();
            sensorOverFlowCheck();
        }
        else // ?λ
        {
            sys.sensor_err = 0;

            shangDianJiShuiwei_Check();
            sys.Sensor_OverFlow = 0;
        }

        if (sys.sensor_err)
        {
            Speak_Con(5);
        }
    }
    else
    {

        if (sys.Auto_Mode == SENSORTYPE_PRESS)
        {

            YaLiCtr_Check();
            xiaDianJiShuiwei_Check();
            sys.sensor_err = 0;
            sys.Sensor_OverFlow = 0;
        }
        else
        {

            shangDianJiShuiwei_Check();
        }

        if (sys.Auto_Mode == SENSORTYPE_WATER4_20MA) // ?λ
        {

            CUR4_20Ma_Check();
            sensorOverFlowCheck();
        }
        else if (sys.Auto_Mode == SENSORTYPE_PRESS4_20MA) // ?λ
        {

            CUR4_20Ma_Check();
            sensorOverFlowCheck();
        }
        else if (sys.Auto_Mode == SENSORTYPE_PRESS0_5V) // ?λ
        {

            Vol0_5V_Check();
            sensorOverFlowCheck();
        }
        else
        {

            sys.sensor_err = 0;
            sys.Sensor_OverFlow = 0;

            xiaDianJiShuiwei_Check();
        }
        if (sys.sensor_err)
        {
            Speak_Con(5);
        }
    }
#if (Model_OUT == Model_1C1)
    sys.sensor_value_valid = (analog_mode && !sys.sensor_err); //20261114-1 add
#endif
}

void RELAY_Auto_Control(void)
{
    if (config.Hand_Auto_timer_Flag == RUNMODE_AUTO)
    {
        if (config.Sensor_Mode == CTROLTYPE_PUMP) // sensor is installed on upper tank
        {
            if (sys.Auto_Mode == SENSORTYPE_PRESS) // pressure mode
            {
                if (sys.Pump_Flag == 0)
                {
                    if ((sys.xia_DIR == 1) && (sys.shang_shuiwei == 1 && sys.shang_DIR == 1)) // upper tank reaches stop level and starts delay check
                    {
                        sys.Pump_Flag = 1;
                    }
                }
                else
                {

                    if ((sys.xia_DIR == 0) || (sys.shang_shuiwei == 1 && sys.shang_DIR == 0))
                    {
                        sys.Pump_Flag = 0;
                    }
                }
            }
            else if (sys.Auto_Mode >= SENSORTYPE_WATER4_20MA) // ?λ
            {
                if (sys.Pump_Flag == 0)
                {
                    if (sys.SensorOut <= sys.Sensor_ON1)
                    {

                        sys.Pump_Flag = 1;
                    }
#if (Model_OUT == Model_1C2)
                    else if (sys.SensorOut <= sys.Sensor_ON2)
                    {

                        sys.Pump_Flag = 2;
                    }
#endif
                }
                else if (sys.Pump_Flag == 1)
                {

                    if (sys.SensorOut >= sys.Sensor_STOP)
                    {

                        sys.Pump_Flag = 0;
                    }
#if (Model_OUT == Model_1C2)
                    else if (sys.SensorOut <= sys.Sensor_ON2)
                    {

                        sys.Pump_Flag = 2;
                    }
#endif
                }
                else
                {
                    if (sys.SensorOut >= sys.Sensor_STOP)
                    {

                        sys.Pump_Flag = 0;
                    }
                }
                if (sys.xia_DIR == 0)
                {
                    sys.Pump_Flag = 0;
                }
            }
            else // ?λ
            {
                if (sys.Pump_Flag == 0)
                {
                    if ((sys.xia_DIR == 1) && (sys.shang_DIR == 0)) // upper tank reaches stop level and starts delay check
                    {
                        sys.Pump_Flag = 1;
                    }
                }
                else
                {

                    if ((sys.xia_DIR == 0) || (sys.shang_DIR == 1)) // dry-run or upper tank full condition
                    {
                        sys.Pump_Flag = 0;
                    }
                }
#if 0 //20260729-6 del 加泵位改为 A 泵触点过载输入，不再参与自动加泵
                if ((sys.Pump_Add_Flag == 1) && (sys.shang_DIR == 0))
                {
                    sys.Pump_Flag = 2;
                }
#endif
            }
        }
        else
        { // sensor is installed on lower tank

            if (sys.Auto_Mode == SENSORTYPE_PRESS) // pressure mode
            {
                if (sys.Pump_Flag == 0)
                {
                    if ((sys.xia_DIR == 1) && (sys.shang_shuiwei == 1 && sys.shang_DIR == 1)) // upper tank reaches stop level and starts delay check
                    {
                        sys.Pump_Flag = 1;
                    }
                }
                else
                {

                    if ((sys.xia_DIR == 0) || (sys.shang_shuiwei == 1 && sys.shang_DIR == 0))
                    {
                        sys.Pump_Flag = 0;
                    }
                }
            }
            else if (sys.Auto_Mode >= SENSORTYPE_WATER4_20MA) // ?λ
            {
                if (sys.Pump_Flag == 0)
                {
                    if (sys.SensorOut >= sys.Sensor_ON1)
                    {

                        sys.Pump_Flag = 1;
                    }
#if (Model_OUT == Model_1C2)
                    else if (sys.SensorOut >= sys.Sensor_ON2)
                    {

                        sys.Pump_Flag = 2;
                    }
#endif
                }
                else if (sys.Pump_Flag == 1)
                {

                    if (sys.SensorOut <= sys.Sensor_STOP)
                    {

                        sys.Pump_Flag = 0;
                    }
#if (Model_OUT == Model_1C2)
                    else if (sys.SensorOut >= sys.Sensor_ON2)
                    {

                        sys.Pump_Flag = 2;
                    }
#endif
                }
                else
                {
                    if (sys.SensorOut <= sys.Sensor_STOP)
                    {

                        sys.Pump_Flag = 0;
                    }
                }
                if (sys.shang_DIR == 1)
                {

                    sys.Pump_Flag = 0;
                }
            }
            else // ?λ
            {
                if (sys.Pump_Flag == 0)
                {
                    if ((sys.xia_DIR == 1) && (sys.shang_DIR == 0)) // upper tank reaches stop level and starts delay check
                    {
                        sys.Pump_Flag = 1;
                    }
                }

                else
                {

                    if ((sys.xia_DIR == 0) || (sys.shang_DIR == 1)) // dry-run or upper tank full condition
                    {
                        sys.Pump_Flag = 0;
                    }
                }

#if 0 //20260729-6 del 加泵位改为 A 泵触点过载输入，不再参与自动加泵
                if ((sys.Pump_Add_Flag == 1) && (sys.shang_DIR == 0))
                {
                    sys.Pump_Flag = 2;
                }
#endif
            }
        }

#if (Model_OUT == Model_1C1)
        if (sys.sensor_err || (sys.Auto_Mode >= SENSORTYPE_WATER4_20MA &&
                               !sys.sensor_value_valid)) //20261114-1 change 未收到有效连续量时禁止自动开泵
#else
        if (sys.sensor_err)
#endif
        {
            sys.Pump_Flag = 0;
        }
    }
    else if (config.Hand_Auto_timer_Flag == RUNMODE_TIMER)
    {
        if (sys.autoTimerFlag)
        {
            if ((sys.Fault_A_Flag == 0) || (sys.Fault_B_Flag == 0))
            {
                if (sys.Auto_timer_count >= config.AUTO_Timing_Run * 60)
                {
                    sys.Auto_timer_count = 0;
                    sys.autoTimerFlag = 0;
                }
            }
            sys.Pump_Flag = 1;
        }
        else
        {
            if (sys.Auto_timer_count >= config.AUTO_Timing_Stop * 60)
            {
                sys.Auto_timer_count = 0;
                sys.autoTimerFlag = 1;
            }
            sys.Pump_Flag = 0;
        }
    }
    else
    {

        sys.Pump_Flag = 0;
    }

    if (config.Sensor_Mode == CTROLTYPE_PUMP) // sensor is installed on upper tank
    {
        if (sys.Auto_Mode >= SENSORTYPE_WATER4_20MA) // ?λ
        {
#if (Model_OUT == Model_1C2)
            if (sys.SensorOut >= sys.Sensor_STOP)
            {
                sys.SensorLever = 2;
            }
            else if (sys.SensorOut >= sys.Sensor_ON1)
            {
                if (sys.SensorLever == 0)
                {
                    sys.SensorLever = 1;
                }
            }
            else if (sys.SensorOut >= sys.Sensor_ON2)
            {
                if (sys.SensorLever == 2)
                {
                    sys.SensorLever = 1;
                }
            }
            else
            {
                sys.SensorLever = 0;
            }
#else
            if (sys.SensorOut >= sys.Sensor_STOP)
            {
                sys.SensorLever = 2;
            }
            else if (sys.SensorOut >= sys.Sensor_ON1)
            {
                if (sys.SensorLever == 0)
                {
                    sys.SensorLever = 1;
                }
            }
            else
            {
                sys.SensorLever = 0;
            }
#endif
        }
    }
    else
    { // sensor is installed on lower tank

        if (sys.Auto_Mode >= SENSORTYPE_WATER4_20MA) // ?λ
        {
#if (Model_OUT == Model_1C2)
            if (sys.SensorOut >= sys.Sensor_ON2)
            {
                sys.SensorLever = 2;
            }
            else if (sys.SensorOut >= sys.Sensor_ON1)
            {
                sys.SensorLever = 1;
            }
            else if (sys.SensorOut <= sys.Sensor_STOP)
            {
                sys.SensorLever = 0;
            }
#else
            if (sys.SensorOut >= sys.Sensor_ON1)
            {
                sys.SensorLever = 2;
            }
            else if (sys.SensorOut > sys.Sensor_STOP)
            {
                sys.SensorLever = 1;
            }
            else
            {
                sys.SensorLever = 0;
            }

#endif

            if (sys.shang_DIR == 1)
            {

                sys.Pump_Flag = 0;
            }
        }
    }
}

void Timer_mode(void)
{
    if (config.Hand_Auto_timer_Flag == RUNMODE_TIMER)
    {
        if (sys.autoTimerFlag)
        {
            if ((sys.Fault_A_Flag == 0) || (sys.Fault_B_Flag == 0))
            {
                sys.Auto_timer_count++;
            }
        }
        else
        {
            if (config.AUTO_Timing_Stop > 0)
            {
                sys.Auto_timer_count++;
            }
        }
    }
    else if (config.shuiWeiDingshi && (sys.Auto_Mode == SENSORTYPE_POLE) && (sys.Pump_Flag > 0))
    {
        sys.Auto_timer_count++;
    }
    else
    {
        sys.Auto_timer_count = 0;
    }
}

void Sys_Control(void)
{
    static u8 Relay1_delay = 0;
    static u8 Relay2_delay = 0;

    static u8 Pump_A_state = 0;
    static u8 Pump_B_state = 0;
    static u8 Pump_C_state = 0;

    static u8 Pump_Flag_old = 0;

#if BF006_CONFIG_STORE_ENABLED
    if (!config_trusted) { //20261114-4 add 配置证据缺失时保持停机
        sys.Pump_Flag = 0;
        sys.Pump_A_ON = 0;
        sys.Pump_B_ON = 0;
        RELAY1 = 0;
        RELAY2 = 0;
        sys.Pump_A_state = 0;
        sys.Pump_B_state = 0;
        return;
    }
#endif
    if (sys.Pump_Flag == 1)
    {
        if (Pump_Flag_old == 0)
        {
            sys.Pump_master++;
        }
    }
    Pump_Flag_old = sys.Pump_Flag;

    if (config.Hand_Auto_timer_Flag > RUNMODE_MANUAL)
    {
#if (Model_OUT == Model_1C2)
        if (sys.Pump_Flag == 2)
        {
            if (sys.Fault_A_Flag == 0)
            {
                sys.Pump_A_ON = 1;
            }

            if (sys.Fault_B_Flag == 0)
            {
                sys.Pump_B_ON = 1;
            }
        }
        else if (sys.Pump_Flag == 1)
        {

            if (config.Pump_AB_Mode == 0) // A pump first, then B pump
            {
                if (sys.Fault_A_Flag == 0)
                {
                    sys.Pump_A_ON = 1;
                    sys.Pump_B_ON = 0;
                }
                else if (sys.Fault_B_Flag == 0)
                {
                    sys.Pump_B_ON = 1;
                    sys.Pump_A_ON = 0;
                }
            }
            else if (config.Pump_AB_Mode == 1) // B pump first, then A pump
            {
                if (sys.Fault_B_Flag == 0)
                {
                    sys.Pump_B_ON = 1;
                    sys.Pump_A_ON = 0;
                }
                else if (sys.Fault_A_Flag == 0)
                {
                    sys.Pump_A_ON = 1;
                    sys.Pump_B_ON = 0;
                }
            }
            else
            { // rotation mode
                if ((sys.Pump_master & 0x01) == 0)
                {
                    if (sys.Fault_A_Flag == 0)
                    {
                        sys.Pump_A_ON = 1;
                        sys.Pump_B_ON = 0;
                    }
                    else if (sys.Fault_B_Flag == 0)
                    {
                        sys.Pump_B_ON = 1;
                        sys.Pump_A_ON = 0;
                    }
                }
                else
                {

                    if (sys.Fault_B_Flag == 0)
                    {
                        sys.Pump_B_ON = 1;
                        sys.Pump_A_ON = 0;
                    }
                    else if (sys.Fault_A_Flag == 0)
                    {
                        sys.Pump_A_ON = 1;
                        sys.Pump_B_ON = 0;
                    }
                }
            }
        }
        else
        {

            sys.Pump_A_ON = 0;
            sys.Pump_B_ON = 0;
        }

#elif (Model_OUT == Model_1C1)

        if (sys.Pump_Flag)
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

#endif
    }

    if ((sys.Fault_A_Flag == 0) && (config.Lock1 == 0))
    {

        if (sys.Pump_A_ON || sys.FangXiuAFlag)
        {
            sys.Fang_Xiu_count_A = 0;
            sys.Pump_A_state = 1;
        }
        else
        {
            sys.Pump_A_state = 0;
        }
    }
    else
    {
        sys.Pump_A_state = 0;
        sys.Pump_A_ON = 0;
        Pump_A_state = 0;
    }

    if ((sys.Fault_B_Flag == 0) && (config.Lock2 == 0))
    {
        if (sys.Pump_B_ON || sys.FangXiuBFlag)
        {
            sys.Fang_Xiu_count_B = 0;
            sys.Pump_B_state = 1;
        }
        else
        {
            sys.Pump_B_state = 0;
        }
    }
    else
    {
        sys.Pump_B_ON = 0;
        sys.Pump_B_state = 0;
        Pump_B_state = 0;
    }

    if (Relay1_delay == 0)
    {
        if (Pump_A_state != sys.Pump_A_state)
        {
            Relay1_delay = 5;
            Pump_A_state = sys.Pump_A_state;
        }
    }
    else
    {
        Relay1_delay--;
    }

    if (Relay2_delay == 0)
    {
        if (Pump_B_state != sys.Pump_B_state)
        {
            Relay2_delay = 5;
            Pump_B_state = sys.Pump_B_state;
        }
    }
    else
    {
        Relay2_delay--;
    }

    if (Pump_A_state)
    {

        RELAY1 = 1;
    }
    else
    {

        RELAY1 = 0;
    }

#if (Model_OUT == Model_1C2)
    if (Pump_B_state)
    {

        RELAY2 = 1;
    }
    else
    {

        RELAY2 = 0;
    }
#endif

    //20261113-0 del 封存旧协议的继电器控制引用：Pump_C_state = sys.GateSW;
    Pump_C_state = sys.Faulte_out || sys.flowover;

    if (Pump_C_state)
    {

        RELAY3 = 1;
    }
    else
    {

        RELAY3 = 0;
    }
}

void FanXiu_pro(void)
{

    if (config.AUTO_FANGXIU == 0)
    {
        sys.Fang_Xiu_count_A = 0;
        sys.Fang_Xiu_count_B = 0;
        sys.FangXiuAFlag = 0;
        sys.FangXiuBFlag = 0;
        return;
    }

    if (sys.Pump_B_state)
    {
        sys.Fang_Xiu_count_B = 0;
    }
    else
    {
        sys.Fang_Xiu_count_B++;
    }

    if (sys.Fang_Xiu_count_B >= config.AUTO_FANGXIU * 600)
    {
        sys.FangXiuBFlag = 1;
        sys.Fang_Xiu_count_B = 0;
        sys.Fang_Xiu_B_Down = 4;
    }

    if (sys.Pump_A_state)
    {
        sys.Fang_Xiu_count_A = 0;
    }
    else
    {
        sys.Fang_Xiu_count_A++;
    }

    if (sys.Fang_Xiu_count_A >= config.AUTO_FANGXIU * 600)
    {
        sys.FangXiuAFlag = 1;
        sys.Fang_Xiu_count_A = 0;
        sys.Fang_Xiu_A_Down = 4;
    }

    if (sys.FangXiuAFlag)
    {
        if (sys.Fang_Xiu_A_Down == 0)
        {
            sys.FangXiuAFlag = 0;
        }
        else
        {

            sys.Fang_Xiu_A_Down--;
        }
    }

    if (sys.FangXiuBFlag)
    {
        if (sys.Fang_Xiu_B_Down == 0)
        {
            sys.FangXiuBFlag = 0;
        }
        else
        {

            sys.Fang_Xiu_B_Down--;
        }
    }
}

void Timer100ms_pro(void)
{
    if (sys.Pump_A_ON || sys.Pump_B_ON)
    {
        if (sys.Timer_Startup_Counter < 600)
            sys.Timer_Startup_Counter++;
    }
    else
    {

        sys.Timer_Startup_Counter = 0;
    }

    if (sys.Pump_A_ON)
    {
        if (sys.Pump1_Startup_Counter < 600)
            sys.Pump1_Startup_Counter++;
    }
    else
    {

        sys.Pump1_Startup_Counter = 0;
    }

    if (sys.Pump_B_ON)
    {
        if (sys.Pump2_Startup_Counter < 600)
            sys.Pump2_Startup_Counter++;
    }
    else
    {

        sys.Pump2_Startup_Counter = 0;
    }
}

void Fault_Error_Set(u8 err)
{
    sys.Fault_Error |= (u32)1 << err;
}

void Fault_Error_Clear(u8 err)
{
#if (CUSTOMER_TEST_LIMIT_ENABLE == 1)
    if ((Customer_Test_Limit_Fault != 0) && (err == ERR_OVP)) //20260501-3 change block clearing over-voltage fault after time-limit fault is locked
    {
        return;
    }
#endif
    sys.Fault_Error &= ~((u32)1 << err);
}

u8 Fault_Error_Check(u8 err)
{
    return ((sys.Fault_Error & ((u32)1 << err)) > 0);
}
#if (Model_Voltage == Model_3PH)
void Pump_Current_Check(void)
{
    u16 MaxValue = 0;
    u16 MinValue = 1000;

    if (sys.Current_A1 > MaxValue)
    {
        MaxValue = sys.Current_A1;
        sys.Current1_Max_Index = 1;
    }
    if (sys.Current_B1 > MaxValue)
    {
        MaxValue = sys.Current_B1;
        sys.Current1_Max_Index = 2;
    }
    if (sys.Current_C1 > MaxValue)
    {
        MaxValue = sys.Current_C1;
        sys.Current1_Max_Index = 3;
    }

    if (sys.Current_A1 < MinValue)
    {
        MinValue = sys.Current_A1;
        sys.Current1_Min_Index = 1;
    }
    if (sys.Current_B1 < MinValue)
    {
        MinValue = sys.Current_B1;
        sys.Current1_Min_Index = 2;
    }
    if (sys.Current_C1 < MinValue)
    {
        MinValue = sys.Current_C1;
        sys.Current1_Min_Index = 3;
    }

    sys.Current1_Max = MaxValue;
    sys.Current1_Min = MinValue;

    MaxValue = 0;
    MinValue = 1000;

    if (sys.Current_A2 > MaxValue)
    {
        MaxValue = sys.Current_A2;
        sys.Current2_Max_Index = 1;
    }
    if (sys.Current_B2 > MaxValue)
    {
        MaxValue = sys.Current_B2;
        sys.Current2_Max_Index = 2;
    }
    if (sys.Current_C2 > MaxValue)
    {
        MaxValue = sys.Current_C2;
        sys.Current2_Max_Index = 3;
    }

    if (sys.Current_A2 < MinValue)
    {
        MinValue = sys.Current_A2;
        sys.Current2_Min_Index = 1;
    }
    if (sys.Current_B2 < MinValue)
    {
        MinValue = sys.Current_B2;
        sys.Current2_Min_Index = 2;
    }
    if (sys.Current_C2 < MinValue)
    {
        MinValue = sys.Current_C2;
        sys.Current2_Min_Index = 3;
    }

    sys.Current2_Max = MaxValue;
    sys.Current2_Min = MinValue;
}
#endif
void DIS_Fault(u8 num)
{
    sys.Err_Dis = num;
}

void Fault_Protect(void)
{
    sys.Fault_A_Flag = 1;
    sys.Fault_B_Flag = 1;
    RELAY1 = 0;
    RELAY2 = 0;
    sys.Pump_A_ON = 0;
    sys.Pump_B_ON = 0;
}

/******************************************************************************
 * Function       : Customer_Test_Limit_IsFaultActive
 * Description       : Check whether the customer test time-limit fault is locked in the current power cycle.
 * Input       : None
 * Return         : 1 locked, 0 unlocked
 * Date            : 2026.05.01
 ******************************************************************************/
u8 Customer_Test_Limit_IsFaultActive(void) //20260501-0 add
{
#if (CUSTOMER_TEST_LIMIT_ENABLE == 1)
    if (Customer_Test_Limit_Fault != 0) // time-limit fault is valid only in this power cycle; the static flag resets after power loss
    {
        return 1;
    }
#endif
    return 0;
}

/******************************************************************************
 * Function       : Customer_Test_Limit_Handle
 * Description       : Trigger a non-clearable customer test time-limit fault after the configured power-on minutes.
 * Input       : None
 * Return         : None
 * Date            : 2026.05.01
 ******************************************************************************/
void Customer_Test_Limit_Handle(void) //20260501-0 add
{
#if (CUSTOMER_TEST_LIMIT_ENABLE == 1)
    static u16 limit_second = 0; // seconds accumulated in the current minute
    static u16 limit_minute = 0; // minutes accumulated since this power-on

    if (Customer_Test_Limit_Fault != 0) // after locking, reapply output shutdown and display each second to prevent other flows from clearing the state
    {
        Fault_Protect();
        Fault_Error_Set(ERR_OVP);
        DIS_Fault(ERR_OVP);
        return;
    }

    limit_second++;
    if (limit_second >= 60)
    {
        limit_second = 0;
        if (limit_minute < CUSTOMER_TEST_LIMIT_MINUTE)
        {
            limit_minute++;
        }
    }

    if (limit_minute >= CUSTOMER_TEST_LIMIT_MINUTE) //20260501-3 change lock over-voltage fault when the customer test time limit is reached
    {
        Customer_Test_Limit_Fault = 1;
        Fault_Protect();
        Fault_Error_Set(ERR_OVP);
        DIS_Fault(ERR_OVP);
    }
#endif
}

void Fault_Protect_PA(void)
{
    RELAY1 = 0;
    sys.Fault_A_Flag = 1;
    sys.Pump_A_ON = 0;
}
#if (Model_OUT == Model_1C2)
void Fault_Protect_PB(void)
{
    RELAY2 = 0;
    sys.Fault_B_Flag = 1;
    sys.Pump_B_ON = 0;
}
#endif
void clear_A_Fault(void)
{

    sys.Fault_Error &= ~((u32)1 << ERR_LOSE_A1);
    sys.Fault_Error &= ~((u32)1 << ERR_LOSE_B1);
    sys.Fault_Error &= ~((u32)1 << ERR_LOSE_C1);
    sys.Fault_Error &= ~((u32)1 << ERR_OVERLOAD1);
    sys.Fault_Error &= ~((u32)1 << ERR_STANDBY1);
    sys.Fault_Error &= ~((u32)1 << ERR_UVP);
    if (Customer_Test_Limit_IsFaultActive() == 0) //20260501-3 change time-limit fault uses ERR_OVP and must stay locked
    {
        sys.Fault_Error &= ~((u32)1 << ERR_OVP);
    }
    else
    {
        sys.Fault_Error |= (u32)1 << ERR_OVP;
        sys.Fault_A_Flag = 1;
        sys.Fault_B_Flag = 1;
    }
    sys.Fault_Error &= ~((u32)1 << ERR_NOLOAD_A);
    if (Customer_Test_Limit_IsFaultActive() == 0) //20260501-3 change keep A fault flag locked for customer test fault
    {
        sys.Fault_A_Flag = 0;
    }
    if (sys.Contact_Overload_A_Flag != 0) //20260729-6 change 触点仍接通时清除后继续保持 A 泵过载
    {
        if (Contact_Overload_Clear(sys.Pump_Add_Flag, &sys.Contact_Overload_A_Flag) == 0)
        {
            sys.Err_Dis = ERR_OVERLOAD1;
            RELAY1 = 0; //20260729-6 change 遥控中断清除时直接保持 A 泵停机，避免跨上下文调用
            sys.Fault_A_Flag = 1;
            sys.Pump_A_ON = 0;
            sys.Fault_Error |= (u32)1 << ERR_OVERLOAD1;
        }
    }

    if ((sys.Fault_Error == 0) || (config.Lock1) || (config.Lock2))
    {
        Speak_Stop();
    }
}

void clear_B_Fault(void)
{
    sys.Fault_Error &= ~((u32)1 << ERR_LOSE_A2);
    sys.Fault_Error &= ~((u32)1 << ERR_LOSE_B2);
    sys.Fault_Error &= ~((u32)1 << ERR_LOSE_C2);
    sys.Fault_Error &= ~((u32)1 << ERR_OVERLOAD2);
    sys.Fault_Error &= ~((u32)1 << ERR_STANDBY2);
    sys.Fault_Error &= ~((u32)1 << ERR_UVP);
    if (Customer_Test_Limit_IsFaultActive() == 0) //20260501-3 change time-limit fault uses ERR_OVP and must stay locked
    {
        sys.Fault_Error &= ~((u32)1 << ERR_OVP);
        sys.Fault_Error &= ~((u32)1 << ERR_NOLOAD_B);
        sys.Fault_B_Flag = 0;
    }
    else
    {
        sys.Fault_Error |= (u32)1 << ERR_OVP;
        sys.Fault_A_Flag = 1;
        sys.Fault_B_Flag = 1;
    }
#if (Model_OUT == Model_1C2)
    if (sys.Contact_Overload_B_Flag != 0) //20260729-7 add 触点仍接通时清除后继续保持 B 泵过载
    {
        if (Contact_Overload_Clear(sys.Flag_OverFlow, &sys.Contact_Overload_B_Flag) == 0)
        {
            sys.Err_Dis = ERR_OVERLOAD2;
            RELAY2 = 0; //20260729-7 add 遥控中断清除时直接保持 B 泵停机，避免跨上下文调用
            sys.Fault_B_Flag = 1;
            sys.Pump_B_ON = 0;
            sys.Fault_Error |= (u32)1 << ERR_OVERLOAD2;
        }
    }
#endif

    if ((sys.Fault_Error == 0) || (config.Lock2))
    {
        Speak_Stop();
    }
}

u8 check_A_Fault(void)
{

    u8 re = 0;
    if (sys.Fault_Error & ((u32)1 << ERR_LOSE_A1))
        re++;
    if (sys.Fault_Error & ((u32)1 << ERR_LOSE_B1))
        re++;
    if (sys.Fault_Error & ((u32)1 << ERR_LOSE_C1))
        re++;
    if (sys.Fault_Error & ((u32)1 << ERR_OVERLOAD1))
        re++;
    if (sys.Fault_Error & ((u32)1 << ERR_STANDBY1))
        re++;
    if (sys.Fault_Error & ((u32)1 << ERR_UVP))
        re++;
    if (sys.Fault_Error & ((u32)1 << ERR_OVP))
        re++;

    return re;
}

u8 check_B_Fault(void)
{
    u8 re = 0;
    if (sys.Fault_Error & ((u32)1 << ERR_LOSE_A2))
        re++;
    if (sys.Fault_Error & ((u32)1 << ERR_LOSE_B2))
        re++;
    if (sys.Fault_Error & ((u32)1 << ERR_LOSE_C2))
        re++;
    if (sys.Fault_Error & ((u32)1 << ERR_OVERLOAD2))
        re++;
    if (sys.Fault_Error & ((u32)1 << ERR_STANDBY2))
        re++;
    if (sys.Fault_Error & ((u32)1 << ERR_UVP))
        re++;
    if (sys.Fault_Error & ((u32)1 << ERR_OVP))
        re++;

    return re;
}

void Set_Pump1_OverLoad_Err(void)
{
    DIS_Fault(ERR_OVERLOAD1);
    Fault_Protect_PA();
    Fault_Error_Set(ERR_OVERLOAD1);
    Write_Fault_Mess(ERR_OVERLOAD1, sys.Voltage_Line, sys.avgCurrent1);
    Speak_Con(4);
}

/******************************************************************************
 * 函数名称       : Contact_Overload_Control
 * 功能描述       : 在前台控制周期处理滤波后的触点过载保护。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.07.29
 ******************************************************************************/
void Contact_Overload_Control(void) //20260729-7 change
{
    u8 first_trigger_a;
#if (Model_OUT == Model_1C2)
    u8 first_trigger_b;
#endif

    first_trigger_a = Contact_Overload_Trigger(sys.Pump_Add_Flag, &sys.Contact_Overload_A_Flag);
    if ((first_trigger_a != 0) && (Fault_Error_Check(ERR_OVERLOAD1) == 0))
    {
        Speak_Con(4); //20260729-11 change 触点过载仅更新运行故障，不再写参数区中的故障历史
    }

    if (sys.Contact_Overload_A_Flag != 0)
    {
        DIS_Fault(ERR_OVERLOAD1);
        Fault_Protect_PA();       // 锁存期间持续禁止 A 泵重新启动，不影响 B 泵故障状态
        Fault_Error_Set(ERR_OVERLOAD1);
    }

#if (Model_OUT == Model_1C2)
    first_trigger_b = Contact_Overload_Trigger(sys.Flag_OverFlow, &sys.Contact_Overload_B_Flag);
    if ((first_trigger_b != 0) && (Fault_Error_Check(ERR_OVERLOAD2) == 0))
    {
        Speak_Con(4); //20260729-11 change 触点过载仅更新运行故障，不再写参数区中的故障历史
    }

    if (sys.Contact_Overload_B_Flag != 0)
    {
        DIS_Fault(ERR_OVERLOAD2);
        Fault_Protect_PB();       // 锁存期间持续禁止 B 泵重新启动，不影响 A 泵故障状态
        Fault_Error_Set(ERR_OVERLOAD2);
    }
#endif
}
#if (Model_OUT == Model_1C2)
void Set_Pump2_OverLoad_Err(void)
{
    DIS_Fault(ERR_OVERLOAD2);
    Fault_Protect_PB();
    Fault_Error_Set(ERR_OVERLOAD2);
    Write_Fault_Mess(ERR_OVERLOAD2, sys.Voltage_Line, sys.avgCurrent2);
    Speak_Con(4);
}
#endif

#if (Model_Voltage == Model_3PH)
u8 Check_LosePhase1(void)
{
    u8 LP_Flag = 0;

    if ((sys.Current1_Max > 5) && (sys.Current1_Max >= (sys.Current1_Min * 3)))
    {
        LP_Flag = sys.Current1_Min_Index;
    }

    return LP_Flag;
}

#if (Model_OUT == Model_1C2)
u8 Check_LosePhase2(void)
{

    u8 LP_Flag = 0;

    if ((sys.Current2_Max > 5) && (sys.Current2_Max >= (sys.Current2_Min * 3)))
    {
        LP_Flag = sys.Current2_Min_Index;
    }

    return LP_Flag;
}
#endif
#endif

u8 Check_UnLoad1(void)
{
    u8 UL_Flag = 0;

    // three-phase current is lower than the configured threshold
    if (sys.avgCurrent1 <= config.Set_STANDBY_Current1 && config.Set_STANDBY_Current1 != 0)
    {
        UL_Flag = 1;
    }

    return UL_Flag;
}
#if (Model_OUT == Model_1C2)
u8 Check_UnLoad2(void)
{
    u8 UL_Flag = 0;

    // three-phase current is lower than the configured threshold
    if (sys.avgCurrent2 <= config.Set_STANDBY_Current2 && config.Set_STANDBY_Current2 != 0)
    {
        UL_Flag = 1;
    }

    return UL_Flag;
}
#endif
u8 Check_UVP(void)
{
    // three-phase current is lower than the configured threshold
    if (sys.Voltage_Line < config.UVP_Value)
        return 1;
    else
        return 0;
}

u8 Check_OVP(void)
{
    // three-phase current is lower than the configured threshold
    if (sys.Voltage_Line > config.OVP_Value)
        return 1;
    else
        return 0;
}

void LosePhase_Check(void)
{
#if (Model_Voltage == Model_3PH)
    u8 temp;
    Pump_Current_Check();
    // loss-phase protection
    if (sys.Pump1_Startup_Counter >= Fault_LOSEPHASE_Tdelay && (config.fault_mask & 0x05) == 0)
    {
        temp = Check_LosePhase1();

        if (temp > 0)
        {
            sys.pump_a_loss_cnt++;
            sys.pump_a_loss_delay = 30;
        }
        switch (temp)
        {
        case 1:

            DIS_Fault(ERR_LOSE_A1);
            Fault_Protect_PA();
            Fault_Error_Set(ERR_LOSE_A1);
            Write_Fault_Mess(ERR_LOSE_A1, sys.Voltage_Line, sys.Current1_Min);
            Speak_Con(4);

            break;
        case 2:

            DIS_Fault(ERR_LOSE_B1);
            Fault_Protect_PA();
            Fault_Error_Set(ERR_LOSE_B1);
            Write_Fault_Mess(ERR_LOSE_B1, sys.Voltage_Line, sys.Current1_Min);
            Speak_Con(4);

            break;

        case 3:

            DIS_Fault(ERR_LOSE_C1);
            Fault_Protect_PA();
            Fault_Error_Set(ERR_LOSE_C1);
            Write_Fault_Mess(ERR_LOSE_C1, sys.Voltage_Line, sys.Current1_Min);
            Speak_Con(4);

            break;

        default:

            break;
        }
    }

//    if(sys.Pump1_Startup_Counter>=Fault_NOLOAD_Tdelay)
//    {
//        if(sys.Current1_Max <= Fault_NOLOAD_LIMET)
//        {
//            DIS_Fault(ERR_NOLOAD_A);
//            Fault_Protect_PA();
//            Speak_Con(4);
//            Fault_Error_Set(ERR_NOLOAD_A);
//        }

//    }
#if (Model_OUT == Model_1C2)
    if (sys.Pump2_Startup_Counter >= Fault_LOSEPHASE_Tdelay && (config.fault_mask & 0x06) == 0)
    {
        temp = Check_LosePhase2();

        if (temp > 0)
        {
            sys.pump_b_loss_cnt++;
            sys.pump_b_loss_delay = 30;
        }

        switch (temp)
        {
        case 1:

            DIS_Fault(ERR_LOSE_A2);
            Fault_Protect_PB();
            Fault_Error_Set(ERR_LOSE_A2);
            Write_Fault_Mess(ERR_LOSE_A2, sys.Voltage_Line, sys.Current2_Min);
            Speak_Con(4);

            break;
        case 2:

            DIS_Fault(ERR_LOSE_B2);
            Fault_Protect_PB();
            Fault_Error_Set(ERR_LOSE_B2);
            Write_Fault_Mess(ERR_LOSE_B2, sys.Voltage_Line, sys.Current2_Min);
            Speak_Con(4);

            break;

        case 3:

            DIS_Fault(ERR_LOSE_C2);
            Fault_Protect_PB();
            Fault_Error_Set(ERR_LOSE_C2);
            Write_Fault_Mess(ERR_LOSE_C2, sys.Voltage_Line, sys.Current2_Min);
            Speak_Con(4);

            break;

        default:

            break;
        }
    }

//    if(sys.Pump2_Startup_Counter>=Fault_NOLOAD_Tdelay)
//    {
//        if(sys.Current2_Max <= Fault_NOLOAD_LIMET)
//        {
//            DIS_Fault(ERR_NOLOAD_B);
//            Fault_Protect_PB();
//            Speak_Con(4);
//            Fault_Error_Set(ERR_NOLOAD_B);
//        }

//    }
#endif
#endif
}

// fault handling
void Fault_Control(void)
{

    static u8 Timer_OverLoad_Cnt1 = 0;
    static u8 Timer_OverLoad_Cnt2 = 0;
    static u8 Timer_OverLoad_Cnt3 = 0;
    static u8 Timer_OverLoad_Cnt4 = 0;
    static u8 Timer_OverLoad_Cnt5 = 0;
    static u8 Unload_Cnt1 = 0;
#if (Model_OUT == Model_1C2)
    static u8 Timer_OverLoad_Cnt6 = 0;
    static u8 Timer_OverLoad_Cnt7 = 0;
    static u8 Timer_OverLoad_Cnt8 = 0;
    static u8 Timer_OverLoad_Cnt9 = 0;
    static u8 Timer_OverLoad_Cnt10 = 0;
    static u8 Unload_Cnt2 = 0;
#endif

    static u8 UVP_Cnt = 0;
    static u8 OVP_Cnt = 0;
#if (Model_Voltage == Model_3PH)
    if (config.Lock1)
    {
        Fault_Protect_PA();
        Fault_Error_Set(ERR_LOCK_A);
    }
#if (Model_OUT == Model_1C2)
    if (config.Lock2)
    {
        Fault_Protect_PB();
        Fault_Error_Set(ERR_LOCK_B);
    }
#endif
#endif

    // unload protection 1
    if (sys.Pump1_Startup_Counter >= 1 && (config.fault_mask & 0x04) == 0) // startup completed
    {

        if (Check_UnLoad1() == 1)
        {
            Unload_Cnt1++;
            if (Unload_Cnt1 >= Fault_UNLOAD_Time)
            {
                Unload_Cnt1 = 0;
                DIS_Fault(ERR_STANDBY1);
                Fault_Protect_PA();
                Fault_Error_Set(ERR_STANDBY1);
                Write_Fault_Mess(ERR_STANDBY1, sys.Voltage_Line, sys.avgCurrent1);
                Speak_Con(4);
            }
        }
        else
        {
            Unload_Cnt1 = 0;
        }
    }
    else
    {

        Unload_Cnt1 = 0;
    }

#if (Model_OUT == Model_1C2)
    // unload protection 2
    if (sys.Pump2_Startup_Counter >= 1 && (config.fault_mask & 0x04) == 0) // startup completed
    {
        if (Check_UnLoad2() == 1)
        {
            Unload_Cnt2++;

            if (Unload_Cnt2 >= Fault_UNLOAD_Time)
            {
                Unload_Cnt2 = 0;
                DIS_Fault(ERR_STANDBY2);
                Fault_Protect_PB();
                Fault_Error_Set(ERR_STANDBY2);
                Write_Fault_Mess(ERR_STANDBY2, sys.Voltage_Line, sys.avgCurrent2);
                Speak_Con(4);
            }
        }
        else
        {
            Unload_Cnt2 = 0;
        }
    }
    else
    {

        Unload_Cnt2 = 0;
    }
#endif

    // overload protection 1
    if (sys.Pump1_Startup_Counter >= 1 && (config.fault_mask & 0x04) == 0) // startup completed
    {

        if (sys.avgCurrent1 >= config.Set_OVERLOAD_Current1 * 12 / 10)
        {
            Timer_OverLoad_Cnt1++;
            if (Timer_OverLoad_Cnt1 >= 30)
            {
                Timer_OverLoad_Cnt1 = 0;
                Set_Pump1_OverLoad_Err();
            }
        }
        else
        {
            Timer_OverLoad_Cnt1 = 0;
        }

        if (sys.avgCurrent1 >= config.Set_OVERLOAD_Current1 * 15 / 10)
        {
            Timer_OverLoad_Cnt2++;
            if (Timer_OverLoad_Cnt2 >= 5)
            {
                Timer_OverLoad_Cnt2 = 0;
                Set_Pump1_OverLoad_Err();
            }
        }
        else
        {
            Timer_OverLoad_Cnt2 = 0;
        }

        if (sys.avgCurrent1 >= config.Set_OVERLOAD_Current1 * 2)
        {
            Timer_OverLoad_Cnt3++;
            if (Timer_OverLoad_Cnt3 >= 1)
            {
                Timer_OverLoad_Cnt3 = 0;

                Set_Pump1_OverLoad_Err();
            }
        }
        else
        {
            Timer_OverLoad_Cnt3 = 0;
        }

        if (sys.avgCurrent1 >= config.Set_OVERLOAD_Current1 * 3)
        {
            Timer_OverLoad_Cnt4++;
            if (Timer_OverLoad_Cnt4 >= 1)
            {
                Timer_OverLoad_Cnt4 = 0;
                Set_Pump1_OverLoad_Err();
            }
        }
        else
        {
            Timer_OverLoad_Cnt4 = 0;
        }

        // if (sys.avgCurrent1 >= config.Set_OVERLOAD_Current1)
        // {
        //     Timer_OverLoad_Cnt5++;
        //     if (Timer_OverLoad_Cnt5 >= 60)
        //     {
        //         Timer_OverLoad_Cnt5 = 0;
        //         Set_Pump1_OverLoad_Err();
        //     }
        // }
        // else
        // {
        //     Timer_OverLoad_Cnt5 = 0;
        // }
    }
    else
    {

        Timer_OverLoad_Cnt1 = 0;
        Timer_OverLoad_Cnt2 = 0;
        Timer_OverLoad_Cnt3 = 0;
        Timer_OverLoad_Cnt4 = 0;
        Timer_OverLoad_Cnt5 = 0;
    }
#if (Model_OUT == Model_1C2)
    // overload protection 2
    if (sys.Pump2_Startup_Counter >= 1 && (config.fault_mask & 0x04) == 0) // startup completed
    {

        if (sys.avgCurrent2 >= config.Set_OVERLOAD_Current2 * 12 / 10)
        {
            Timer_OverLoad_Cnt6++;
            if (Timer_OverLoad_Cnt6 >= 30)
            {
                Timer_OverLoad_Cnt6 = 0;
                Set_Pump2_OverLoad_Err();
            }
        }
        else
        {
            Timer_OverLoad_Cnt6 = 0;
        }

        if (sys.avgCurrent2 >= config.Set_OVERLOAD_Current2 * 15 / 10)
        {
            Timer_OverLoad_Cnt7++;
            if (Timer_OverLoad_Cnt7 >= 5)
            {
                Timer_OverLoad_Cnt7 = 0;
                Set_Pump2_OverLoad_Err();
            }
        }
        else
        {
            Timer_OverLoad_Cnt7 = 0;
        }

        if (sys.avgCurrent2 >= config.Set_OVERLOAD_Current2 * 2)
        {
            Timer_OverLoad_Cnt8++;
            if (Timer_OverLoad_Cnt8 >= 1)
            {
                Timer_OverLoad_Cnt8 = 0;
                Set_Pump2_OverLoad_Err();
            }
        }
        else
        {
            Timer_OverLoad_Cnt8 = 0;
        }

        if (sys.avgCurrent2 >= config.Set_OVERLOAD_Current2 * 3)
        {
            Timer_OverLoad_Cnt9++;
            if (Timer_OverLoad_Cnt9 >= 1)
            {
                Timer_OverLoad_Cnt9 = 0;
                Set_Pump2_OverLoad_Err();
            }
        }
        else
        {
            Timer_OverLoad_Cnt9 = 0;
        }

        // if (sys.avgCurrent2 >= config.Set_OVERLOAD_Current2)
        // {
        //     Timer_OverLoad_Cnt10++;
        //     if (Timer_OverLoad_Cnt10 >= 60)
        //     {
        //         Timer_OverLoad_Cnt10 = 0;
        //         Set_Pump2_OverLoad_Err();
        //     }
        // }
        // else
        // {
        //     Timer_OverLoad_Cnt10 = 0;
        // }
    }
    else
    {

        Timer_OverLoad_Cnt6 = 0;
        Timer_OverLoad_Cnt7 = 0;
        Timer_OverLoad_Cnt8 = 0;
        Timer_OverLoad_Cnt9 = 0;
        Timer_OverLoad_Cnt10 = 0;
    }

#endif

    // under-voltage protection
    if (sys.Timer_Startup_Counter >= 1) // startup completed
    {
        if (Check_UVP())
        {

            UVP_Cnt++;

            if (UVP_Cnt >= Fault_UVP_Time)
            {
                UVP_Cnt = 0;
                DIS_Fault(ERR_UVP);
                Fault_Protect();
                Fault_Error_Set(ERR_UVP);
                Write_Fault_Mess(ERR_UVP, sys.Voltage_Line, sys.avgCurrent1);
                Speak_Con(4);
            }
        }
        else
        {
            UVP_Cnt = 0;
        }
    }
    else
    {

        UVP_Cnt = 0;
    }

    // over-voltage protection
    if (sys.Timer_Startup_Counter >= 1) // startup completed
    {
        if (Check_OVP())
        {
            OVP_Cnt++;

            if (OVP_Cnt >= Fault_UVP_Time)
            {
                OVP_Cnt = 0;
                DIS_Fault(ERR_OVP);
                Fault_Protect();
                Fault_Error_Set(ERR_OVP);
                Write_Fault_Mess(ERR_OVP, sys.Voltage_Line, sys.avgCurrent1);
                Speak_Con(4);
            }
        }
        else
        {
            OVP_Cnt = 0;
        }
    }
    else
    {

        OVP_Cnt = 0;
    }

    Fault_ReStart();

    if (sys.Fault_A_Flag || sys.Fault_B_Flag)
    {

        sys.Faulte_out = 1;
    }
    else
    {
        sys.Faulte_out = 0;
    }
#if (Model_Voltage == Model_3PH)
    //
    if (sys.pump_a_loss_delay)
    {
        sys.pump_a_loss_delay--;
    }
    else
    {
        sys.pump_a_loss_cnt = 0;
    }

    if (sys.pump_b_loss_delay)
    {
        sys.pump_b_loss_delay--;
    }
    else
    {
        sys.pump_b_loss_cnt = 0;
    }

    if (sys.pump_a_loss_cnt >= 3)
    {
        config.Lock1 = 1;
        sys.save_flag = 1;
    }

    if (sys.pump_b_loss_cnt >= 3)
    {
        config.Lock2 = 1;
        sys.save_flag = 1;
    }
#endif
}

void ReStart_Clear_Fault(void)
{

    if (check_A_Fault() == 0)
    {
        sys.Fault_A_Flag = 0;
    }
    if (check_B_Fault() == 0)
    {
        sys.Fault_B_Flag = 0;
    }

    if ((sys.Fault_A_Flag == 0) && (sys.Fault_B_Flag == 0))
    {
        Speak_Stop();
    }
}

void Fault_ReStart(void)
{
    static u16 Timer_Standby_Cnt1 = 0;  // fault recovery timer counter
    static u16 Timer_Overload_Cnt1 = 0; // fault recovery timer counter
    static u16 Timer_Standby_Cnt2 = 0;  // fault recovery timer counter
    static u16 Timer_Overload_Cnt2 = 0; // fault recovery timer counter
    static u16 Timer1 = 0;              // fault recovery timer counter

    // fault recovery
    if (Fault_Error_Check(ERR_UVP) || Fault_Error_Check(ERR_OVP))
    {
        if (config.Timer_OUV_Restart != 0)
        {
            if (Timer1 >= Fault_UVP_Recovery_Time)
            {

                if (Fault_Error_Check(ERR_UVP))
                {
                    if (Check_UVP() == 0)
                    {
                        Fault_Error_Clear(ERR_UVP);
                        ReStart_Clear_Fault();
                        Timer1 = 0;
                    }
                }
                else if (Fault_Error_Check(ERR_OVP))
                {
                    if (Check_OVP() == 0)
                    {
                        Fault_Error_Clear(ERR_OVP);
                        ReStart_Clear_Fault();
                        Timer1 = 0;
                    }
                }
            }
            else
            {
                Timer1++;
            }
        }
    }
    else
    {

        Timer1 = 0;
    }

    // unload fault recovery
    if (Fault_Error_Check(ERR_STANDBY1))
    {
        if (config.Timer_Standby_Restart != 0)
        {

            if (Timer_Standby_Cnt1 >= config.Timer_Standby_Restart * 60)
            {
                Timer_Standby_Cnt1 = 0;

                Fault_Error_Clear(ERR_STANDBY1);
                ReStart_Clear_Fault();
            }
            else
            {
                Timer_Standby_Cnt1++;
            }
        }
    }
    else
    {

        Timer_Standby_Cnt1 = 0;
    }

    if (Fault_Error_Check(ERR_OVERLOAD1))
    {
        if (sys.Contact_Overload_A_Flag != 0) //20260729-6 change 所有机型的 A 泵触点过载均禁止定时自动恢复
        {
            Timer_Overload_Cnt1 = 0;
        }
        else
        {
            if (config.Timer_Unload_Restart != 0)
            {
                if (Timer_Overload_Cnt1 >= config.Timer_Unload_Restart * 60)
                {
                    Timer_Overload_Cnt1 = 0;
                    Fault_Error_Clear(ERR_OVERLOAD1);
                    ReStart_Clear_Fault();
                }
                else
                {
                    Timer_Overload_Cnt1++;
                }
            }
        }
    }
    else
    {

        Timer_Overload_Cnt1 = 0;
    }

    if (Fault_Error_Check(ERR_STANDBY2))
    {
        if (config.Timer_Standby_Restart != 0)
        {

            if (Timer_Standby_Cnt2 >= config.Timer_Standby_Restart * 60)
            {
                Timer_Standby_Cnt2 = 0;

                Fault_Error_Clear(ERR_STANDBY2);
                ReStart_Clear_Fault();
            }
            else
            {
                Timer_Standby_Cnt2++;
            }
        }
    }
    else
    {

        Timer_Standby_Cnt2 = 0;
    }

    if (Fault_Error_Check(ERR_OVERLOAD2))
    {
#if (Model_OUT == Model_1C2)
        if (sys.Contact_Overload_B_Flag != 0) //20260729-7 change B 泵触点过载禁止定时自动恢复
        {
            Timer_Overload_Cnt2 = 0;
        }
        else
#endif
        {
            if (config.Timer_Unload_Restart != 0)
            {

                if (Timer_Overload_Cnt2 >= config.Timer_Unload_Restart * 60)
                {
                    Timer_Overload_Cnt2 = 0;

                    Fault_Error_Clear(ERR_STANDBY2);
                    Fault_Error_Clear(ERR_OVERLOAD2);
                    ReStart_Clear_Fault();
                }
                else
                {
                    Timer_Overload_Cnt2++;
                }
            }
        }
    }
    else
    {

        Timer_Overload_Cnt2 = 0;
    }
}

void zhengdingA(void)
{
    if (sys.Pump_A_state)
    {
        if ((sys.avgCurrent1 + 10 <= Limit_Current) && (sys.avgCurrent1 > 2))
        {
            config.Set_STANDBY_Current1 = sys.avgCurrent1 * 65 / 100;
            config.Set_OVERLOAD_Current1 = sys.avgCurrent1 * 13 / 10;
            sys.save_flag = 1;
            Speak_Con(2);
            config.A_Turn = 1;
        }
    }
}

void Clear_zhengdingA(void)
{

    config.Set_STANDBY_Current1 = 0;
    switch (config.Dev_type)
    {

    case 1:

        config.Set_OVERLOAD_Current1 = V2_Set_Current;

        break;
    case 2:

        config.Set_OVERLOAD_Current1 = V3_Set_Current;

        break;
    case 3:

        config.Set_OVERLOAD_Current1 = V4_Set_Current;
    case 4:

        config.Set_OVERLOAD_Current1 = V5_Set_Current;

        break;
    default:

        config.Set_OVERLOAD_Current1 = V1_Set_Current;

        config.Dev_type = 0;
        break;
    }
    sys.save_flag = 1;
    Speak_Con(2);
    config.A_Turn = 0;
}

void zhengdingB(void)
{
    if (sys.Pump_B_state)
    {
        if ((sys.avgCurrent2 + 10 <= Limit_Current) && (sys.avgCurrent2 > 2))
        {
            config.Set_STANDBY_Current2 = sys.avgCurrent2 * 65 / 100;
            config.Set_OVERLOAD_Current2 = sys.avgCurrent2 * 13 / 10;
            sys.save_flag = 1;
            Speak_Con(2);
            config.B_Turn = 1;
        }
    }
}

void Clear_zhengdingB(void)
{

    config.Set_STANDBY_Current2 = 0;
    switch (config.Dev_type)
    {

    case 1:

        config.Set_OVERLOAD_Current2 = V2_Set_Current;

        break;
    case 2:

        config.Set_OVERLOAD_Current2 = V3_Set_Current;

        break;
    case 3:

        config.Set_OVERLOAD_Current2 = V4_Set_Current;

        break;

    case 4:

        config.Set_OVERLOAD_Current2 = V5_Set_Current;

        break;
    default:

        config.Set_OVERLOAD_Current2 = V1_Set_Current;

        config.Dev_type = 0;
        break;
    }
    sys.save_flag = 1;
    Speak_Con(2);
    config.B_Turn = 0;
}

void autoZhending(void)
{

    if (config.A_Turn == 0)
    {
        if (sys.Pump_A_state)
        {
            if (++sys.autoTurnA_cnt > 150)
            {

                zhengdingA();
            }
        }
        else
        {

            sys.autoTurnA_cnt = 0;
        }
    }

    if (config.B_Turn == 0)
    {
        if (sys.Pump_B_state)
        {
            if (++sys.autoTurnB_cnt > 150)
            {

                zhengdingB();
            }
        }
        else
        {

            sys.autoTurnB_cnt = 0;
        }
    }
}
