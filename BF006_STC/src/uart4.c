#include "uart4.h"
#include "boot_identity.h" //20261114-21 add
//20261113-0 del 封存旧串口状态清零所用头文件：#include "string.h"

#if (Model_OUT == Model_1C1) && !defined(UART4_ECHO_TEST) //20261114-3 change 回显工程仅使用串口初始化
#define UART4_IDLE_TICKS 11 // 跨越十个完整毫秒后判定静默
#define UART4_RX 0
#define UART4_READY 1
#define UART4_OWNED 2
#define UART4_TX 3
#define UART4_DROP 4

u8 xdata uart4_frame[UART4_FRAME_CAPACITY]; //20261114-2 add 收帧、组包及发送依次使用
static volatile u8 uart4_state;
static volatile u8 uart4_idle;
static volatile u8 uart4_count;
static u8 uart4_tx_next;
#endif

#define	BaudRate4		9600UL	//20261113-1 change 串口四使用九千六百波特，八位数据、无校验、一位停止位
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
#if (Model_OUT == Model_1C1) && !defined(UART4_ECHO_TEST)
    uart4_state = UART4_RX; //20261114-2 add 仅在开放串口中断前初始化所有权
    uart4_count = 0;
    uart4_idle = 0;
#endif
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

#if (Model_OUT == Model_1C1) && !defined(UART4_ECHO_TEST)
/******************************************************************************
 * 函数名称       : UART4_Tick
 * 功能描述       : 在一毫秒中断中结束收帧或解除丢弃状态。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void UART4_Tick(void) //20261114-2 add
{
    if (RI4 || uart4_idle == 0 || --uart4_idle != 0) { // 接收中断待处理时仍有数据到达
        return;
    }

    if (uart4_state == UART4_RX) {
        uart4_state = UART4_READY;
    } else if (uart4_state == UART4_DROP) {
        uart4_count = 0;
        uart4_state = UART4_RX;
    }
}

/******************************************************************************
 * 函数名称       : UART4_Claim
 * 功能描述       : 将已分帧的缓冲交给主循环。
 * 输入参数       : None
 * 返回值         : 已收帧长度，零表示无可处理帧
 * 日期            : 2026.11.14
 ******************************************************************************/
u8 UART4_Claim(void) //20261114-2 add
{
    if (uart4_state != UART4_READY) {
        return 0;
    }
    uart4_state = UART4_OWNED; // 接收与计时中断均不改动就绪状态和帧内容
    return uart4_count;
}

#if BF006_BOARD_CONTEXT || defined(BF006_CONTEXT_SIMULATION)
/******************************************************************************
 * 函数名称       : UART4_Claim_Idle
 * 功能描述       : 串口空闲时暂借共享帧保存操作证据。
 * 输入参数       : None
 * 返回值         : 取得所有权返回一
 * 日期            : 2026.11.14
 ******************************************************************************/
u8 UART4_Claim_Idle(void) //20261114-21 add
{
    bit interrupt_enabled;
    u8 claimed;

    interrupt_enabled = EA;
    EA = 0;
    claimed = uart4_state == UART4_RX && uart4_count == 0 && !RI4;
    if (claimed) {
        uart4_state = UART4_OWNED;
    }
    EA = interrupt_enabled;
    return claimed;
}

#endif

/******************************************************************************
 * 函数名称       : UART4_Finish
 * 功能描述       : 放弃本帧或启动中断发送应答。
 * 输入参数       : length 应答长度，零表示静默结束
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void UART4_Finish(u8 length) //20261114-2 add
{
    bit interrupt_enabled;

    interrupt_enabled = EA;
    EA = 0;
    uart4_count = length;
    if (length != 0) {
        uart4_tx_next = 1;
        uart4_state = UART4_TX;
        CLR_TI4();
        S4BUF = uart4_frame[0]; // 首字节送出后，由发送中断推进
    } else if (uart4_idle != 0) {
        uart4_state = UART4_DROP;
    } else {
        uart4_state = UART4_RX;
    }
    EA = interrupt_enabled;
}

/******************************************************************************
 * 函数名称       : UART4_int
 * 功能描述       : 搬运收发字节并处理接收边界，一控二清除中断标志。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void UART4_int(void) interrupt UART4_VECTOR //20261114-2 change
{
    u8 received;

    if (RI4) {
        received = S4BUF;
        CLR_RI4();
        uart4_idle = UART4_IDLE_TICKS;
        if (uart4_state == UART4_RX) {
            if (uart4_count < UART4_REQUEST_LIMIT) {
                uart4_frame[uart4_count++] = received;
                if (uart4_count == 10 && uart4_frame[5] != 1 &&
                    (uart4_frame[8] != 0 || uart4_frame[9] > UART4_REQUEST_LIMIT - 12)) {
                    uart4_state = UART4_DROP; // 新格式声明长度超过最大请求
                }
            } else {
                uart4_state = UART4_DROP; // 收到第五十六字节，等待完整静默
            }
        }
    }

    if (TI4) {
        CLR_TI4();
        if (uart4_state == UART4_TX) {
            if (uart4_tx_next < uart4_count) {
                S4BUF = uart4_frame[uart4_tx_next++];
            } else {
                uart4_count = 0;
                if (uart4_idle != 0) {
                    uart4_state = UART4_DROP; // 发送期间来的字节已丢弃
                } else {
                    uart4_state = UART4_RX;
                }
            }
        }
    }
}
#else
void UART4_int(void) interrupt UART4_VECTOR
{
    if (RI4) {
        ACC = S4BUF; // 一控二继续读出并丢弃接收字节
        CLR_RI4();
    }

    if (TI4) {
        CLR_TI4();
    }
}
#endif
