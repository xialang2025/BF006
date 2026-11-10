#ifndef	_HT1622_H
#define	_HT1622_H

#include	"system.h"

////////////////////////
typedef struct NUM_BITS {
    u8  A:1;
    u8  B:1;
    u8  C:1;
    u8  D:1;
    u8  E:1;
    u8  F:1;
    u8  G:1;
    u8  H:1;
};

typedef union NUM_REG {
    u8	            All;
    struct NUM_BITS  Bit;
};


extern union NUM_REG NUM1;
extern union NUM_REG NUM2;
extern union NUM_REG NUM3;
extern union NUM_REG NUM4;
extern union NUM_REG NUM5;
extern union NUM_REG NUM6;
extern union NUM_REG NUM7;
extern union NUM_REG NUM8;
extern union NUM_REG NUM9;
extern union NUM_REG NUM10;
extern union NUM_REG NUM11;
extern union NUM_REG NUM12;
extern union NUM_REG NUM13;
extern union NUM_REG NUM14;
extern union NUM_REG NUM15;


extern u8 LCD_VOLT_L;
extern u8 LCD_VOLT_H;
extern u8 LCD_ERROR_PHASE;
extern u8 LCD_LOSS_A_PHASE;
extern u8 LCD_LOSS_B_PHASE;
extern u8 LCD_LOSS_C_PHASE;
extern u8 LCD_OVERLOAD;
extern u8 LCD_STANDBY;
extern u8 LCD_ERROR_A_PUMP;
extern u8 LCD_ERROR_B_PUMP;


extern u8 LCD_WATER_Hlevel1;
extern u8 LCD_WATER_Hlevel2;
extern u8 LCD_WATER_Hlevel3;
extern u8 LCD_WATER_Hlevel4;
extern u8 LCD_WATER_Hlevel5;
extern u8 LCD_WATER_Hlevel6;
extern u8 LCD_WATER_HFLAG;

extern u8 LCD_WATER_Llevel1;
extern u8 LCD_WATER_Llevel2;
extern u8 LCD_WATER_Llevel3;
extern u8 LCD_WATER_Llevel4;
extern u8 LCD_WATER_Llevel5;
extern u8 LCD_WATER_Llevel6;
extern u8 LCD_WATER_LFLAG;

extern u8 LCD_HAND;
extern u8 LCD_AUTO;
extern u8 LCD_SETUP;
extern u8 LCD_TIMER;

extern u8 LCD_RUN;
extern u8 LCD_STOP;
extern u8 LCD_HOUR;
extern u8 LCD_MPA;
extern u8 LCD_MIMITER;
extern u8 LCD_CELS;

extern u8 LCD_SEN_WATER;
extern u8 LCD_SEN_PRESS;
extern u8 LCD_SEN_WATER_MA;
extern u8 LCD_SEN_PRESS_MA;
extern u8 LCD_SEN_PRESS_V ;


extern u8 LCD_PA_X23;
extern u8 LCD_PA_X52;
extern u8 LCD_PA_X53;

extern u8 LCD_PB_X29;
extern u8 LCD_PB_X50;
extern u8 LCD_PB_X51;

extern u8 LCD_V_X1;
extern u8 LCD_V_X2;
extern u8 LCD_A_X22;
extern u8 LCD_A_X27;
extern u8 LCD_A_X33;

extern u8 LCD_AA_X24;
extern u8 LCD_AB_X25;
extern u8 LCD_AC_X26;

extern u8 LCD_BA_X30;
extern u8 LCD_BB_X31;
extern u8 LCD_BC_X32;

extern u8 LCD_P1;
extern u8 LCD_P2;
extern u8 LCD_P3;
extern u8 LCD_P4;
extern u8 LCD_P5;
extern u8 LCD_P6;
extern u8 LCD_COL;

extern u8 LCD_A_UNTURN;
extern u8 LCD_B_UNTURN;

extern u8 LCD_SINGLE_RUN;
extern u8 LCD_ALL_RUN;
extern u8 LCD_CALIBRATE;
extern u8 LCD_RANGE;
extern u8 LCD_TIMER_ON;
extern u8 LCD_TIMER_OFF;

extern u8 LCD_OVERFLOW;

extern void HT1622_init(void);
extern void SETUP_HT1622(void);
extern void START_HT1622(void);
extern void display(void);



#endif