#include "system.h"
#include "ad.h"
#include "user.h"
#include "Display.h"
#include "HT1622.h"
#include "modbus.h"

volatile u8 xdata Test_Result _at_ 0x0BFE;
static u16 reply_value;

/******************************************************************************
 * 函数名称       : M_Uart_Set_BaudRate
 * 功能描述       : 在仿真中接收波特率切换调用。
 * 输入参数       : baudrate 波特率选项
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void M_Uart_Set_BaudRate(char baudrate)
{
    baudrate = baudrate;
}

/******************************************************************************
 * 函数名称       : M_Uart_Send_Block
 * 功能描述       : 捕获现场串口发出的单寄存器应答。
 * 输入参数       : frame 应答内容，length 应答长度
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void M_Uart_Send_Block(u8 *frame, u8 length)
{
    if (length == 7 && frame[0] == 1 && frame[1] == 3 && frame[2] == 2) {
        reply_value = ((u16)frame[3] << 8) | frame[4];
    }
}

/******************************************************************************
 * 函数名称       : Test_Done
 * 功能描述       : 停在仿真器可读取测试结果的位置。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void Test_Done(void)
{
    while (1) {
    }
}

extern void MEASURE_Sample(void);
extern void Modbus_Receive_handle(u8 value);
extern void Modbus_Timer_handle(void);

/******************************************************************************
 * 函数名称       : Read_Register
 * 功能描述       : 通过现场串口接收、分帧和应答流程读取单个寄存器。
 * 输入参数       : address 寄存器地址
 * 返回值         : 收到的寄存器数值
 * 日期            : 2026.11.14
 ******************************************************************************/
static u16 Read_Register(u16 address) //20261114-1 add
{
    u8 frame[6];
    u16 crc;
    u8 index;
    u8 position;

    frame[0] = 1;
    frame[1] = 3;
    frame[2] = address >> 8;
    frame[3] = address;
    frame[4] = 0;
    frame[5] = 1;
    crc = 0xFFFF;
    for (index = 0; index < sizeof(frame); index++) {
        Modbus_Receive_handle(frame[index]);
        crc ^= frame[index];
        for (position = 0; position < 8; position++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    Modbus_Receive_handle((u8)crc);
    Modbus_Receive_handle((u8)(crc >> 8));
    for (index = 0; index < 30; index++) {
        Modbus_Timer_handle();
    }
    reply_value = 0xBEEF;
    Modbus_analysis();
    return reply_value;
}

/******************************************************************************
 * 函数名称       : Sample_Sensors
 * 功能描述       : 从转换寄存器注入一批传感器采样。
 * 输入参数       : current 电流输入计数，voltage 电压输入计数
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
static void Sample_Sensors(u16 current, u16 voltage) //20261114-1 add
{
    u16 tick;
    u16 sample;
    u8 conversion;
    u8 channel;

    for (tick = 0; tick < 512; tick++) {
        MEASURE_timeOut();
        MEASURE_Pro();
        for (conversion = 0; conversion < 3; conversion++) {
            channel = ADC_CONTR & 0x0F;
            sample = 0;
            if (channel == 9) {
                sample = current << 2;
            } else if (channel == 8) {
                sample = voltage << 2;
            }
            ADC_RES = sample >> 8;
            ADC_RESL = sample;
            MEASURE_Sample();
        }
    }
    shuiwei_check();
}

/******************************************************************************
 * 函数名称       : main
 * 功能描述       : 核对连续量从转换计数到控制、寄存器和面板的结果。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void main(void) //20261114-1 add
{
    u16 reading_address;

#if (MODBUS_USE_OLD_PROTOCOL == 1)
    reading_address = 9;
#else
    reading_address = 0x2109;
#endif
    sys_init();
    memset(&RecData, 0, sizeof(RecData));
    config.Dev_Address = 1;
    config.Sensor_Rang = 6553;
    config.Sensor_STOP = 6554;
    config.Sensor_ON1 = 9999;
    config.Sensor_ON2 = 9999;
    updatSysdata();
    Test_Result = 1;
#if (Model_OUT == Model_1C2)
    if (sys.Sensor_Rang != 65530U || sys.Sensor_STOP != 4U ||
        sys.Sensor_ON1 != 34454U || sizeof(sys.SensorOut) != sizeof(u16)) {
        Test_Done();
    }
    Test_Result = 0;
    Test_Done();
#endif
    if (sys.Sensor_Rang != 65530UL || sys.Sensor_STOP != 65540UL ||
        sys.Sensor_ON1 != 99990UL || sys.Sensor_ON2 != 99990UL) {
        Test_Done();
    }

    MEASURE_init();
    config.Sensor_Mode = CTROLTYPE_PUMP;
    config.Hand_Auto_timer_Flag = RUNMODE_AUTO;
    config.Auto_Mode = SENSORTYPE_PRESS0_5V;
    SW_Read();
    shuiwei_check();
    RELAY_Auto_Control();
    Test_Result = 2;
    if (sys.sensor_err || sys.Pump_Flag) {
        Test_Done();
    }

    Sample_Sensors(0, 2);
    RELAY_Auto_Control();
    Test_Result = 3;
    if (sys.sensor_err) {
        Test_Done();
    }
    Test_Result = 4;
    if (sys.SensorOut != 0 || sys.Pump_Flag != 1) {
        Test_Done();
    }

    config.Auto_Mode = SENSORTYPE_WATER4_20MA;
    config.Sensor_Rang = 9876;
    config.Sensor_ON1 = 1234;
    config.Sensor_STOP = 6554;
    config.Sensor_overFlow = 6554;
    updatSysdata();
    Test_Result = 8;
    if (Read_Register(reading_address) != 0xFFFF) {
        Test_Done();
    }
    SW_Read();
    shuiwei_check();
    if (Read_Register(reading_address) != 0xFFFF) {
        Test_Done();
    }
    Sample_Sensors(982, 2);
    RELAY_Auto_Control();
    Test_Result = 5;
    if (sys.SensorOut != 98760UL || sys.Pump_Flag || !sys.Sensor_OverFlow) {
        Test_Done();
    }
    Test_Result = 6;
#if (MODBUS_USE_OLD_PROTOCOL == 1)
    if (Read_Register(9) != 0xFFFF || Read_Register(8) != 0xFFFF) {
#else
    if (Read_Register(0x2109) != 0xFFFF || Read_Register(0x2110) != 0xFFFF) {
#endif
        Test_Done();
    }

    config.Sensor_Rang = 2469;
    updatSysdata();
    Sample_Sensors(589, 2);
    DIS_Control();
    Test_Result = 7;
    if (sys.SensorOut != 12345 || sys.sensor_err ||
        NUM5.All != 0x40 || NUM6.All != 0x40 || NUM7.All != 0x40 || NUM8.All != 0x40 ||
        !LCD_MIMITER || LCD_MPA || LCD_P3 || Read_Register(reading_address) != 12345) {
        Test_Done();
    }

    Test_Result = 9;
    Sample_Sensors(99, 1);
    if (!sys.sensor_err || Read_Register(reading_address) != 0xFFFF) {
        Test_Done();
    }
    Sample_Sensors(100, 2);
    if (sys.sensor_err || Read_Register(reading_address) != 0) {
        Test_Done();
    }
    Sample_Sensors(195, 10);
    if (sys.sensor_err || Read_Register(reading_address) != 0) {
        Test_Done();
    }
    Sample_Sensors(196, 11);
    if (sys.sensor_err || Read_Register(reading_address) != 0) {
        Test_Done();
    }

    Test_Result = 10;
    config.Sensor_Rang = 9999;
    updatSysdata();
    Sample_Sensors(1023, 1023);
    if (sys.SensorOut != 100117UL || Read_Register(reading_address) != 0xFFFF) {
        Test_Done();
    }
    config.Auto_Mode = SENSORTYPE_PRESS0_5V;
    SW_Read();
    Sample_Sensors(982, 1023);
    if (sys.SensorOut != 99990UL || Read_Register(reading_address) != 0xFFFF) {
        Test_Done();
    }
    Sample_Sensors(982, 1);
    if (!sys.sensor_err || Read_Register(reading_address) != 0xFFFF) {
        Test_Done();
    }
    Sample_Sensors(982, 10);
    if (sys.sensor_err || Read_Register(reading_address) != 0) {
        Test_Done();
    }
    Sample_Sensors(982, 11);
    DIS_Control();
    if (sys.sensor_err || Read_Register(reading_address) != 99 ||
        NUM5.All != LED_LIST[0] || NUM6.All != LED_LIST[0] ||
        NUM7.All != LED_LIST[9] || NUM8.All != LED_LIST[9] || !LCD_MPA || !LCD_P3) {
        Test_Done();
    }
    Test_Result = 0;
    Test_Done();
}
