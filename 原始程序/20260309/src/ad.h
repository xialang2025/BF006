#ifndef	_AD_H
#define	_AD_H

#include	"system.h"


typedef struct{
	u16 Voltage_Line;
	u16 Current_A1;
	u16 Current_B1;
	u16 Current_C1;
	
	u16 Current_A2;
	u16 Current_B2;
	u16 Current_C2;	
	
}MEASURE_T;

void MEASURE_Pro(void);
void MEASURE_init(void);
void MEASURE_timeOut(void);

#endif