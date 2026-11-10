#ifndef __DS1302_H_
#define __DS1302_H_

#include "system.h"

//---定义ds1302使用的IO口---//
#define DSIO RE1
#define TRISIO TRISE1
#define RST  RE2
#define SCLK RE0

//---定义全局函数---//
void Ds1302Write(u8 addr, u8 dat);
u8 Ds1302Read(u8 addr);
u8 bcd2bin(u8 bv);

#endif