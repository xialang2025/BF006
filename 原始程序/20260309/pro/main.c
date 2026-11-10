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
#include "updata4G.h"

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
    updata4G_init();
    EA = 1; // 允许CPU 全局中断
    RS485_EN  = 0;
	Send_dat.updataAll = 1;
    Send_dat.GuZhangJiLu = 0;
    while(1)
    {
#if (Function_RS485 == 1)
        Modbus_analysis();
#endif

        Analyse_4G();

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

            autoZhending();
            if(Boot_Flag)
            {			
				Updata_4G();
			}
        }

    }
}




