#ifndef UPDATA4G_H
#define UPDATA4G_H

#include	"config.h"

#define TIMEOUTCNT 20

typedef struct{
	
	u8 UpWaterLever;  //上水位 // 0低水位  1中  2高
//	u8 UpWaterLever_Timer;
	u8 BottomWaterLever; //下水位 // 0低水位  1中  2高
//	u8 BottomWaterLever_Timer;
	u8 ErrorCodeA;		//A泵故障 //0无故障 1过压  2欠压 3空载 4过载 5A缺相 6B缺相 6C缺相
//	u8 ErrorCodeA_Timer;
	u8 ErrorCodeB;		//B泵故障 //0无故障 1过压  2欠压 3空载 4过载 5A缺相 6B缺相 6C缺相
//	u8 ErrorCodeB_Timer;	
	u16 Current_B_A;		//B泵电流
	u16 Current_B_B;		//B泵电流 
	u16 Current_B_C;		//B泵电流 	
	u8 Current_B_Timer;
	u16 Current_A_A;		//A泵电流
	u16 Current_A_B;		//A泵电流	
	u16 Current_A_C;		//A泵电流	
	u8 Current_A_Timer;	
	u16 Voltage_Line;	//电压
	u8 Voltage_Line_Timer;
	u8 Pump_A_state;	//A泵状态  //0停止  1开启
	u8 Pump_A_update;
//	u8 Pump_A_state_Timer;
	u8 Pump_B_state;	//B泵状态  //0停止  1开启
	u8 Pump_B_update;
//	u8 Pump_B_state_Timer;
	u8 Hand_Auto_timer_Flag;		//工作状态 //0手动  1自动  2循环定时

	u8 GateSW;


	u8 Sensor_Mode; // 0水位 1压力 2 液位传感器器 3压力传感器 4远传压力表
//	u8 Dev_type; //机种

	u8 Auto_Mode;// 0水位 1压力 2 液位传感器器 3压力传感器 4远传压力表


	u16 SensorOut; //传感器输出 

//  设置参数 
	u16 AUTO_Timing_Run; //定时功能的运行时间
	u16 AUTO_Timing_Stop; //定时功能的停止时间
	u16 Set_STANDBY_CurrentA; 	//A泵空载电流
	u16 Set_OVERLOAD_CurrentA; 	//A泵过载载电流
	u16 Set_STANDBY_CurrentB; 	//B泵空载电流
	u16 Set_OVERLOAD_CurrentB;	//B泵过载载电流
	
	u16 UVP_Value;    //欠压设置
	u16 OVP_Value;    //过压设置
	
	u8 Pump_AB_Mode;  //0 A主B备 1 B主A备 2双泵轮回 3独立运行
	u16 Sensor_Rang;
	u16 Sensor_STOP; 	//停止位
	u16 Sensor_ON1;		//单泵启动
	u16 Sensor_ON2;		//双泵启动

	u8 sensorOut_Timer;
	u16 sensorOut;
	
	u8 ErrCode;
	
	u8 GuZhangJiLu;
	u8 GuZhangJiLu_Timer;


	u8 updataAll;
	u8 Hand_set; //头部发送

	u16 send_delay_A;
	u8 send_out_A;	

	u16 send_delay_B;
	u8 send_out_B;	

	u8 sensorUpdate;	

}Send_dat_t;

void updata4G_init(void);
void Updata_4G(void);
void Analyse_4G(void);

extern Send_dat_t Send_dat;
#endif