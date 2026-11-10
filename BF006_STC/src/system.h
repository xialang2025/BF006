#ifndef	_SYSTEM_H
#define	_SYSTEM_H

#include	"config.h"
#include	"delay.h"
#include	"string.h"
#include "configPar.h"
#include "contact_overload.h" //20260729-5 add 触点过载锁存状态接口

#define Function_RF 1
#define Function_RS485 1

#define CUSTOMER_TEST_LIMIT_ENABLE  0    //20260501-0 add customer test time-limit fault switch: 0 off, 1 on
#define CUSTOMER_TEST_LIMIT_MINUTE  120  //20260501-0 add customer test time limit in minutes

#define ERR_UVP          0
#define ERR_OVP          1

#define ERR_OVERLOAD1    2
#define ERR_STANDBY1     3
#define ERR_LOSE_A1      4
#define ERR_LOSE_B1      5
#define ERR_LOSE_C1      6

#define ERR_OVERLOAD2    7
#define ERR_STANDBY2     8
#define ERR_LOSE_A2      9
#define ERR_LOSE_B2      10
#define ERR_LOSE_C2      11

#define ERR_SENSOR       12

#define ERR_NOLOAD_A     13
#define ERR_NOLOAD_B     14

#define ERR_LOCK_A       15
#define ERR_LOCK_B       16

#define ERR_NUMB       	 17

#define V1_Limit_Current	130
#define V1_Set_Current		80
#define V2_Limit_Current	210
#define V2_Set_Current		150
#define V3_Limit_Current	270
#define V3_Set_Current		220
#define V4_Limit_Current	390
#define V4_Set_Current		300
#define V5_Limit_Current	880
#define V5_Set_Current		800

//======================================
#define Model_3PH_380_1C1_LCD			0			
#define Model_3PH_380_1C2_LCD			1
#define Model_1PH_220_1C1_LCD			2
#define Model_1PH_220_1C2_LCD			3
#define Model_3PH_220_1C1_LCD			4			
#define Model_3PH_220_1C2_LCD			5

#define Model_1C1		0
#define Model_1C2		1

#define Model_3PH		0
#define Model_1PH		1

#define Vol_380		0
#define Vol_220		1


// device type selection
//=================================================================
#define Model_Device Model_1PH_220_1C2_LCD
//================================================================

#if(Model_Device==Model_3PH_380_1C2_LCD)

#define Dev_Model		0xB3
#define DIS_TYPE		Display_LCD

//#define Function_RF		0
#define Model_Voltage		Model_3PH
#define VoltageValue		Vol_380

#define Model_OUT			Model_1C2

#elif(Model_Device==Model_3PH_380_1C1_LCD)

#define Dev_Model		0xB2
#define DIS_TYPE		Display_LCD

//#define Function_RF		0
#define Model_Voltage		Model_3PH
#define VoltageValue		Vol_380

#define Model_OUT			Model_1C1

#elif(Model_Device==Model_1PH_220_1C1_LCD)

#define Dev_Model		0xA2
#define DIS_TYPE		Display_LCD

//#define Function_RF		0
#define Model_Voltage		Model_1PH
#define VoltageValue		Vol_220

#define Model_OUT			Model_1C1

#elif(Model_Device==Model_1PH_220_1C2_LCD)

#define Dev_Model		0xA3
#define DIS_TYPE		Display_LCD

//#define Function_RF		0
#define Model_Voltage		Model_1PH
#define VoltageValue		Vol_220

#define Model_OUT			Model_1C2

#elif(Model_Device==Model_3PH_220_1C1_LCD)

#define Dev_Model		0xB2
#define DIS_TYPE		Display_LCD

//#define Function_RF		0
#define Model_Voltage		Model_3PH
#define VoltageValue		Vol_220

#define Model_OUT			Model_1C1

#elif(Model_Device==Model_3PH_220_1C2_LCD)

#define Dev_Model		0xB2
#define DIS_TYPE		Display_LCD

//#define Function_RF		0
#define Model_Voltage		Model_3PH
#define VoltageValue		Vol_220

#define Model_OUT			Model_1C2

#endif


typedef struct{
	
	u8 Record_Dis_Flag;		// 1: display history records
	u8 Record_Cnt;			// record count
	u8 Record_Timer; 		// exit after 10 seconds
	
	
	u32 Fault_Error;
	u8 Err_Dis;
	u8 Fault_A_Flag;
	u8 Fault_B_Flag;
	
	u16 Voltage_Line;
	u16 Current_A1;
	#if(Model_Voltage==Model_3PH)
	u16 Current_B1;
	u16 Current_C1;	
	u16 Current1_Max;
	u8 Current1_Max_Index;
	u16 Current1_Min;
	u8 Current1_Min_Index;
	#endif
	u16 avgCurrent1;
	
	u16 Current_A2;
	#if(Model_Voltage==Model_3PH)
	u16 Current_B2;
	u16 Current_C2;
	u16 Current2_Max;
	u8 Current2_Max_Index;
	u16 Current2_Min;
	u8 Current2_Min_Index;
	#endif
	u16 avgCurrent2;	
	
	u8 shang_shuiwei;
	u8 shang_DIR;
	u8 xia_shuiwei;
	u8 xia_DIR;	
	
	u16 xia_zhong_count;
	u8 xia_zhong_Flag;	

	u16 xia_gao_count;
	u8 xia_gao_Flag;
	
	u16 shang_zhong_count;
	u8 shang_zhong_Flag;


	u16 shang_gao_count;
	u8 shang_gao_Flag;


	u16 Pump_Add_count;
	u8 Pump_Add_Flag;
	u8 Contact_Overload_A_Flag; //20260729-5 add A 泵触点过载锁存标志

	u16 Flag_Over_count;
	u8 Flag_OverFlow;	
#if (Model_OUT == Model_1C2)
	u8 Contact_Overload_B_Flag; //20260729-7 add B 泵触点过载锁存标志
#endif
	
	u8 Pump_A_ON;
	u8 Pump_B_ON;

	u8 Pump_Flag;
	
	u8 Pump_A_state;
	u8 Pump_B_state;

	u8 Setting_Flag;
	u8 Setting_Index;
	
	u8 save_flag;
	
	u16 Vol_AD;
	u16 Cur_AD;
	u16 SensorOut;
	u8 SensorLever;

	u8 FangXiuAFlag;
    u8 FangXiuBFlag;
	
	u16 Fang_Xiu_count_A;	// anti-rust run counter for pump A
	u16 Fang_Xiu_count_B;	// anti-rust run counter for pump B
	
	u8 Fang_Xiu_A_Down;
	u8 Fang_Xiu_B_Down;	
	
	u8 autoTimerFlag;
	u16 Auto_timer_count;
	
	u16 Timer_Startup_Counter;
	u16 Pump1_Startup_Counter;
	u16 Pump2_Startup_Counter;
	
	u16 autoTurnA_cnt;
	u16 autoTurnB_cnt;
	
	u16 Sensor_Rang;
	u16 Sensor_STOP;
	u16 Sensor_ON1;
	u16 Sensor_ON2;
	u8 sensor_err;
	u8 Sensor_OverFlow;
    u8 Pump_master;	
	u8 SW_State;
	
	u8 Auto_Mode;
	
	u8 hour;
	u8 second;
	u8 minute;
	
	u8 RF_Key_Flag;	
	u8 Faulte_out;
	
	
	/* single-phase dry-run protection */
	#if(Model_Voltage==Model_3PH)
	u8 pump_a_loss_delay;
	u8 pump_a_loss_cnt;
	
	u8 pump_b_loss_delay;
	u8 pump_b_loss_cnt;
	#endif

	u8 beep_err;
	
	u8 up4G_err;
	
	u8 flowover;
	u8 GateSW;
	
}sys_t;


sbit  	RELAY1= P2^3;
sbit  	RELAY2= P2^2;
sbit  	RELAY3= P2^1;

sbit  	OverFlow_P  = P3^6;
sbit  	PUMP_ADD   	= P3^7;
sbit  	LowWater_M 	= P4^1;
sbit  	LowWater_H 	= P4^2;
sbit  	HighWater_M	= P4^3;
sbit  	HighWater_H	= P4^4;


sbit  	RF_315M_PIN = P3^2;
sbit 	RS485_EN = P3^5;


extern bit Boot_Flag;
extern sys_t sys;

void sys_init(void);
void Sys_Control(void);
void Water_Level(void);
#if (Model_OUT == Model_1C2)
void Set_Pump2_OverLoad_Err(void); //20260729-7 add 建立 B 泵过载并写入历史记录
#endif
void Contact_Overload_Control(void); //20260729-5 add 处理滤波后的触点过载保护
void shuiwei_check(void);
void RELAY_Auto_Control(void);
void Timer_mode(void);
void FanXiu_pro(void);
void Timer100ms_pro(void);
void Fault_Control(void);
void Customer_Test_Limit_Handle(void);       //20260501-0 add customer test time-limit fault check
u8 Customer_Test_Limit_IsFaultActive(void);  //20260501-0 add query whether the customer test time-limit fault is locked

void clear_A_Fault(void);
void clear_B_Fault(void);

void zhengdingA(void);
void Clear_zhengdingA(void);
void zhengdingB(void);
void Clear_zhengdingB(void);
void autoZhending(void);
void LosePhase_Check(void);

#endif
