#include "ad.h"
#include "system.h"

//========================================================================
//                               本地常量声明
//========================================================================
#define ADC_START 0x40 // ADC 转换启动控制位。自动清
#define ADC_NUMB 8
#define Adc_Filer 60
#define Adc_Filer_cnt 20
code u8 ADC_Chanle[ADC_NUMB] = {1, 0, 14, 13, 11, 10, 9, 8};
//========================================================================
//                               本地变量声明
//========================================================================
u8 index = 0;

u32 Sum_AD[ADC_NUMB] = {0, 0, 0, 0, 0, 0, 0, 0};
u16 Result_AD[ADC_NUMB] = {0, 0, 0, 0, 0, 0, 0, 0};

s16 ADC[ADC_NUMB + 1] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
u8 ADC_Cnt[ADC_NUMB + 1] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

u8 Vol_ADC_cnt = 0;
u16 Vol_ADC_MAX = 0;
u16 Vol_ADC = 0;
u32 Sum_Vol = 0;
u8 Sum_Vol_cnt = 0;
u8 Sam_Counter = 0;

union
{
    u8 all;
    struct
    {
        u8 complete : 1;
        u8 finish : 1;
        u8 ch_flag : 2;
        u8 timeOut : 1;
        u8 processed : 1;

    } bits;

} Measure_flag;

MEASURE_T Measure;

//========================================================================
//                               本地函数声明
//========================================================================

void Read_ADC_Chan(u8 channel);
//========================================================================
//                            外部函数和变量声明
//========================================================================
void Filter(u8 Ch, s16 Adc)
{
    if ((ADC[Ch] - Adc > Adc_Filer) || (Adc - ADC[Ch] > Adc_Filer))
    {

        if (++ADC_Cnt[Ch] > Adc_Filer_cnt)
        {
            ADC_Cnt[Ch] = 0;
            ADC[Ch] = Adc;
        }
    }
    else
    {
        ADC_Cnt[Ch] = 0;
    }
    if (Adc < Adc_Filer)
    {
        ADC[Ch] = 0;
    }
}

//========================================================================
// 函数: ADtoUART_init
// 描述: 用户初始化程序.
// 参数: None.
// 返回: None.
// 版本: V1.0, 2020-09-28
//========================================================================
void MEASURE_init(void)
{
    ADC_CONTR = (ADC_CONTR & ~(3 << 5)) | 0;
    ADC_CONTR |= 0x80;
    ADCCFG = 0x2f;
    ADCTIM = 0x3f;
    EADC = 1;
    Measure_flag.bits.complete = 1;
    Measure_flag.bits.processed = 1;
}

//========================================================================
// 函数: Read_ADC_Chan
// 描述:.
// 参数: channel.
// 返回: None.
// 版本: V1.0, 2020-09-24
//========================================================================
void Read_ADC_Chan(u8 channel)
{
    ADC_CONTR = (ADC_CONTR & 0xf0) | channel;
    ADC_CONTR |= ADC_START;
}

//========================================================================
// 函数: Sample_ADtoUART
// 描述: ADC中断调用.
// 参数: None.
// 返回: None.
// 版本: V1.0, 2020-09-24
//========================================================================
void MEASURE_Sample(void)
{
    u16 AD_Temp;
    u32 u32Temp;
    u8 i;
    P55 = 1;

    AD_Temp = ((u16)ADC_RES << 8) | ADC_RESL;

    if (Measure_flag.bits.ch_flag == 0)
    {

        // ch7
        if (Vol_ADC_MAX < AD_Temp)
        {
            Vol_ADC_MAX = AD_Temp;
        }
        if (++Vol_ADC_cnt > 40)
        {
            Vol_ADC_cnt = 0;
            Filter(ADC_NUMB, Vol_ADC_MAX);
            Sum_Vol += ADC[ADC_NUMB];
            //			Sum_Vol += Vol_ADC_MAX;
            Vol_ADC_MAX = 0;
            Sum_Vol_cnt++;
        }
        Measure_flag.bits.ch_flag = 1;
        Read_ADC_Chan(ADC_Chanle[index]);
    }
    else
    {

        //		Filter(index,AD_Temp);
        //		Sum_AD[index] += ADC[index];
        Sum_AD[index] += AD_Temp;

        if (++index >= ADC_NUMB)
        {

            Measure_flag.bits.finish = 1;
            Measure_flag.bits.complete = 1;
            Measure_flag.bits.processed = 0;
            index = 0;
        }
        else
        {
            if (Measure_flag.bits.ch_flag == 1)
            {
                Measure_flag.bits.ch_flag = 2;
                Read_ADC_Chan(ADC_Chanle[index]);
            }
            else
            {
                Measure_flag.bits.complete = 1;
                Measure_flag.bits.processed = 0;
            }
        }
    }

    if (Measure_flag.bits.complete)
    {

        if (Measure_flag.bits.processed == 0)
        {
            Measure_flag.bits.processed = 1;

            if (Sum_Vol_cnt > 50)
            {
                Sum_Vol = Sum_Vol / Sum_Vol_cnt;
                Sum_Vol >>= 2;
                Sum_Vol_cnt = 0;
                sys.Voltage_Line = Sum_Vol * 98 / 100;
                Sum_Vol = 0;
            }
            if (Measure_flag.bits.finish)
            {

                if (++Sam_Counter >= 128)
                {

                    Sam_Counter = 0;

                    for (i = 0; i < ADC_NUMB; i++)
                    {
                        Result_AD[i] = Sum_AD[i] >> 9;
                        Sum_AD[i] = 0;
                        if (Result_AD[i] < 10)
                        {
                            Result_AD[i] = 0;
                        }
                    }

                    u32Temp = Result_AD[0];
                    sys.Current_A1 = (u32Temp * 72 + 50) / 100;
#if (Model_Voltage == Model_3PH)
                    u32Temp = Result_AD[1];
                    sys.Current_B1 = (u32Temp * 72 + 50) / 100;

                    u32Temp = Result_AD[2];
                    sys.Current_C1 = (u32Temp * 72 + 50) / 100;
#endif

                    //                    sys.Current_A1 = 60;
                    //                    sys.Current_B1 = 60;
                    //                    sys.Current_C1 = 60;

                    u32Temp = Result_AD[3];
                    sys.Current_A2 = (u32Temp * 72 + 50) / 100;
#if (Model_Voltage == Model_3PH)
                    u32Temp = Result_AD[4];
                    sys.Current_B2 = (u32Temp * 72 + 50) / 100;
                    u32Temp = Result_AD[5];
                    sys.Current_C2 = (u32Temp * 72 + 50) / 100;
#endif

                    //                    sys.Current_A2 = 60;
                    //                    sys.Current_B2 = 60;
                    //                    sys.Current_C2 = 60;

                    sys.Cur_AD = Result_AD[6];
                    sys.Vol_AD = Result_AD[7];
#if (Model_Voltage == Model_3PH)
                    u32Temp = sys.Current_A1 + sys.Current_B1 + sys.Current_C1;

                    sys.avgCurrent1 = u32Temp / 3;

                    u32Temp = sys.Current_A2 + sys.Current_B2 + sys.Current_C2;

                    sys.avgCurrent2 = u32Temp / 3;
#else
                    sys.avgCurrent1 = sys.Current_A1;
                    sys.avgCurrent2 = sys.Current_A2;
#endif
                }
            }
        }
    }

    P55 = 0;
}

//========================================================================
// 函数:
// 描述: 用户应用程序.
// 参数: None.
// 返回: None.
// 版本: V1.0, 2020-09-24
//========================================================================
void MEASURE_Pro(void)
{
    if (Measure_flag.bits.complete && Measure_flag.bits.timeOut)
    {
        Measure_flag.all = 0;
        Read_ADC_Chan(7);
        //        P54 = 1;
    }
}

//========================================================================
// 函数:
// 描述: 用户应用程序.
// 参数: None.
// 返回: None.
// 版本: V1.0, 2020-09-24
//========================================================================
void MEASURE_timeOut(void)
{
    Measure_flag.bits.timeOut = 1;
}

void ADC_IRQHandler(void) interrupt ADC_VECTOR
{
    ADC_CONTR &= ~0x20;
    MEASURE_Sample();
}
