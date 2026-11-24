#include "uart4.h"
//20261113-0 del 封存旧串口状态清零所用头文件：#include "string.h"

#include "configPar.h"

#define BaudRate4        9600UL //20261116-0 change 定时器四提供串口四的波特率。
#define Timer4_Reload    (65536UL - MAIN_Fosc / 4 / BaudRate4)
#define S4_QUIET_RELOAD  (65536UL - MAIN_Fosc / 12 / 100)
#define S4_TAIL_RELOAD   (65536UL - MAIN_Fosc / 12 / BaudRate4 * 2)

#define S4_CONTROL_TICKS 30 //20261117-7 add 每次十毫秒，最多等待三百毫秒。
#define S4_FAULT_WAIT_TICKS 5001 //20261124-2 add 避免首个节拍的相位使五秒窗口提前结束。
#define S4_FAULT_ATTEMPTS 11 //20261124-2 add 一次首发加十次重发。
#define S4_FAULT_STOP_MARK 0xa5 //20261124-2 add 在保留的运行内存中标记本次通电已停报。

// 定时器三用于帧间静默、控制等待和发送尾部计时。
#define S4_TIMER_ARM(reload) do { \
    Timer3_Stop(); \
    AUXINTIF &= ~T3IF; \
    T3_Load(reload); \
    Timer3_Run(); \
} while (0)

enum {
    S4_IDLE,
    S4_RECEIVE,
    S4_READY,
    S4_PROCESS,
    S4_SEND,
    S4_DRAIN,
    S4_DISCARD
};

static u8 xdata s4_frame[BF4G_FRAME_MAX]; //20261116-0 add 收发交替独占同一帧。
static volatile u8 s4_state;
static volatile u8 s4_count;
static volatile u8 s4_next;
static volatile u8 s4_control_ticks; //20261117-7 add 控制等待的剩余时段数。
static volatile bit s4_disturbed;

enum { //20261124-0 add 启动基线在采样、待发和等待确认之间移交。
    S4_FAULT_DISABLED,
    S4_FAULT_BASELINE,
    S4_FAULT_READY,
    S4_FAULT_IDLE, //20261124-3 add 已有基线，等待新的有效状态。
    S4_FAULT_WAIT
};

static u8 xdata s4_fault_frame[BF4G_FAULT_FRAME_LENGTH];
static u32 xdata s4_fault_boot;
#define S4_FAULT_CHANGED 1 //20261124-3 add 在途期间记录最新状态和合并情况。
#define S4_FAULT_RESYNC  2
#define S4_FAULT_INVALID 4
static u32 xdata s4_fault_latest;
static u32 xdata s4_fault_seq;
static u8 s4_fault_flags;
static u8 s4_fault_state;
static u8 s4_fault_scans;
static u8 s4_fault_sent; //20261124-2 add 只累计已经开始发送的次数。
static volatile u16 s4_fault_timeout;
static volatile bit s4_fault_tx;
static volatile bit s4_fault_due;
static u8 xdata s4_fault_stop_mark; // 启动汇编保留扩展运行内存，真实上电时显式清除。
#if 0 //20261113-0 del 释放旧串口的收发缓冲区、指针及接收超时状态。
#define	TimeOutSet4		5

COMx_Define	COM4;
u8	xdata TX4_Buffer[COM_TX4_Lenth];	//发送缓冲
u8 	xdata RX4_Buffer[COM_RX4_Lenth];	//接收缓冲
#endif

/******************************************************************************
 * 函数名称       : uart_init
 * 功能描述       : 配置串口四和两个独立的串口时基。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
void uart_init(void) //20261116-0 change
{
    S4CON = 0;                   // 八位数据、无校验、一位停止位。
    Timer3_Stop();
    Timer3_AsTimer();
    Timer3_12T();
    AUXINTIF &= ~T3IF;
    Timer3_InterruptEnable();
    s4_state = S4_IDLE;
    s4_disturbed = 0;
    s4_fault_state = S4_FAULT_DISABLED; //20261124-0 add 可信编号就绪之前保持上报停用。
    s4_fault_timeout = 0; //20261124-2 add 每次串口初始化均清除在途计时状态。
    s4_fault_tx = 0;
    s4_fault_due = 0;
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
 * 函数名称       : uart4_fault_cold_start
 * 功能描述       : 软件启动时仅在确认是新通电周期后解除停发。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.24
 ******************************************************************************/
void uart4_fault_cold_start(void) //20261124-2 add
{
    s4_fault_stop_mark = 0;
}

/******************************************************************************
 * 函数名称       : uart4_fault_tick
 * 功能描述       : 以定时器零的毫秒节拍更新主动帧确认窗口。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.24
 ******************************************************************************/
void uart4_fault_tick(void) //20261124-2 add
{
    if (s4_fault_timeout != 0 && --s4_fault_timeout == 0) {
        s4_fault_due = 1;
    }
}

/******************************************************************************
 * 函数名称       : uart4_fault_start
 * 功能描述       : 在取得可信启动编号后开启首帧采样。
 * 输入参数       : boot，已经可靠保存的启动编号，零表示保持停用。
 * 返回值         : None
 * 日期            : 2026.11.24
 ******************************************************************************/
void uart4_fault_start(u32 boot) //20261124-2 change
{
    s4_fault_boot = boot;
    s4_fault_scans = 0;
    s4_fault_flags = 0; //20261124-3 add 每次可信启动独立建立状态。
    s4_fault_sent = 0;
    if (boot != 0 && s4_fault_stop_mark != S4_FAULT_STOP_MARK) {
        s4_fault_state = S4_FAULT_BASELINE;
    }
}

/******************************************************************************
 * 函数名称       : uart4_queue_fault
 * 功能描述       : 冻结最新有效状态并开始该事件自己的发送计数。
 * 输入参数       : kind，主动事件类别。
 * 返回值         : None
 * 日期            : 2026.11.24
 ******************************************************************************/
static void uart4_queue_fault(u8 kind) //20261124-3 add
{
    bf4g_fault_encode(s4_fault_frame, s4_fault_boot, s4_fault_seq, kind, s4_fault_latest);
    s4_fault_flags = 0;                 // 后续采样只能更新下一条事件的状态。
    s4_fault_sent = 0;
    s4_fault_state = S4_FAULT_READY;
}

/******************************************************************************
 * 函数名称       : uart4_fault_scanned
 * 功能描述       : 在保护扫描后更新最新有效位图和已观察变化的序号。
 * 输入参数       : one_second，一表示一秒组，零表示百毫秒组。
 * 返回值         : None
 * 日期            : 2026.11.24
 ******************************************************************************/
void uart4_fault_scanned(bit one_second) //20261124-3 change
{
    u32 faults;

    if (s4_fault_state == S4_FAULT_DISABLED || !Boot_Flag) {
        return;
    }
    if (one_second) {
        s4_fault_scans |= 2;
    } else {
        if (s4_fault_scans & 1) {
            return;                    // 两组都运行后只在一秒组结束时采样。
        }
        s4_fault_scans |= 1;
    }
    if (s4_fault_scans != 3) {
        return;
    }
    if (!bf4g_fault_sample(&faults)) {
        s4_fault_flags |= S4_FAULT_INVALID;
        return;
    }
    if (s4_fault_state == S4_FAULT_BASELINE) {
        s4_fault_latest = faults;
        s4_fault_seq = 1;
        uart4_queue_fault(0);
    } else if (faults != s4_fault_latest || (s4_fault_flags & S4_FAULT_INVALID)) {
        if (s4_fault_seq == 0xffffffffUL) {
            return;                    // 本启动内不复用序号，待下次上电建立基线。
        }
        if (s4_fault_flags & (S4_FAULT_CHANGED | S4_FAULT_INVALID)) {
            s4_fault_flags |= S4_FAULT_RESYNC; // 已有变化被合并，或样本恢复有效。
        }
        s4_fault_flags &= ~S4_FAULT_INVALID;
        s4_fault_flags |= S4_FAULT_CHANGED;
        s4_fault_latest = faults;
        s4_fault_seq++;                 // 已知中间变化占用其序号，因此合并后有可见缺口。
    }
}

/******************************************************************************
 * 函数名称       : uart4_fault_awaiting_ack
 * 功能描述       : 判断当前是否仍保留已发的主动帧等待确认。
 * 输入参数       : None
 * 返回值         : 等待确认时为一。
 * 日期            : 2026.11.24
 ******************************************************************************/
bit uart4_fault_awaiting_ack(void) //20261124-0 add
{
    return s4_fault_state == S4_FAULT_WAIT;
}

/******************************************************************************
 * 函数名称       : UART4_int
 * 功能描述       : 接收串口四的字节，或将已经冻结的应答逐字节交给硬件。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
void UART4_int(void) interrupt UART4_VECTOR //20261116-0 change
{
    u8 value;

    if (RI4) {
        value = S4BUF;
        CLR_RI4();
        if ((AUXINTIF & T3IF) && s4_state == S4_DISCARD) {
            s4_state = S4_IDLE;          // 新字节已在完整静默之后到达。
        }
        if ((AUXINTIF & T3IF) && s4_state == S4_RECEIVE) {
            s4_state = S4_READY;         // 保住已完整的上一帧，当前字节不能排队。
        }
        if (s4_state == S4_READY || s4_state == S4_PROCESS ||
            s4_state == S4_SEND || s4_state == S4_DRAIN) {
            s4_disturbed = 1;
        } else {
            if (s4_state == S4_IDLE) {
                s4_count = 0;
                // 首字节即按当时生效的地址接收，捕获的地址留在共享帧头。
                if (value == 0 || value == 0xff || value != config.Dev_Address) {
                    s4_state = S4_DISCARD;
                } else {
                    s4_state = S4_RECEIVE;
                }
            }
            if (s4_state == S4_RECEIVE) {
                if (s4_count < BF4G_FRAME_MAX) {
                    s4_frame[s4_count++] = value;
                } else {
                    s4_state = S4_DISCARD; // 超长帧的合法前缀也不得被解析。
                }
            }
            S4_TIMER_ARM(S4_QUIET_RELOAD);
        }
    }

    if (TI4) {
        CLR_TI4();
        if (s4_state == S4_SEND) {
            if (s4_next < s4_count) {
                S4BUF = s4_frame[s4_next++];
            } else {
                // 串口发送标志在停止位开始时置位，再等待两个位宽后释放。
                s4_state = S4_DRAIN;
                S4_TIMER_ARM(S4_TAIL_RELOAD);
            }
        }
    }
}

/******************************************************************************
 * 函数名称       : UART4_Timer3_int
 * 功能描述       : 在静默或发送尾部计时结束时切换共享帧的所有权。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
void UART4_Timer3_int(void) interrupt TIMER3_VECTOR //20261116-0 add
{
    Timer3_Stop();
    AUXINTIF &= ~T3IF;
    if (s4_state == S4_DRAIN && s4_fault_tx) { //20261124-2 add 发送结束后才开始本轮确认窗口。
        s4_fault_tx = 0;
        s4_fault_timeout = S4_FAULT_WAIT_TICKS;
    }
    if (s4_state == S4_RECEIVE) {
        s4_state = S4_READY;
    } else if (s4_state == S4_PROCESS && s4_control_ticks != 0) { //20261117-7 add
        if (--s4_control_ticks != 0) {
            S4_TIMER_ARM(S4_QUIET_RELOAD);
        }
    } else if (s4_state == S4_DRAIN && (s4_disturbed || RI4)) {
        s4_disturbed = 0;
        s4_state = S4_DISCARD;           // 发送期间有输入，等待新的完整静默。
        S4_TIMER_ARM(S4_QUIET_RELOAD);
    } else if (s4_state == S4_DRAIN || s4_state == S4_DISCARD) {
        s4_state = S4_IDLE;
    }
}

/******************************************************************************
 * 函数名称       : uart4_finish
 * 功能描述       : 在主循环处理一条完成的请求并启动异步应答。
 * 输入参数       : length，应答帧字节数；零表示丢弃。
 * 返回值         : None
 * 日期            : 2026.11.17
 ******************************************************************************/
static void uart4_finish(u8 length) //20261117-7 add
{
    bit interrupts = EA;
    // 禁止两个串口状态中断在移交所有权和写首字节之间运行。
    EA = 0;
    Timer3_Stop();
    AUXINTIF &= ~T3IF;                 // 撤销控制等待，发送尾部会重新启用定时器。
    s4_control_ticks = 0;
    if (length != 0) {
        s4_count = length;
        s4_next = 1;
        s4_state = S4_SEND;
        S4BUF = s4_frame[0];
    } else if (s4_disturbed || RI4) {
        s4_disturbed = 0;
        s4_state = S4_DISCARD;
        S4_TIMER_ARM(S4_QUIET_RELOAD);
    } else {
        s4_state = S4_IDLE;
    }
    EA = interrupts;
}

/******************************************************************************
 * 函数名称       : uart4_send_fault
 * 功能描述       : 有完整首帧且没有普通请求占用串口时启动发送。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.24
 ******************************************************************************/
static void uart4_send_fault(void) //20261124-2 change
{
    bit interrupts = EA;
    u8 i;

    EA = 0;
    if (s4_state == S4_IDLE && !RI4) {
        for (i = 0; i < BF4G_FAULT_FRAME_LENGTH; i++) {
            s4_frame[i] = s4_fault_frame[i];
        }
        s4_fault_due = 0;
        s4_fault_tx = 1;
        s4_fault_sent++;
        s4_fault_state = S4_FAULT_WAIT;   // 重发从原帧缓冲复制，保持全部字节不变。
        uart4_finish(BF4G_FAULT_FRAME_LENGTH);
    }
    EA = interrupts;
}

/******************************************************************************
 * 函数名称       : uart4_service
 * 功能描述       : 处理完整请求或已到期的控制应答，主循环可继续扫描。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.17
 ******************************************************************************/
void uart4_service(void) //20261124-3 change
{
    u8 length;
    bit interrupts;

    if (s4_fault_state == S4_FAULT_WAIT && s4_fault_due && s4_fault_sent == S4_FAULT_ATTEMPTS) {
        s4_fault_state = S4_FAULT_DISABLED;
        s4_fault_stop_mark = S4_FAULT_STOP_MARK; // 最后一轮超时后的回包不能恢复主动上报。
    }
    if (s4_state == S4_PROCESS) {
        if (s4_control_ticks == 0) {
            uart4_finish(bf4g_control_result(s4_frame, 1));
        }
        return;
    }
    if (s4_state != S4_READY) {
        if (s4_state == S4_IDLE && s4_fault_state == S4_FAULT_IDLE &&
            (s4_fault_flags & (S4_FAULT_CHANGED | S4_FAULT_INVALID)) == S4_FAULT_CHANGED) {
            uart4_queue_fault((s4_fault_flags & S4_FAULT_RESYNC) != 0 ? 2 : 1);
        }
        if (s4_state == S4_IDLE &&
            (s4_fault_state == S4_FAULT_READY || (s4_fault_state == S4_FAULT_WAIT && s4_fault_due))) {
            uart4_send_fault();
        }
        return;
    }

    s4_state = S4_PROCESS;             // 独占共享帧，等待期间不接收下一笔请求。
    if (s4_count > 5 && s4_frame[5] == 0xa0) { //20261124-0 add 主动确认单独处理，始终静默。
        if (uart4_fault_awaiting_ack() &&
            bf4g_fault_ack_matches(s4_frame, s4_count, s4_fault_frame)) {
            interrupts = EA;
            EA = 0;
            s4_fault_timeout = 0;
            s4_fault_due = 0;
            EA = interrupts;
            s4_fault_state = S4_FAULT_IDLE; // 本帧已经确认，可以创建后续状态帧。
        }
        uart4_finish(0);
        return;
    }
    length = bf4g_reply(s4_frame, s4_count);
    if (length == BF4G_REPLY_PENDING) {
        s4_control_ticks = S4_CONTROL_TICKS;
        S4_TIMER_ARM(S4_QUIET_RELOAD);
        return;
    }

    uart4_finish(length);
}

/******************************************************************************
 * 函数名称       : uart4_control_scanned
 * 功能描述       : 由主循环在正常控制扫描结束时检查待确认的控制结果。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.17
 ******************************************************************************/
void uart4_control_scanned(void) //20261117-7 add
{
    u8 length;

    if (s4_state == S4_PROCESS && s4_control_ticks != 0) {
        length = bf4g_control_result(s4_frame, 0);
        if (length != 0) {
            uart4_finish(length);
        }
    }
}

/******************************************************************************
 * 函数名称       : uart4_sending
 * 功能描述       : 判断应答是否仍在发送或等待最后一个停止位完成。
 * 输入参数       : None
 * 返回值         : 发送未结束时为一。
 * 日期            : 2026.11.16
 ******************************************************************************/
bit uart4_sending(void) //20261116-0 add
{
    return s4_state == S4_SEND || s4_state == S4_DRAIN;
}

/******************************************************************************
 * 函数名称       : uart4_blocks_save
 * 功能描述       : 在控制等待、应答发送及发送尾部计时期间推迟后台擦写。
 * 输入参数       : None
 * 返回值         : 需要推迟后台保存时为一。
 * 日期            : 2026.11.17
 ******************************************************************************/
bit uart4_blocks_save(void) //20261117-7 add
{
    return s4_state == S4_PROCESS || uart4_sending();
}
