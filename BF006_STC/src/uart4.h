#ifndef __UART_H
#define __UART_H	 

#include "system.h" //20261114-2 change 按机型开放快照串口接口

void uart_init(void); //20261113-0 change 保留串口四硬件初始化接口

#if (Model_OUT == Model_1C1) && !defined(UART4_ECHO_TEST) //20261114-3 change 独立回显不接入快照收发
#define UART4_FRAME_CAPACITY 127
#define UART4_REQUEST_LIMIT 55

extern u8 xdata uart4_frame[UART4_FRAME_CAPACITY]; //20261114-2 add 收发共享帧
void UART4_Tick(void);
u8 UART4_Claim(void);
void UART4_Finish(u8 length);
#endif

#if 0 //20261113-0 del 封存旧协议的串口缓冲区、计数和收发接口声明。
#define	COM_TX4_Lenth	512
#define	COM_RX4_Lenth	255
#define	TimeOutSet1		5

typedef struct
{ 
	u8	id;				//串口号

	u16	TX_read;		//发送读指针
	u16	TX_write;		//发送写指针
	u8	B_TX_busy;		//忙标志

	u16 RX_Cnt;			//接收字节计数
	u8	RX_TimeOut;		//接收超时
	u8	B_RX_OK;		//接收块完成
} COMx_Define; 

void TX1_write2buff(u8 dat);	//写入发送缓冲，指针+1
void PrintString1(u8 *puts);
void uart_init(void);
void UART4_RX_CNT(void);
u8 Getchar(char* vl);

extern	COMx_Define	COM4;
extern	u8	xdata TX4_Buffer[COM_TX4_Lenth];	//发送缓冲
extern	u8 	xdata RX4_Buffer[COM_RX4_Lenth];	//接收缓冲
#endif
#endif