

#include	"delay.h"


void delay_ms(unsigned char ms)
{
	unsigned int i;
	do{
		i = MAIN_Fosc / 10000;
		while(--i);
	}while(--ms);
}

void delay_us(unsigned int us)
{
	unsigned int i;
	
	do{
		NOP(10);
	}while(--us);
}
