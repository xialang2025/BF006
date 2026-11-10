
#include "HT1622.h"
#include "Display.h"

sbit SPEAK = P2 ^ 0;

const u8 LED_LIST[13] = {0x3F, 0x6, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x7, 0x7F, 0x6F, 0x71, 0x40, 0x79};

u8 Dis1_Flash = 2;
u8 Dis2_Flash = 2;
u8 Dis3_Flash = 2;
u8 Dis4_Flash = 2;
u8 Dis5_Flash = 2;
u8 Dis6_Flash = 2;
u8 Dis7_Flash = 2;
u8 Dis8_Flash = 2;

void Flash_Dis(void)
{
    static u8 Flash_Cnt = 0;
    static u8 Flash_Flag = 0;

    Flash_Cnt++;
    if (Flash_Cnt >= 5)
    {
        Flash_Cnt = 0;

        Flash_Flag++;
    }

    if (Dis1_Flash == 0)
    {
        NUM1.All = 0;
        NUM2.All = 0;
        NUM3.All = 0;
    }
    else if (Dis1_Flash == 1)
    {
        if ((Flash_Flag & 0x01) == 0)
        {
            NUM1.All = 0;
            NUM2.All = 0;
            NUM3.All = 0;
        }
    }

    if (Dis2_Flash == 0)
    {
        NUM4.All = 0;
    }
    else if (Dis2_Flash == 1)
    {
        if ((Flash_Flag & 0x01) == 0)
        {
            NUM4.All = 0;
        }
    }

    if (Dis3_Flash == 0)
    {
        NUM5.All = 0;
        NUM6.All = 0;
        NUM7.All = 0;
        NUM8.All = 0;
    }
    else if (Dis3_Flash == 1)
    {
        if ((Flash_Flag & 0x01) == 0)
        {
            NUM5.All = 0;
            NUM6.All = 0;
            NUM7.All = 0;
            NUM8.All = 0;
        }
    }

    if (Dis4_Flash == 0)
    {
        NUM9.All = 0;
        NUM10.All = 0;
        NUM11.All = 0;
    }
    else if (Dis4_Flash == 1)
    {
        if ((Flash_Flag & 0x01) == 0)
        {
            NUM9.All = 0;
            NUM10.All = 0;
            NUM11.All = 0;
        }
    }

    if (Dis5_Flash == 0)
    {
        NUM12.All = 0;
        NUM13.All = 0;
        NUM14.All = 0;
    }
    else if (Dis5_Flash == 1)
    {
        if ((Flash_Flag & 0x01) == 0)
        {
            NUM12.All = 0;
            NUM13.All = 0;
            NUM14.All = 0;
        }
    }

    if (Dis6_Flash == 0)
    {
        NUM15.All = 0;
    }
    else if (Dis6_Flash == 1)
    {
        if ((Flash_Flag & 0x01) == 0)
        {
            NUM15.All = 0;
        }
    }

    if (Dis7_Flash == 0)
    {
        LCD_WATER_Hlevel1 = 0;
        LCD_WATER_Hlevel2 = 0;
        LCD_WATER_Hlevel3 = 0;
        LCD_WATER_Hlevel4 = 0;
        LCD_WATER_Hlevel5 = 0;
        LCD_WATER_Hlevel6 = 0;
        LCD_WATER_HFLAG = 0;
    }
    else if (Dis7_Flash == 1)
    {
        if ((Flash_Flag & 0x01) == 0)
        {
            LCD_WATER_Hlevel1 = 0;
            LCD_WATER_Hlevel2 = 0;
            LCD_WATER_Hlevel3 = 0;
            LCD_WATER_Hlevel4 = 0;
            LCD_WATER_Hlevel5 = 0;
            LCD_WATER_Hlevel6 = 0;
            LCD_WATER_HFLAG = 0;
        }
    }
    if (Dis8_Flash == 0)
    {
        LCD_WATER_Llevel1 = 0;
        LCD_WATER_Llevel2 = 0;
        LCD_WATER_Llevel3 = 0;
        LCD_WATER_Llevel4 = 0;
        LCD_WATER_Llevel5 = 0;
        LCD_WATER_Llevel6 = 0;
        LCD_WATER_LFLAG = 0;
    }
    else if (Dis8_Flash == 1)
    {
        if ((Flash_Flag & 0x01) == 0)
        {
            LCD_WATER_Llevel1 = 0;
            LCD_WATER_Llevel2 = 0;
            LCD_WATER_Llevel3 = 0;
            LCD_WATER_Llevel4 = 0;
            LCD_WATER_Llevel5 = 0;
            LCD_WATER_Llevel6 = 0;
            LCD_WATER_LFLAG = 0;
        }
    }
}

void Display_Init(void)
{
    HT1622_init();
}

void Display_Update(void)
{
    START_HT1622();
    SETUP_HT1622();
    Flash_Dis();
    display();
}

void Power_Up_Dis(void)
{
    u8 i = 0;
    u8 j = 0;
    static u8 TCnt = 0;
    static u8 TFlag = 0;

    if (Boot_Flag == 1)
        return;

    Dis1_Flash = 2;
    Dis2_Flash = 2;
    Dis3_Flash = 2;
    Dis4_Flash = 2;
    Dis5_Flash = 2;
    Dis6_Flash = 2;

    LCD_VOLT_L = 1;
    LCD_VOLT_H = 1;
    LCD_ERROR_PHASE = 1;
    LCD_LOSS_A_PHASE = 1;
    LCD_LOSS_B_PHASE = 1;
    LCD_LOSS_C_PHASE = 1;
    LCD_OVERLOAD = 1;
    LCD_STANDBY = 1;
    LCD_ERROR_A_PUMP = 1;
    LCD_ERROR_B_PUMP = 1;

    LCD_WATER_Hlevel1 = 1;
    LCD_WATER_Hlevel2 = 1;
    LCD_WATER_Hlevel3 = 1;
    LCD_WATER_Hlevel4 = 1;
    LCD_WATER_Hlevel5 = 1;
    LCD_WATER_Hlevel6 = 1;
    LCD_WATER_HFLAG = 1;

    LCD_WATER_Llevel1 = 1;
    LCD_WATER_Llevel2 = 1;
    LCD_WATER_Llevel3 = 1;
    LCD_WATER_Llevel4 = 1;
    LCD_WATER_Llevel5 = 1;
    LCD_WATER_Llevel6 = 1;
    LCD_WATER_LFLAG = 1;

    LCD_HAND = 1;
    LCD_AUTO = 1;
    LCD_SETUP = 1;
    LCD_TIMER = 1;

    LCD_RUN = 1;
    LCD_STOP = 1;
    LCD_HOUR = 1;
    LCD_MPA = 1;
    LCD_MIMITER = 1;
    LCD_CELS = 1;

    LCD_SEN_WATER = 1;
    LCD_SEN_PRESS = 1;
    LCD_SEN_WATER_MA = 1;
    LCD_SEN_PRESS_MA = 1;
    LCD_SEN_PRESS_V = 1;

    LCD_PA_X23 = 1;
    LCD_PA_X52 = 1;
    LCD_PA_X53 = 1;

    LCD_PB_X29 = 1;
    LCD_PB_X50 = 1;
    LCD_PB_X51 = 1;

    LCD_V_X1 = 1;
    LCD_V_X2 = 1;
    LCD_A_X22 = 1;
    LCD_A_X27 = 1;
    LCD_A_X33 = 1;

    LCD_AA_X24 = 1;
    LCD_AB_X25 = 1;
    LCD_AC_X26 = 1;

    LCD_BA_X30 = 1;
    LCD_BB_X31 = 1;
    LCD_BC_X32 = 1;

    LCD_P1 = 1;
    LCD_P2 = 1;
    LCD_P3 = 1;
    LCD_P4 = 1;
    LCD_P5 = 1;
    LCD_P6 = 1;
    LCD_COL = 1;

    LCD_A_UNTURN = 1;
    LCD_B_UNTURN = 1;

    LCD_SINGLE_RUN = 1;
    LCD_ALL_RUN = 1;
    LCD_CALIBRATE = 1;
    LCD_RANGE = 1;
    LCD_TIMER_ON = 0;
    LCD_TIMER_OFF = 0;

    LCD_OVERFLOW = 1;

    Dis1_Flash = 2;
    Dis2_Flash = 2;
    Dis3_Flash = 2;

    NUM1.All = LED_LIST[TCnt];
    NUM2.All = LED_LIST[TCnt];
    NUM3.All = LED_LIST[TCnt];
    NUM4.All = LED_LIST[TCnt];
    NUM5.All = LED_LIST[TCnt];
    NUM6.All = LED_LIST[TCnt];
    NUM7.All = LED_LIST[TCnt];
    NUM8.All = LED_LIST[TCnt];
    NUM9.All = LED_LIST[TCnt];
    NUM10.All = LED_LIST[TCnt];
    NUM11.All = LED_LIST[TCnt];
    NUM12.All = LED_LIST[TCnt];
    NUM13.All = LED_LIST[TCnt];
    NUM14.All = LED_LIST[TCnt];
    NUM15.All = LED_LIST[TCnt];

    if (TFlag == 0)
    {
        TCnt++;
        if (TCnt >= 9)
        {
            TCnt = 9;
            TFlag = 1;

            Speak_Con(2);
        }
    }
    else
    {
        if (TCnt > 0)
        {
            TCnt--;
        }
        else
        {
            Boot_Flag = 1;
            if (config.Hand_Auto_timer_Flag == RUNMODE_TIMER)
            {
                sys.autoTimerFlag = 1;
                sys.Auto_timer_count = 0;
            }
        }
    }
}

u8 Beep_Flag = 0;
u8 Beep_Type = 0;
u16 Beep_Cnt = 0;
u8 Link_Cnt = 0;

// ∑‰√˘∆˜π‹¿Ì
void Beep(void)
{

    if (sys.beep_err)
    {
        if (Link_Cnt < 200)
        {
            Beep_Cnt++;
            if (Beep_Cnt < 2)
            {
                SPEAK = 1;
            }
            else if (Beep_Cnt < 4)
            {

                SPEAK = 0;
            }
            else
            {
                ++Link_Cnt;
                Beep_Cnt = 0;
                Beep_Flag = 0;
            }
        }
        else
        {
            SPEAK = 0;
            sys.beep_err = 0;
        }
        Beep_Flag = 0;
    }
    else if (Beep_Flag == 1)
    {
        switch (Beep_Type)
        {
        case 0:
            SPEAK = 1;
            Beep_Cnt++;
            if (Beep_Cnt > 1)
            {
                Beep_Cnt = 0;
                SPEAK = 0;
                Beep_Flag = 0;
            }
            break;
        case 1:
            Beep_Cnt++;
            if (Beep_Cnt < 1)
            {
                SPEAK = 1;
            }
            else if (Beep_Cnt < 2)
            {
                SPEAK = 0;
            }
            else if (Beep_Cnt < 3)
            {
                SPEAK = 1;
            }
            else if (Beep_Cnt < 4)
            {
                SPEAK = 0;
                Beep_Cnt = 0;
                Beep_Flag = 0;
            }
            break;
        case 2:
            SPEAK = 1;
            Beep_Cnt++;
            if (Beep_Cnt > 2)
            {
                Beep_Cnt = 0;
                Beep_Flag = 0;

                SPEAK = 0;
            }
            break;
        case 3:
            Beep_Cnt++;
            if (Beep_Cnt < 2)
            {
                SPEAK = 1;
            }
            else if (Beep_Cnt < 4)
            {
                SPEAK = 0;
            }
            else if (Beep_Cnt < 6)
            {
                SPEAK = 1;
            }
            else if (Beep_Cnt < 8)
            {
                SPEAK = 0;
            }
            else
            {
                Beep_Cnt = 0;
            }
            break;
        case 4:
            Beep_Cnt++;
            if (Beep_Cnt < 2)
            {
                SPEAK = 1;
            }
            else if (Beep_Cnt < 4)
            {

                SPEAK = 0;
            }
            else
            {
                if (++Link_Cnt > 200)
                {
                    Beep_Cnt = 0;
                    Beep_Flag = 0;
                    Link_Cnt = 0;
                    SPEAK = 0;
                }
                else
                {
                    Beep_Cnt = 0;
                }
            }
            break;
        case 5:
            Beep_Cnt++;
            if (Beep_Cnt < 10)
            {
                SPEAK = 1;
            }
            else if (Beep_Cnt < 20)
            {

                SPEAK = 0;
            }
            else
            {

                Beep_Cnt = 0;
                Beep_Flag = 0;
                SPEAK = 0;
            }
            break;
        default:
            break;
        }
    }
    else
    {

        Beep_Cnt = 0;
        Link_Cnt = 0;
        SPEAK = 0;
    }
}

// ∑‰√˘∆˜øÿ÷∆
//  type:0  ∂Ã√˘
//		 1  ∂Ã√˘¡Ω…˘
//		 2  ≥§√˘
//        3  “ª∂Ã“ª≥§
//        4  ∂Ã√˘»˝…˘
void Speak_Con(u8 type)
{
    Beep_Flag = 1;
    Beep_Type = type;
    if (type == 4)
    {
        Link_Cnt = 0;
        sys.beep_err = 1;
    }
}

void Speak_Stop(void)
{
    sys.beep_err = 0;
}
