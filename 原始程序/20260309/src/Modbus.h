
#ifndef MODBUS_H
#define MODBUS_H

#define Usart_Idle_Time	10
#define Usart_Rec_Len		32



#define	MBS_Error_Addr  0x01
#define	MSB_Error_Reg 	0x02
#define	Msb_Error_Dat	  0x03


struct Uart_Type{
	unsigned char Rec_Start_Flag;
	unsigned char Rec_Cnt;
	unsigned char Rec_Fifo[Usart_Rec_Len];
	unsigned char Rec_Data[Usart_Rec_Len];
	unsigned char Rec_Idle_Time;
	unsigned char Rec_End_Flag;
	unsigned char Rec_Len;
};

extern struct Uart_Type RecData;

extern void Modbus_analysis(void);


#endif



