#include "uart4.h"
//20261113-0 del 封存旧串口状态清零所用头文件：#include "string.h"

#define	BaudRate4		115200UL	//选择波特率
#define Timer4_Reload	(65536UL -(MAIN_Fosc / 4 / BaudRate4)) //20261113-0 change 定时器四重装值，溢出频率为波特率的四倍
#if 0 //20261113-0 del 释放旧串口的收发缓冲区、指针及接收超时状态。
#define	TimeOutSet4		5

COMx_Define	COM4;
u8	xdata TX4_Buffer[COM_TX4_Lenth];	//发送缓冲
u8 	xdata RX4_Buffer[COM_RX4_Lenth];	//接收缓冲
#endif

/******************************************************************************
 * 函数名称       : uart_init
 * 功能描述       : 配置串口四的引脚、定时器四波特率和串口中断。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.13
 ******************************************************************************/
void uart_init(void) //20261113-0 change
{
    S4_8bit();				//8位数据
    S4_USE_P52P53();
    S4_BRT_UseTimer4();
    Timer4_Stop();
    Timer4_AsTimer();
    Timer4_1T();
    T4_Load(Timer4_Reload);
    Timer4_InterruptDisable();
    Timer4_Run();
    S4_Int_Enable();	//允许中断
    S4_RX_Enable();	//允许接收
    //20261113-0 del 停用已封存的旧串口状态初始化：memset(&COM4,0,sizeof(COMx_Define));
}

#if 0 //20261113-0 del 封存旧串口收发队列、字符输出及超时分帧。
void TX4_write2buff(u8 dat)	//写入发送缓冲，指针+1
{
	u16 temp = COM4.TX_write;
	if(++temp >= COM_TX4_Lenth) temp = 0;
	
	while(temp == COM4.TX_read);
	
	TX4_Buffer[COM4.TX_write] = dat;	//装发送缓冲
	COM4.TX_write = temp;
	
    if(COM4.B_TX_busy == 0)		//空闲
    {
        COM4.B_TX_busy = 1;		//标志忙
        SET_TI4();					//触发发送中断
    }
}


char putchar(char ch)
{
    TX4_write2buff(ch);
    return ch;
}



void UART4_int (void) interrupt UART4_VECTOR
{
	if(RI4)
	{
		CLR_RI4();
		if(COM4.B_RX_OK == 0)
		{
			if(COM4.RX_Cnt >= COM_RX4_Lenth)	{COM4.B_RX_OK = 1; COM4.RX_Cnt = 0;}
			RX4_Buffer[COM4.RX_Cnt++] = S4BUF;
			COM4.RX_TimeOut = TimeOutSet4;
		}
	}

    if(TI4)
    {
        CLR_TI4();
        if(COM4.TX_read != COM4.TX_write)
        {
            S4BUF = TX4_Buffer[COM4.TX_read];
            if(++COM4.TX_read >= COM_TX4_Lenth)		COM4.TX_read = 0;
        }
        else	COM4.B_TX_busy = 0;
    }
}


void UART4_RX_CNT(void)
{
	if(COM4.RX_TimeOut > 0)
	{
		if(--COM4.RX_TimeOut == 0)
		{
			COM4.B_RX_OK = 1;
			COM4.RX_Cnt = 0;
		}

	}
}
#endif

/******************************************************************************
 * 函数名称       : UART4_int
 * 功能描述       : 在新协议接入前丢弃串口四收到的字节，并清除串口中断标志。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.13
 ******************************************************************************/
void UART4_int(void) interrupt UART4_VECTOR //20261113-0 add
{
    if (RI4) {
        ACC = S4BUF; // 通过累加器读取并丢弃接收字节，避免分配缓冲区
        CLR_RI4();   // 清除本次接收中断标志
    }

    if (TI4) {
        CLR_TI4();   // 清除发送中断标志，保持发送端空闲
    }
}
