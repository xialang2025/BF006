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

extern volatile bit voltage_sample_ready; //20261116-2 add 首次有效发布电压后置位。
extern volatile bit sensor_adc_fresh; //20261116-6 add 传感器类型变更后等待新的模拟量采样。
extern volatile bit current_sample_ready; //20261116-2 add 首次发布各相电流及模拟量后置位。

void MEASURE_Pro(void);
void MEASURE_init(void);
void MEASURE_timeOut(void);

#endif