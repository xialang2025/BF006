
#include "system.h"
#include "HT1622.h"


sbit  	HT1622_DATA = P4^7;
sbit 	HT1622_CS = P0^7;
sbit	HT1622_RD = P1^6;
sbit   	HT1622_WD = P1^2;

sbit   	HT1622_BLIGHT = P0^4;




#define BIAS24		0x50	//1/2偏压,4COM
#define BIAS34		0x52	//1/3偏压,4COM

#define RC256 		0x30	//系统时钟选择为片内RC振荡器
#define RC258		0x38
#define WDTDIS1		0x0a	//禁止WDT溢出标志输出	
#define TIMERDIS	0x08	//时基输出禁能
#define	SYSEN 		0x02	//打开系统时钟
#define LCDON		0x06	//打开偏压发生器
#define SYSDIS		0x00	//关闭系统时钟和偏压发生器
#define TONEON		0x12	//打开声音
#define TONEOFF		0x10	//关闭声音
#define TONE2K		0xc0	//2K
#define TONE4K		0x80


union NUM_REG NUM1;
union NUM_REG NUM2;
union NUM_REG NUM3;
union NUM_REG NUM4;
union NUM_REG NUM5;
union NUM_REG NUM6;
union NUM_REG NUM7;
union NUM_REG NUM8;
union NUM_REG NUM9;
union NUM_REG NUM10;
union NUM_REG NUM11;
union NUM_REG NUM12;
union NUM_REG NUM13;
union NUM_REG NUM14;
union NUM_REG NUM15;


u8 LCD_VOLT_L = 0;
u8 LCD_VOLT_H = 0;
u8 LCD_ERROR_PHASE = 0;
u8 LCD_LOSS_A_PHASE = 0;
u8 LCD_LOSS_B_PHASE = 0;
u8 LCD_LOSS_C_PHASE = 0;
u8 LCD_OVERLOAD = 0;
u8 LCD_STANDBY = 0;
u8 LCD_ERROR_A_PUMP = 0;
u8 LCD_ERROR_B_PUMP = 0;


u8 LCD_WATER_Hlevel1 = 0;
u8 LCD_WATER_Hlevel2 = 0;
u8 LCD_WATER_Hlevel3 = 0;
u8 LCD_WATER_Hlevel4 = 0;
u8 LCD_WATER_Hlevel5 = 0;
u8 LCD_WATER_Hlevel6 = 0;
u8 LCD_WATER_HFLAG = 0;

u8 LCD_WATER_Llevel1 = 0;
u8 LCD_WATER_Llevel2 = 0;
u8 LCD_WATER_Llevel3 = 0;
u8 LCD_WATER_Llevel4 = 0;
u8 LCD_WATER_Llevel5 = 0;
u8 LCD_WATER_Llevel6 = 0;
u8 LCD_WATER_LFLAG = 0;

u8 LCD_HAND = 0;
u8 LCD_AUTO = 0;
u8 LCD_SETUP = 0;
u8 LCD_TIMER = 0;

u8 LCD_RUN = 0;
u8 LCD_STOP = 0;
u8 LCD_HOUR = 0;
u8 LCD_MPA = 0;
u8 LCD_MIMITER = 0;
u8 LCD_CELS = 0;

u8 LCD_SEN_WATER = 0;
u8 LCD_SEN_PRESS = 0;
u8 LCD_SEN_WATER_MA = 0;
u8 LCD_SEN_PRESS_MA = 0;
u8 LCD_SEN_PRESS_V  = 0;


u8 LCD_PA_X23 = 0;
u8 LCD_PA_X52 = 0;
u8 LCD_PA_X53 = 0;

u8 LCD_PB_X29 = 0;
u8 LCD_PB_X50 = 0;
u8 LCD_PB_X51 = 0;

u8 LCD_V_X1 = 0;
u8 LCD_V_X2 = 0;
u8 LCD_A_X22 = 0;
u8 LCD_A_X27 = 0;
u8 LCD_A_X33 = 0;

u8 LCD_AA_X24 = 0;
u8 LCD_AB_X25 = 0;
u8 LCD_AC_X26 = 0;

u8 LCD_BA_X30 = 0;
u8 LCD_BB_X31 = 0;
u8 LCD_BC_X32 = 0;

u8 LCD_P1 = 0;
u8 LCD_P2 = 0;
u8 LCD_P3 = 0;
u8 LCD_P4 = 0;
u8 LCD_P5 = 0;
u8 LCD_P6 = 0;
u8 LCD_COL = 0;

u8 LCD_A_UNTURN = 0;
u8 LCD_B_UNTURN = 0;

u8 LCD_SINGLE_RUN = 0;
u8 LCD_ALL_RUN = 0;
u8 LCD_CALIBRATE = 0;
u8 LCD_RANGE = 0;
u8 LCD_TIMER_ON = 0;
u8 LCD_TIMER_OFF = 0;

u8 LCD_OVERFLOW= 0;


void SENDBIT(u8 dat,u8 bitcnt);
void SENDBYTE(u8 dat);


//********HT1622B初始化**********//
void START_HT1622(void)
{
//	u8 i;
    HT1622_CS=1;
    delay_us(10);
    HT1622_WD=1;
    delay_us(10);
    HT1622_DATA=1;
    delay_us(10);
    HT1622_CS=0;
    delay_us(10);
    HT1622_WD=0;
}

/***********发送命令子函数A*************/
void SENDCOMA(u8 com)	//
{
    START_HT1622();
    SENDBIT(0x80,4);
    SENDBYTE(com);
}
/***********发送命令子函数B*************/
void SENDCOMB(u8 adr)
{
    START_HT1622();
    SENDBIT(0xa0,4);	//write mode
    SENDBIT(adr,5);		//adr
}
/***********发送命令子函数C*************/


/**********按位发送数据子函数*************/
void SENDBIT(u8 dat,u8 bitcnt)
{
    u8 i =0 ;
    for(i=0; i<bitcnt; i++)
    {
        if((dat&0x80)==0x00)
        {
            HT1622_DATA=0;
        }
        else
        {
            HT1622_DATA=1;
        }
        delay_us(10);
        HT1622_WD=1;
        delay_us(10);
        HT1622_WD=0;
        delay_us(10);
        dat=dat<<1;
    }
}

/*********发送字节数据子函数*********/
void SENDBYTE(u8 dat)
{
    SENDBIT(dat,8);
}


/**********HT1622B初始化调置*************/
void SETUP_HT1622(void)
{
    SENDCOMA(BIAS34);
    SENDCOMA(RC256);
    SENDCOMA(SYSEN);
    delay_us(100);
    SENDCOMA(LCDON);
    SENDCOMA(WDTDIS1);
    SENDCOMA(TIMERDIS);
}

void HT1622_init(void)
{


    u8 i = 0;

    START_HT1622();
    SETUP_HT1622();

    SENDCOMB(0x00);	//从RAM的0x00地址开始放入数据

    for(i = 0; i < 25; i++)
    {
        SENDBYTE(0);
    }
	NUM1.All = 0;
	NUM2.All = 0;
	NUM3.All = 0;
	NUM4.All = 0;
	NUM5.All = 0;
	NUM6.All = 0;
	NUM7.All = 0;
	NUM8.All = 0;
	NUM9.All = 0;
	NUM10.All = 0;
	NUM11.All = 0;
	NUM12.All = 0;
	NUM13.All = 0;
	NUM14.All = 0;
	NUM15.All = 0;

}



/************LCD显示子函数**************/
void display(void)
{
    u8 temp = 0;
    u8 i=0;
	
	SENDCOMB(0x00);	//从RAM的0x00地址开始放入数据


    temp = (1<<0)+(LCD_SEN_WATER<<1)+(LCD_LOSS_B_PHASE<<2)+(LCD_LOSS_A_PHASE<<3)+(LCD_ERROR_PHASE<<4);
    SENDBYTE(temp);	//SEG0

    temp = (LCD_B_UNTURN<<0)+(NUM12.Bit.D<<1)+(NUM12.Bit.E<<2)+(NUM12.Bit.C<<3)+(NUM12.Bit.G<<4)+(NUM12.Bit.F<<5)+(NUM12.Bit.B<<6)+(NUM12.Bit.A<<7);
    SENDBYTE(temp);	//SEG1

    temp = (LCD_A_X33<<0)+(NUM13.Bit.D<<1)+(NUM13.Bit.E<<2)+(NUM13.Bit.C<<3)+(NUM13.Bit.G<<4)+(NUM13.Bit.F<<5)+(NUM13.Bit.B<<6)+(NUM13.Bit.A<<7);
    SENDBYTE(temp);	//SEG2		
	
    temp = (LCD_P6<<0)+(NUM14.Bit.D<<1)+(NUM14.Bit.E<<2)+(NUM14.Bit.C<<3)+(NUM14.Bit.G<<4)+(NUM14.Bit.F<<5)+(NUM14.Bit.B<<6)+(NUM14.Bit.A<<7);
    SENDBYTE(temp);	//SEG3
	
    temp = (LCD_BC_X32<<4)+(LCD_BB_X31<<5)+(LCD_BA_X30<<6)+(LCD_PB_X29<<7);
    SENDBYTE(temp);	//SEG4
	
    temp = (LCD_VOLT_H<<0)+(LCD_PA_X23<<1)+(LCD_AA_X24<<2)+(LCD_AB_X25<<3)+(LCD_AC_X26<<4)+(LCD_A_X27<<5)+(LCD_A_UNTURN<<6)+(LCD_P5<<7);
    SENDBYTE(temp);	//SEG5
	
    temp = (LCD_VOLT_L<<0)+(NUM11.Bit.A<<1)+(NUM11.Bit.B<<2)+(NUM11.Bit.F<<3)+(NUM11.Bit.G<<4)+(NUM11.Bit.C<<5)+(NUM11.Bit.E<<6)+(NUM11.Bit.D<<7);
    SENDBYTE(temp);	//SEG6	
	
    temp = (LCD_OVERLOAD<<0)+(NUM10.Bit.A<<1)+(NUM10.Bit.B<<2)+(NUM10.Bit.F<<3)+(NUM10.Bit.G<<4)+(NUM10.Bit.C<<5)+(NUM10.Bit.E<<6)+(NUM10.Bit.D<<7);
    SENDBYTE(temp);	//SEG7	
	
    temp = (LCD_A_X22<<0)+(NUM9.Bit.A<<1)+(NUM9.Bit.B<<2)+(NUM9.Bit.F<<3)+(NUM9.Bit.G<<4)+(NUM9.Bit.C<<5)+(NUM9.Bit.E<<6)+(NUM9.Bit.D<<7);
    SENDBYTE(temp);	//SEG8	
	
    temp = (LCD_V_X1<<0)+(NUM1.Bit.D<<1)+(NUM1.Bit.E<<2)+(NUM1.Bit.C<<3)+(NUM1.Bit.G<<4)+(NUM1.Bit.F<<5)+(NUM1.Bit.B<<6)+(NUM1.Bit.A<<7);
    SENDBYTE(temp);	//SEG9
	
	temp = (LCD_P1<<0)+(NUM2.Bit.D<<1)+(NUM2.Bit.E<<2)+(NUM2.Bit.C<<3)+(NUM2.Bit.G<<4)+(NUM2.Bit.F<<5)+(NUM2.Bit.B<<6)+(NUM2.Bit.A<<7);
    SENDBYTE(temp);	//SEG10	

    temp = (LCD_V_X2<<0)+(NUM3.Bit.D<<1)+(NUM3.Bit.E<<2)+(NUM3.Bit.C<<3)+(NUM3.Bit.G<<4)+(NUM3.Bit.F<<5)+(NUM3.Bit.B<<6)+(NUM3.Bit.A<<7);
    SENDBYTE(temp);	//SEG11
	
    temp = (LCD_ERROR_A_PUMP<<0)+(1<<1)+(LCD_RANGE<<2)+(LCD_ALL_RUN<<3)+(LCD_CALIBRATE<<4)+(LCD_SINGLE_RUN<<5)+(LCD_TIMER_OFF<<6)+(LCD_TIMER_ON<<7);
    SENDBYTE(temp);	//SEG12
	
    temp = (LCD_LOSS_C_PHASE<<0)+(NUM4.Bit.D<<1)+(NUM4.Bit.E<<2)+(NUM4.Bit.C<<3)+(NUM4.Bit.G<<4)+(NUM4.Bit.F<<5)+(NUM4.Bit.B<<6)+(NUM4.Bit.A<<7);
    SENDBYTE(temp);	//SEG13	
	
    temp = (LCD_P2<<0)+(NUM5.Bit.D<<1)+(NUM5.Bit.E<<2)+(NUM5.Bit.C<<3)+(NUM5.Bit.G<<4)+(NUM5.Bit.F<<5)+(NUM5.Bit.B<<6)+(NUM5.Bit.A<<7);
    SENDBYTE(temp);	//SEG14	

    temp = (LCD_P3<<0)+(NUM6.Bit.D<<1)+(NUM6.Bit.E<<2)+(NUM6.Bit.C<<3)+(NUM6.Bit.G<<4)+(NUM6.Bit.F<<5)+(NUM6.Bit.B<<6)+(NUM6.Bit.A<<7);
    SENDBYTE(temp);	//SEG15
	
    temp = (LCD_COL<<0)+(NUM7.Bit.D<<1)+(NUM7.Bit.E<<2)+(NUM7.Bit.C<<3)+(NUM7.Bit.G<<4)+(NUM7.Bit.F<<5)+(NUM7.Bit.B<<6)+(NUM7.Bit.A<<7);
    SENDBYTE(temp);	//SEG16	
	
    temp = (LCD_P4<<0)+(NUM8.Bit.D<<1)+(NUM8.Bit.E<<2)+(NUM8.Bit.C<<3)+(NUM8.Bit.G<<4)+(NUM8.Bit.F<<5)+(NUM8.Bit.B<<6)+(NUM8.Bit.A<<7);
    SENDBYTE(temp);	//SEG17		
	
    temp = (LCD_ERROR_B_PUMP<<0)+(LCD_STANDBY<<1)+(LCD_CELS<<2)+(LCD_MIMITER<<3)+(LCD_MPA<<4)+(LCD_HOUR<<5)+(LCD_STOP<<6)+(LCD_RUN<<7);
    SENDBYTE(temp);	//SEG18	

    temp = (LCD_WATER_HFLAG<<1)+(LCD_WATER_Hlevel1<<2)+(LCD_WATER_Hlevel2<<3)+(LCD_WATER_Hlevel3<<4)+(LCD_WATER_Hlevel4<<5)+(LCD_WATER_Hlevel5<<6)+(LCD_WATER_Hlevel6<<7);
    SENDBYTE(temp);	//SEG19
	
    temp = (LCD_PA_X53<<2)+(LCD_AUTO<<3)+(NUM15.Bit.E<<4)+(NUM15.Bit.G<<5)+(NUM15.Bit.F<<6)+(LCD_SETUP<<7);
    SENDBYTE(temp);	//SEG20	
	
    temp = (LCD_PA_X52<<3)+(NUM15.Bit.D<<4)+(NUM15.Bit.C<<5)+(NUM15.Bit.B<<6)+(NUM15.Bit.A<<7);
    SENDBYTE(temp);	//SEG21	

    temp = (LCD_PB_X51<<3)+(LCD_PB_X50<<4)+(LCD_HAND<<5)+(LCD_TIMER<<6)+(LCD_OVERFLOW<<7);
    SENDBYTE(temp);	//SEG22	
	
    temp = (LCD_WATER_LFLAG<<1)+(LCD_WATER_Llevel1<<2)+(LCD_WATER_Llevel2<<3)+(LCD_WATER_Llevel3<<4)+(LCD_WATER_Llevel4<<5)+(LCD_WATER_Llevel5<<6)+(LCD_WATER_Llevel6<<7);
    SENDBYTE(temp);	//SEG23
	
    temp = (LCD_SEN_PRESS<<0)+(LCD_SEN_WATER_MA<<1)+(LCD_SEN_PRESS_MA<<2)+(LCD_SEN_PRESS_V<<3);
    SENDBYTE(temp);	//SEG24	
	
    HT1622_BLIGHT = 1;

}




