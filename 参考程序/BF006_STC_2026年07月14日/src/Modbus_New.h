#ifndef MODBUS_NEW_H
#define MODBUS_NEW_H

#include "system.h"
#include "Modbus.h"

#define MODBUS_NEW_ADDR_BROADCAST 0xff //20260430-0 add BF006 private broadcast address

#define MODBUS_NEW_FUNC_READ 0x03
#define MODBUS_NEW_FUNC_WRITE_ONE 0x06
#define MODBUS_NEW_FUNC_WRITE_MORE 0x10
#define MODBUS_NEW_FUNC_SET_ADDR 0x41
#define MODBUS_NEW_FUNC_CLEAR_TIME 0x42 //20260501-1 add private total-time clear command

#define MODBUS_NEW_ERR_FUNC 0x01
#define MODBUS_NEW_ERR_REG 0x02
#define MODBUS_NEW_ERR_DATA 0x03

#define MODBUS_NEW_MAX_WORDS 8
#define MODBUS_NEW_PRIVATE_REG 0xc351
#define MODBUS_NEW_PRIVATE_CONFIRM 0xa5 //20260501-1 add private command confirmation byte to avoid accidental clearing
#define MODBUS_NEW_VERSION 201 //20260713-0 change 协议版本升级，标识新增传感器与水池状态寄存器

#define MODBUS_NEW_SENSOR_VALUE_INVALID 0xffff //20260713-0 add 当前传感器没有连续数值时返回无效值
#define MODBUS_NEW_SENSOR_LEVEL_INVALID 0xffff //20260713-0 add 当前传感器没有模拟档位时返回无效值
#define MODBUS_NEW_SENSOR_UNIT_NONE 0x0000     //20260713-0 add 无连续数值单位
#define MODBUS_NEW_SENSOR_UNIT_METER 0x0001    //20260713-0 add 液位单位为米
#define MODBUS_NEW_SENSOR_UNIT_MPA 0x0002      //20260713-0 add 压力单位为兆帕

#define MODBUS_NEW_SENSOR_STATUS_ERROR 0x0001       //20260713-0 add 传感器断线状态位
#define MODBUS_NEW_SENSOR_STATUS_OVERFLOW 0x0002    //20260713-0 add 传感器测量值超限状态位
#define MODBUS_NEW_SENSOR_STATUS_FLOWOVER 0x0004    //20260713-0 add 系统溢流状态位
#define MODBUS_NEW_SENSOR_STATUS_UPPER_FLASH 0x0008 //20260713-0 add 上水池显示闪烁状态位
#define MODBUS_NEW_SENSOR_STATUS_LOWER_FLASH 0x0010 //20260713-0 add 下水池显示闪烁状态位

#define MODBUS_NEW_CLEAR_RUN_TIME 0x21   //20260501-1 add clear P-21 total run time
#define MODBUS_NEW_CLEAR_POWER_TIME 0x22 //20260501-1 add clear P-22 total power-on time

#define MODBUS_NEW_PARAM_START 0x0000
#define MODBUS_NEW_PARAM_END 0x0016 //20260430-2 change parameter area extended to P-22
#define MODBUS_NEW_CTRL_START 0x2000
#define MODBUS_NEW_CTRL_END 0x2007
#define MODBUS_NEW_MON_START 0x2100
#define MODBUS_NEW_MON_END 0x2118 //20260713-0 change 监控区扩展到新增传感器和水池状态寄存器
#define MODBUS_NEW_HIS_START 0x2200
#define MODBUS_NEW_HIS_END 0x221d

#define MODBUS_NEW_PUMP_ACTION_NONE 0x00
#define MODBUS_NEW_PUMP_ACTION_STOP 0x01
#define MODBUS_NEW_PUMP_ACTION_START 0x02
#define MODBUS_NEW_PUMP_TARGET_A 0x01
#define MODBUS_NEW_PUMP_TARGET_B 0x02
#define MODBUS_NEW_PUMP_TARGET_AB 0x03

void Modbus_Receive_handle(u8 rdata);
void Modbus_Timer_handle(void);
void Modbus_analysis(void);

#endif
