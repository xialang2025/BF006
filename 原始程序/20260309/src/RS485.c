
#include "system.h"
#include "RS485.h"
#include "Modbus.h"

#define	BaudRate1		9600UL	//选择波特率



#define	Timer2_Reload	(65536UL -(MAIN_Fosc / 4 / BaudRate1))		//Timer 2 重装值， 对应300KHZ
bit	B_TX2_Busy;	//发送忙标志

#if(Function_RS485 == 1)

/***********************************************************
接收中断
***********************************************************/
extern void Modbus_Receive_handle(u8 rdata);

void UART3_IRQHandler(void) interrupt UART3_VECTOR
{
	if(RI3)
	{
		CLR_RI3();
	  Modbus_Receive_handle( S3BUF);
	
	}
	
	if(TI3)
	{
		CLR_TI3();
		B_TX2_Busy = 0;		//清除发送忙标志
	}	
}
/***********************************************************




/***********************************************************
RS485用到得串口初始化
***********************************************************/
void rs485_Init(void)
{			

//	if(config.Dev_Address < 1 || config.Dev_Address>255)
//	{
//		config.Dev_Address=1;
//		sys.save_flag = 1;
//	}
	
//	CLK_PCKENR1 |= 0X04;
//	UART1_CR1 = 0x00;
//	UART1_CR2 = 0x00;
// 	UART1_CR3 = 0x00;
	
	// 设置波特率，必须注意以下几点：
	// (1) 必须先写BRR2
	// (2) BRR1存放的是分频系数的第11位到第4位，
	// (3) BRR2存放的是分频系数的第15位到第12位，和第3位
 	// 到第0位
	// 例如对于波特率位9600时，分频系数=16000000/9600=208
 	// 对应的十六进制数为00D0，BBR1=0D,BBR2=00
	/*
		baud     brr2      brr1
		57600    0x06      0x11
		115200   0X0B      0X08
		9600     0x00	   0x0D
	*/
// 	UART1_BRR2	= 0x02;
//	UART1_BRR1	= 0x68;
//	UART1_CR2	= 0x2c;		//允许接收，发送，开接收中断发送中断
//	UART1_SR 	= 0X00;

	S3_8bit();				//8位数据
	
//	S1_USE_P30P31();		//UART1 使用P30 P31口	默认
//	S2_USE_P10P11();		//UART1 使用P36 P37口
	S3_USE_P50P51();		//UART1 使用P16 P17口
	
	S3_BRT_UseTimer2();

	AUXR &= ~(1<<4);	//Timer stop
	AUXR |= 0x01;		//S1 BRT Use Timer2;
	AUXR &= ~(1<<3);	//Timer2 set As Timer
	AUXR |=  (1<<2);	//Timer2 set as 1T mode
	TH2 = (u8)(Timer2_Reload>>8);
	TL2 = (u8)Timer2_Reload;
	IE2  &= ~(1<<2);	//禁止中断
	AUXR |=  (1<<4);	//Timer run enable

	S3_RX_Enable();	//允许接收
	S3_Int_Enable();	//允许中断
	
	memset(RecData.Rec_Fifo,0,Usart_Rec_Len);
	memset(RecData.Rec_Data,0,Usart_Rec_Len);
	RecData.Rec_Cnt = 0;
}

void M_Uart_Send_Block(unsigned char *sdat, unsigned char len)
{
	unsigned char i;
	
	RS485_EN  = 1; //发送使能
	
	for(i=0;i<len;i++)
	{
		B_TX2_Busy = 1;
		S3BUF = sdat[i];
		while(B_TX2_Busy);

	}	

	delay_ms(10);
	RS485_EN  = 0; //接收使能
}

void M_Uart_Set_BaudRate(char baudrate)
{
//	u16 reload = 0;
//	reload = (u16)(65536UL -(MAIN_Fosc / 4 / baudrate));
//	
//	AUXR &= ~(1<<4);	//Timer stop		波特率使用Timer2产生
//	AUXR |= 0x01;		//S1 BRT Use Timer2;
//	AUXR |=  (1<<2);	//Timer2 set as 1T mode
//	TH2 = (u8)(reload >> 8);
//	TL2 = (u8)reload;
//	AUXR |=  (1<<4);	//Timer run enable	
	
}

#endif



