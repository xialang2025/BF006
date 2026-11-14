
#include "system.h"
#include "configPar.h"
#include "config_store.h" //20261114-13 add



#if(Function_RF==1)

#if(Model_OUT==Model_1C2)
#define RFCODE_PUMP_A_OPEN	0x08
#define RFCODE_PUMP_A_STOP	0x04
#define RFCODE_PUMP_B_OPEN	0x02
#define RFCODE_PUMP_B_STOP	0x01
#else
#define RFCODE_PUMP_A_OPEN	0x04
#define RFCODE_PUMP_A_STOP	0x02
#define RFCODE_PUMP_B_OPEN	0x08
#define RFCODE_PUMP_B_STOP	0x01

#endif




extern void Speak_Con(u8 type);


#if BF006_CONFIG_STORE_ENABLED
static volatile u32 rf_pending;
static volatile u8 rf_pending_ready;
#endif
u32 RF_Value_Rev	= 0x0000;		//接收到的数据
u8 Rev_flag = 0;					//接收标志
u8 Rev_Down = 0;

u32 Cmd_Decord_Last = 0;
u32 Cmd_Decord_New = 0;

u16 Cycle_Cnt = 0;
u16 Cycle_Low_Cnt = 0;
u16 Cycle_High_Cnt = 0;
u8 Pin_Status = 0;

u8 Rev_start_flag = 0;
u8 Rev_Bit_Num = 0;

u8 pipe(void)
{
    u8 i = 0;
    for(i=0; i<5; i++)
    {
        if((RF_Value_Rev>>8)==config.RF_Family[i]&&config.RF_Family[i]!=0&&config.RF_Family[i]!=0xFF) break;
    }
    if(i==5)
    {
        return 0;
    } else {
        return 1;
    }


}

//RF接收程序
//目前执行时间间隔为10000/秒，即为100us
void RF_Decord(void)
{
#if !BF006_CONFIG_STORE_ENABLED
    u8 i;
#endif

    Cycle_Cnt++;	//计数器累加

    if(Pin_Status==RF_315M_PIN)	  //当前读到的端口状态跟上一次的一样时，退出
    {
        return;
    }
    Pin_Status = RF_315M_PIN; 	//如果不一样，保存的端口状态

    if(Pin_Status)  //说明目前是低电平向高电平跳变
    {
        Cycle_Low_Cnt =  Cycle_Cnt;		//将计数值赋予低电平计数器
    }
    else		   //说明目前是高电平向低电平跳变
    {
        Cycle_High_Cnt =  Cycle_Cnt;	//将计数值赋予高电平计数器
    }

    Cycle_Cnt = 0;//计数值清零

    if(Pin_Status==0)			//0->1 上升沿判断； 下降沿退出
    {
        return;
    }

    //超长低电平，则置位为0
    if(Cycle_Low_Cnt>=200)
    {
        Rev_Down 		= 0;
        RF_Value_Rev 	= 0;
        Rev_flag 		= 0;
        Rev_start_flag = 0;

        return;
    }
    // 10ms引导码
    else if((Cycle_Low_Cnt>90)&&(Cycle_Low_Cnt<200))
    {
        Rev_start_flag = 1;
        Rev_Bit_Num = 0;
        Cmd_Decord_New = 0;
        return;
    }

    if(Rev_start_flag==0) return;


    //解码
    //===================================
    Cmd_Decord_New = Cmd_Decord_New<<1;	//解码0

    if(Cycle_Low_Cnt<7)
    {
        Cmd_Decord_New = Cmd_Decord_New|0x01;	//解码1
    } else if(Cycle_Low_Cnt > 20)
    {
        Rev_start_flag = 0;
			
        return;
    }
    Rev_Bit_Num++;
    if(Rev_Bit_Num>=24)
    {
        Rev_Bit_Num = 0;

        Rev_start_flag = 0;

        if((Cmd_Decord_New==Cmd_Decord_Last)&(Cmd_Decord_New!=0))	//两次接收的码都是一样时，且不为0
        {
#if !BF006_CONFIG_STORE_ENABLED
            RF_Value_Rev 	= Cmd_Decord_New;
#endif
            if(Rev_Down==0)	Rev_flag = 1;
            Rev_Down = 1;
        }
        else
        {
            Rev_Down = 0;		//类似松手
        }

        Cmd_Decord_Last = Cmd_Decord_New;			//保存解码值
        Cmd_Decord_New = 0;
    } else {

        return;
    }


#if BF006_CONFIG_STORE_ENABLED
    if (Rev_flag) {
        Rev_flag = 0;
        if (!rf_pending_ready || (Cmd_Decord_Last & 0x0F) == RFCODE_PUMP_A_STOP) {
            rf_pending = Cmd_Decord_Last;
            rf_pending_ready = 1;
        }
    }
#else
    //RF 按键处理
    if(Rev_flag==1)
    {
        Rev_flag = 0;

        if(sys.RF_Key_Flag==1)
        {
            if(pipe() == 0)
            {

                //下载家族类号
                for(i=0; i<4; i++) config.RF_Family[4-i] = config.RF_Family[3-i];

                config.RF_Family[0] = RF_Value_Rev>>8;

                sys.save_flag = 1;
            }

            sys.RF_Key_Flag = 0;
            Speak_Con(2);
        }
        else
        {
            if(pipe() == 0)
            {
                return;
            }

            switch(RF_Value_Rev&0x000F)
            {

            case RFCODE_PUMP_A_STOP:			// A停止
//            case RFCODE_PUMP_A_STOP2:
                Speak_Con(0);
                if(sys.Record_Dis_Flag==1)
                {
                    sys.Record_Dis_Flag = 0;
                }
                else
                {
                    sys.Pump_A_ON = 0;
                    config.Hand_Auto_timer_Flag 	= RUNMODE_MANUAL;
                    if(sys.Fault_A_Flag)
                    {
                        clear_A_Fault();
#if 0 //20260729-8 del 故障标志由统一清除函数按触点状态决定
                        sys.Fault_A_Flag = 0;
#endif
                    }
                }

                break;
            case RFCODE_PUMP_A_OPEN:				// A启动
                Speak_Con(0);
                if(sys.Record_Dis_Flag==1)
                {
                    sys.Record_Dis_Flag = 0;
                }
                else
                {
                    if(sys.Fault_A_Flag==0)
                    {
                        sys.Pump_A_ON 	= 1;
                        config.Hand_Auto_timer_Flag =RUNMODE_MANUAL;
                    }
                }
                break;
            case RFCODE_PUMP_B_STOP:
                Speak_Con(0);
                if(sys.Record_Dis_Flag==1)
                {
                    sys.Record_Dis_Flag = 0;
                }
                else
                {
                    sys.Pump_B_ON 	= 0;

                    config.Hand_Auto_timer_Flag =RUNMODE_MANUAL;


                    if(sys.Fault_B_Flag)
                    {
                        clear_B_Fault();

#if 0 //20260729-8 del 故障标志由统一清除函数按触点状态决定
                        sys.Fault_B_Flag = 0;
#endif
                    }
                }
  
                break;
            case RFCODE_PUMP_B_OPEN:

                Speak_Con(0);
                if(sys.Record_Dis_Flag==1)
                {
                    sys.Record_Dis_Flag = 0;
                }
                else
                {
                    if(sys.Fault_B_Flag==0)
                    {
                        sys.Pump_B_ON 	= 1;
                        config.Hand_Auto_timer_Flag =RUNMODE_MANUAL;
                    }
                }
                break;




            default:
                break;
            }
        }
    }
#endif
}

#if BF006_CONFIG_STORE_ENABLED
/******************************************************************************
 * 函数名称       : RF_Process
 * 功能描述       : 在主循环处理遥控动作，避免中断修改保存候选。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void RF_Process(void) //20261114-13 add
{
    bit interrupt_enabled;
    u8 command;
    u8 i;

    if (!rf_pending_ready) {
        return;
    }
    interrupt_enabled = EA;
    EA = 0;
    RF_Value_Rev = rf_pending;
    rf_pending_ready = 0;
    EA = interrupt_enabled;
    if (sys.RF_Key_Flag) {
        if (pipe() == 0 && Config_Panel_Available()) {
            Config_Panel_Encode(0, CONFIG_OFFSET(RF_Family), (u16)(RF_Value_Rev >> 8), 2);
            for (i = 1; i < 5; i++) {
                Config_Panel_Encode(i * 2, CONFIG_OFFSET(RF_Family) + i * 2, config.RF_Family[i - 1], 2);
            }
            Config_Panel_Submit(10);
        }
        sys.RF_Key_Flag = 0;
        Speak_Con(2);
        return;
    }
    if (!pipe()) {
        return;
    }
    command = (u8)RF_Value_Rev & 0x0F;
    if (command != RFCODE_PUMP_A_STOP && command != RFCODE_PUMP_A_OPEN) {
        return;
    }
    Speak_Con(0);
    if (sys.Record_Dis_Flag) {
        sys.Record_Dis_Flag = 0;
        return;
    }
    if (command == RFCODE_PUMP_A_STOP) {
        sys.Pump_A_ON = 0;
        sys.Pump_Flag = 0;
        RELAY1 = 0;
        sys.Pump_A_state = 0;
        if (config.Hand_Auto_timer_Flag != RUNMODE_MANUAL) {
            config.Hand_Auto_timer_Flag = RUNMODE_MANUAL;
            Config_Observe_Changes();
        }
        if (sys.Fault_A_Flag) {
            clear_A_Fault();
        }
    } else if (config_trusted && Config_Panel_Available() && !sys.Fault_A_Flag &&
               config.Hand_Auto_timer_Flag == RUNMODE_MANUAL) {
        sys.Pump_A_ON = 1;
    }
}
#endif

#endif



