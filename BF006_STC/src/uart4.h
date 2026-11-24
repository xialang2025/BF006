#ifndef __UART_H
#define __UART_H	 

#include	"config.h"

#define BF4G_FRAME_MAX 97 //20261116-0 add 串口四共享帧的最大字节数。

void uart_init(void);
void uart4_service(void);
void uart4_fault_start(u32 boot); //20261124-0 add 由已可靠取得编号的启动路径启用上报。
void uart4_fault_scanned(bit one_second); //20261124-0 add 在相应保护组结束后通知。
bit uart4_fault_awaiting_ack(void); //20261124-0 add 查询原帧的等待确认状态。
#define BF4G_FAULT_FRAME_LENGTH 24 //20261124-0 add 主动故障帧完整长度。
bit bf4g_fault_baseline(u8 xdata *frame, u32 boot); //20261124-0 add 编码有效的启动基线。
bit bf4g_fault_ack_matches(const u8 xdata *frame, u8 length, const u8 xdata *event); //20261124-0 add 校验主动确认。
void uart4_control_scanned(void); //20261117-7 add
bit uart4_sending(void);
bit uart4_blocks_save(void); //20261117-7 add
#define BF4G_REPLY_PENDING 0xff //20261117-7 add 受理后保留请求，等待控制扫描。
u8 bf4g_reply(u8 xdata *frame, u8 length);
u8 bf4g_control_result(u8 xdata *frame, bit timed_out); //20261117-7 add

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