#ifndef	_RS485_H
#define	_RS485_H

#include	"config.h"
#include	"delay.h"
#include	"string.h"
#include    "system.h"

void rs485_Init(void);
void M_Uart_Send_Block(unsigned char *sdat, unsigned char len);
void M_Uart_Set_BaudRate(char baudrate);

#endif