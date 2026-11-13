#include "system.h"
#include "ad.h"
//20261113-0 del 封存旧串口接收超时接口引用：#include "uart4.h"

#define	Timer0_Reload	(MAIN_Fosc / 1000)		//Timer 0 中断频率, 1000HZ
#define	Timer1_Reload	(MAIN_Fosc / 10000)		//Timer 0 中断频率, 10000HZ


bit Scan_Flag = 0;
bit Dis_Flag = 0;
bit Timer_1s_Flag = 0;
bit Timer_1ms_Flag = 0;

extern void RF_Decord(void);

void Time0_init(void)
{
    TR0 = 0;
    TMOD &= ~0x03;
    TMOD |= 0;	//工作模式, 0: 16位自动重装, 1: 16位定时/计数, 2: 8位自动重装, 3: 16位自动重装, 不可屏蔽中断
    //	TMOD |=  0x04;	//对外计数或分频
    TMOD &= ~0x04;	//定时
    AUXR |=  0x80;	//1T mode
    TH0 = (u8)((65536UL - Timer0_Reload) / 256);
    TL0 = (u8)((65536UL - Timer0_Reload) % 256);

    TR0 = 1;	//开始运行
    ET0 = 1;

}

void Time1_init(void)
{
    TR1 = 0;
    PT1 = 1;
    TMOD = (TMOD & ~0x30) | 0;
    TMOD &= ~0x04;	//定时
    AUXR |=  0x40;	//1T mode
    TH1 = (u8)((65536UL - Timer1_Reload) / 256);
    TL1 = (u8)((65536UL - Timer1_Reload) % 256);

    TR1 = 1;	//开始运行
    ET1 = 1;
}





extern void Modbus_Timer_handle(void);


void TIM0_IRQHandler (void) interrupt TIMER0_VECTOR
{
    static u16 Timer_dis3_Cnt = 0;
	static u16 Timer_dis2_Cnt = 0;
	static u16 Timer_1S = 0;
	P55 = 1;
    Timer_1ms_Flag = 1;
	Water_Level();

#if (Function_RS485 == 1)
    Modbus_Timer_handle();
#endif
	
	MEASURE_timeOut();
	MEASURE_Pro();	

    Timer_dis3_Cnt++;
	//20261113-0 del 停用旧协议的接收超时分帧：UART4_RX_CNT();
    if(Timer_dis3_Cnt>=10)
    {
        Timer_dis3_Cnt = 0;
        Scan_Flag = 1;
        Timer_dis2_Cnt++;
        if(Timer_dis2_Cnt>=10)
        {
            Timer_dis2_Cnt = 0;
            Dis_Flag = 1;
				
            if(++Timer_1S>=10)
            {
                Timer_1S = 0;
                Timer_1s_Flag = 1;

            }
        }
    }
	P55 = 0;
}


//10K
void TIM1_IRQHandler (void) interrupt TIMER1_VECTOR
{
#if(Function_RF==1)
    //RF接收
    RF_Decord();
#endif
}












