#ifndef	_CONFIGPAR_H
#define	_CONFIGPAR_H

#include "system.h"

//时间参数
#define Fault_LOSEPHASE_Tdelay				15	//缺相保护延时判断
#define Fault_UNLOAD_Time					4	//s
#define Fault_UNLOAD_Recovery_Time		    30 	//s

#define RUNMODE_MANUAL	0
#define RUNMODE_AUTO	1
#define RUNMODE_TIMER   2

#define SENSORTYPE_POLE			0
#define SENSORTYPE_PRESS		1
#define SENSORTYPE_WATER4_20MA	2
#define SENSORTYPE_PRESS4_20MA	3
#define SENSORTYPE_PRESS0_5V	4

#define CTROLTYPE_PUMP	0
#define CTROLTYPE_DRAIN 1 

#define TOTAL_TIME_DEBUG_MODE 0 //20260430-3 add set to 1 to accumulate and display P-21/P-22 in minutes for debugging


typedef struct{
    
	u8 Dev_type;
	u8 Hand_Auto_timer_Flag;
    u16 AUTO_Timing_Run;				//定时功能的运行时间 0~999
    u16 AUTO_Timing_Stop;			//定时功能的停止时间 0~999
    u16 AUTO_FANGXIU;
    u16 Timer_Standby_Restart;
	u16 Timer_Unload_Restart;
	u16 Timer_OUV_Restart;
    u16 Set_STANDBY_Current1;		//设定的空载保护电流
    u16 Set_OVERLOAD_Current1;	//设定的过载保护电流
	u8 Lock1;		
    u16 Set_STANDBY_Current2;		//设定的空载保护电流
    u16 Set_OVERLOAD_Current2;	//设定的过载保护电流
	u8 Lock2;	
	
	u16 UVP_Value;
	u16 OVP_Value;	
	u8  A_Turn;
	u8  B_Turn;
	u8  shuiWeiDingshi;
	
	u16 Sensor_Rang;
	u16 Sensor_STOP;
	u16 Sensor_ON1;
	u16 Sensor_ON2;
	
	u8 Dev_Address;
	u8 Pump_AB_Mode;
	u8 Sensor_Mode;
	
	
    u16 RF_Family[5];
    u8  Record_Type[10];
    u16 Record_Dat1[10];
    u16 Record_Dat2[10];
	
	u8  run_fen;
	u16 run_xiaoshi;

	u8  Auto_Mode;
	
	u8  fault_mask;

	u16 Sensor_overFlow;

    u16 Total_Run_Hour;        //20260430-3 change total run time; hours in normal mode, minutes in debug mode
    u16 Total_Power_Day;       //20260430-3 change total power-on time; days in normal mode, minutes in debug mode

	u8 banben;

    u8  Total_Run_Minute;      //20260430-3 add minute remainder of total run time below one hour
    u8  Total_Power_Hour;      //20260430-3 add hour remainder of total power-on time below one day
    u8  Total_Power_Minute;    //20260430-3 add minute remainder of total power-on time below one hour
		
}config_t;

void Par_Init(void);
void save_pro(void);
void Total_Time_Second_Handle(u8 pump_running); //20260430-3 add
void Total_Run_Time_Clear(void);                //20260501-1 add clear total run time
void Total_Power_Time_Clear(void);              //20260501-1 add clear total power-on time
extern config_t config;

#endif
