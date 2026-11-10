#include "system.h"
#include "configPar.h"
#include "Display.h"

#define KEY_LONG_REKIT_TIME 100
// 中长按时间
#define KEY_LONG_PRESS_TIME 300
// 超长按时间
#define KEY_LONGLONG_PRESS_TIME 400

#define KEY_LONGLONGLONG_PRESS_TIME 6000

sbit KEY_OUT1 = P4 ^ 6;
sbit KEY_OUT2 = P4 ^ 5;
sbit KEY_OUT3 = P2 ^ 7;
sbit KEY_OUT4 = P2 ^ 6;

sbit KEY_IN1 = P2 ^ 5;
sbit KEY_IN2 = P2 ^ 4;

u8 KEY_status = 0;
u32 KEY_counter = 0;
u16 view = 0;
u8 Key_Code = 0;

void KeyScan(void)
{

    u8 Key_temp = 0;
    static u8 KEY_hang = 1;
    static u8 Key_Code_Temp = 0;
    static u8 rekitCnt = 0;

    static u8 tick = 0;

    Key_temp |= KEY_IN2;
    Key_temp <<= 1;
    Key_temp |= KEY_IN1;

    switch (KEY_status)
    {
    case 0:

        if ((Key_temp & 0x03) != 0x03) // 进入按键消抖
        {
            KEY_counter++;
            view = KEY_counter;
            if (KEY_counter >= 2)
            {
                KEY_counter = 0;
                KEY_status = 2;
            }
        }
        else
        {
            KEY_hang++; //	行扫描加1
            if (KEY_hang >= 5)
            {
                KEY_hang = 1;
            }

            switch (KEY_hang)
            {
            case 1:
                KEY_OUT1 = 0;
                KEY_OUT2 = 1;
                KEY_OUT3 = 1;
                KEY_OUT4 = 1;
                break;
            case 2:
                KEY_OUT1 = 1;
                KEY_OUT2 = 0;
                KEY_OUT3 = 1;
                KEY_OUT4 = 1;
                break;
            case 3:
                KEY_OUT1 = 1;
                KEY_OUT2 = 1;
                KEY_OUT3 = 0;
                KEY_OUT4 = 1;
                break;
            case 4:
                KEY_OUT1 = 1;
                KEY_OUT2 = 1;
                KEY_OUT3 = 1;
                KEY_OUT4 = 0;
                break;
            default:
                break;
            }

            KEY_counter = 0;
        }

        break;
    case 1:
        KEY_counter++;
        if (KEY_counter >= 10)
        {
            KEY_counter = 0;
            KEY_status = 2;
        }
        KEY_status = 2;
        break;
    case 2:
        if ((Key_temp & 0x03) != 0x03)
        {
            switch (Key_temp & 0x03)
            {
            case 0x01:
                Key_Code_Temp = KEY_hang;
                break;
            case 0x02:
                Key_Code_Temp = KEY_hang + 4;
                break;
            default:
                break;
            }
            KEY_status = 3;
        }
        else
        {
            KEY_status = 0;
        }

        break;
    case 3:
        if ((Key_temp & 0x03) == 0x03) // 松手
        {
            KEY_status = 0;
            Key_Code = Key_Code_Temp;
        }
        else
        {
            KEY_counter++;

            if (KEY_counter > (KEY_LONG_REKIT_TIME))
            {
                if ((sys.Setting_Flag == 2) && ((Key_Code_Temp == 3) || (Key_Code_Temp == 7)))
                {
                    KEY_status = 4;
                }
            }
            if (KEY_counter > (KEY_LONGLONG_PRESS_TIME))
            {

                if (((config.Lock1) || (config.Lock2)) && (Key_Code_Temp == 2))
                {
                    KEY_status = 6;
                }

                else
                {
                    KEY_status = 5;
                    Key_Code = Key_Code_Temp + 0X80;
                }
            }
        }

        break;
    case 4:
        if ((Key_temp & 0x03) == 0x03) // 松手
        {
            KEY_status = 0;
        }

        if (++rekitCnt > 5)
        {
            rekitCnt = 0;
            Key_Code = Key_Code_Temp;
        }
        break;
    case 5:
        if ((Key_temp & 0x03) == 0x03) // 松手
        {
            KEY_status = 0;
        }
        break;

    case 6:
        if ((Key_temp & 0x03) == 0x03) // 松手
        {
            KEY_status = 0;
        }

        if (KEY_counter < KEY_LONGLONGLONG_PRESS_TIME)
        {
            if (++KEY_counter == KEY_LONGLONGLONG_PRESS_TIME)
            {
                Key_Code = Key_Code_Temp + 0X80;
            }

            if (++tick > 100)
            {
                tick = 0;
                Speak_Con(0);
            }
        }

        break;
    default:
        break;
    }
}