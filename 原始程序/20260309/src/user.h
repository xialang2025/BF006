#ifndef	_USER_H
#define	_USER_H

#include	"system.h"
typedef enum{
	
	EM_VOl = 0,
	EM_CUR ,
	EM_TIME ,
	EM_MODE ,
	EM_SENSOR ,
	EM_PUMP_MODE ,

}ParType_t;



typedef struct{
	void* pSrc;
	void* pMax;
	void* pMin;
	u8 Size;
	ParType_t type;
	
}Par_t;

extern code u16 Value_UVP;
extern code u16 Value_OVP;

extern u16 Limit_Current;

void KEY_Control(void);
void DIS_Control(void);
void SW_Read(void);
void reset_dev(u8 ty);

#endif