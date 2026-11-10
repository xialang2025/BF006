
#include "system.h"

void GPIO_Init(void)
{	
    RELAY1 = 0;
    RELAY2 = 0;
	RELAY3 = 0;
	
	P0PU = 0;
	P1PU = 0;
	P2PU = 0;
	P3PU = 0;
	P4PU = 0;
	P5PU = 0;
	
	P0NCS = 0;
	P1NCS = 0;
	P2NCS = 0;
	P3NCS = 0;
	P4NCS = 0;
	P5NCS = 0;
	
	P0IE = 0x90;
	P1IE = 0x7c;
	P2IE = 0xff;
	P3IE = 0xff;
	P4IE = 0xff;
	P5IE = 0xff;
	
	
	P0M1 = 0x6f;
	P0M0 = 0x90;
	
	P1M1 = 0x83;
	P1M0 = 0x44;
	
	P2M1 = 0x00;
	P2M0 = 0xCF;
	
	P3M1 = 0xC4;
	P3M0 = 0x30;
	
	P4M1 = 0x1E;
	P4M0 = 0xE0;
	
	P5M1 = 0x00;
	P5M0 = 0x38;
	
	P20 = 0;
	P24 = 1;
	P25 = 1;
}