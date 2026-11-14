#include "system.h"
#include "timer.h"
#include "gpio.h"
#include "Display.h"
#include "key.h"
#include "ad.h"
#include "user.h"
#include "configPar.h"
#include "RS485.h"
#include "modbus.h"
#include "uart4.h" //20261114-2 change 由主函数直接配置串口四
#include "snapshot4g.h" //20261114-2 add
#include "config_store.h" //20261114-4 add

/******************** 主函数 **************************/
void main(void)
{
    u8 run_miao = 0;


    GPIO_Init();
    delay_ms(20);
    Display_Init();
    Power_Up_Dis();
    MEASURE_init();
    Time0_init();
    Time1_init();
#if (Function_RS485 == 1)	
    rs485_Init();
#endif	
    sys_init();
    Par_Init();
    uart_init(); //20261113-0 change 在开放全局中断前完成串口四配置
    EA = 1; // 允许CPU 全局中断
    RS485_EN  = 0;
#if 0 //20261113-0 del 封存旧协议的上电上报标志。
	Send_dat.updataAll = 1;
    Send_dat.GuZhangJiLu = 0;
#endif
    while(1)
    {
#if (Function_RS485 == 1)
        Modbus_analysis();
#endif

        //模拟量采集
//		MEASURE_Pro();

        if(Timer_1ms_Flag)
        {
            Timer_1ms_Flag = 0;
        }
        if(Scan_Flag)
        {
            Scan_Flag = 0;

            if(Boot_Flag)
            {
                KeyScan();
            }
        }

        if(Dis_Flag==1)
        {
            Dis_Flag = 0;
			P54 = 1;
            SW_Read();
            shuiwei_check();
            Contact_Overload_Control(); //20260729-5 add 在启停控制前执行触点过载保护

            if(Boot_Flag)
            {
                LosePhase_Check();
                RELAY_Auto_Control();
                Sys_Control();
                Timer100ms_pro();
                KEY_Control();
                DIS_Control();
            }

            Display_Update();
            Beep();
#if BF006_CONFIG_STORE_ENABLED
            Config_Observe_Changes();
#endif
            save_pro();
			P54 = 0;
        }

        if(Timer_1s_Flag)
        {
            Timer_1s_Flag = 0;
            Timer_mode();
            FanXiu_pro();
            //启动轮查
            Power_Up_Dis();
            Fault_Control();
            Customer_Test_Limit_Handle(); //20260501-0 add customer test time-limit fault check

            if(sys.Record_Dis_Flag==1)
            {
                sys.Record_Timer++;
                if(sys.Record_Timer>=50)
                {
                    sys.Record_Timer = 0;
                    sys.Record_Dis_Flag = 0;
                }
            }
            else
            {
                sys.Record_Timer = 0;
            }

            if(RELAY1||RELAY2)
            {
                if(++ run_miao > 59)
                {
                    run_miao = 0;
                    if(++config.run_fen > 59)
                    {
                        config.run_fen = 0;
                        if(++config.run_xiaoshi > 9999)
                        {
                            config.run_xiaoshi = 0;
                        }
                        sys.save_flag = 1;
                    }
                }
            }

            Total_Time_Second_Handle((RELAY1 || RELAY2) ? 1 : 0); //20260430-3 add maintain P-21/P-22 total time

            autoZhending();
#if 0 //20261113-0 del 封存旧协议每秒执行的上报入口。
            if(Boot_Flag)
            {			
				Updata_4G();
			}
#endif
        }

#if (Model_OUT == Model_1C1)
        Snapshot4G_Process(); //20261114-2 add 完成本轮运行及显示更新后才取快照
#endif
#if BF006_CONFIG_STORE_ENABLED
        Config_Background(); //20261114-4 add 在逐字节擦写之间继续处理通信及保护
#endif
    }
}




