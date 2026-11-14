#include "system.h"
#include "uart4.h"
#include "HT1622.h"
#include "modbus.h"
#include "snapshot4g.h"
#include "Display.h"
#include "ad.h"

volatile u8 xdata Test_Result _at_ 0x0BFE;

static const u8 code request[] = {0x01, 0x43, 0xBF, 0x06, 1, 1, 0, 2, 0xB0, 0x31};
static const u8 code expected_single_phase[] = {
    0x01, 0x43, 0xBF, 0x06, 0x01, 0x01, 0x00, 0x02, 0x00, 0x00, 0x4D,
    0x0F, 0xFF, 0xFF, 0xF3, 0x0F, 0xFF, 0xF7, 0xF3, 0x00, 0xDC, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0F, 0x3F, 0x01, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0A, 0x00, 0x05, 0x00, 0x09, 0x00, 0x00, 0x00, 0x96, 0x01,
    0x04, 0x00, 0x64, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x1E, 0xA2, 0x93, 0x00,
    0xC9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x60, 0xE6,
};

static const u8 code expected_three_phase[] = {
    0x01, 0x43, 0xBF, 0x06, 0x01, 0x01, 0x00, 0x02, 0x00, 0x00, 0x4D,
    0x0F, 0xFF, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF, 0xFF, 0x01, 0x7C, 0x00, 0x26,
    0x00, 0x28, 0x00, 0x27, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0F, 0x3F, 0x01, 0x02, 0x00, 0x00, 0x00, 0x7D, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x19, 0x00, 0x05, 0x00, 0x14, 0x00, 0x18, 0x01, 0x40, 0x01,
    0xE0, 0x00, 0x64, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x1E, 0xB2, 0x93, 0x00,
    0xC9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x3F, 0x11,
};

static u16 rs485_reply;

/******************************************************************************
 * 函数名称       : M_Uart_Set_BaudRate
 * 功能描述       : 在仿真中接收现场串口波特率切换调用。
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
 * 功能描述       : 捕获现场串口的单寄存器应答。
 * 输入参数       : frame 应答内容，length 应答长度
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void M_Uart_Send_Block(u8 *frame, u8 length)
{
    if (length == 7 && frame[0] == 1 && frame[1] == 3 && frame[2] == 2) {
        rs485_reply = ((u16)frame[3] << 8) | frame[4];
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

extern void UART4_int(void);
extern void MEASURE_Sample(void);
extern void Modbus_Receive_handle(u8 value);
extern void Modbus_Timer_handle(void);

#if (Model_OUT == Model_1C1)
static u8 xdata reply[90];
static u8 xdata frame[56];

/******************************************************************************
 * 函数名称       : Feed_UART4
 * 功能描述       : 将一个到达字节交给串口四中断。
 * 输入参数       : value 接收字节
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
static void Feed_UART4(u8 value) //20261114-2 add
{
    S4BUF = value;
    S4CON |= 1;
    UART4_int();
}

/******************************************************************************
 * 函数名称       : Wait_UART4
 * 功能描述       : 向分帧逻辑提供指定数量的一毫秒节拍。
 * 输入参数       : ticks 节拍数
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
static void Wait_UART4(u8 ticks) //20261114-2 add
{
    while (ticks) {
        UART4_Tick();
        ticks--;
    }
}

/******************************************************************************
 * 函数名称       : Test_CRC
 * 功能描述       : 以独立的测试计算核对应答循环冗余校验。
 * 输入参数       : bytes 帧内容，length 帧长
 * 返回值         : 校验值
 * 日期            : 2026.11.14
 ******************************************************************************/
static u16 Test_CRC(u8 *bytes, u8 length) //20261114-2 add
{
    u8 position;
    u16 crc;

    crc = 0xFFFF;
    while (length) {
        crc ^= *bytes++;
        for (position = 0; position < 8; position++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
        length--;
    }
    return crc;
}

/******************************************************************************
 * 函数名称       : Send_Frame
 * 功能描述       : 注入完整请求并等待结束静默。
 * 输入参数       : bytes 帧内容，length 帧长
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
static void Send_Frame(u8 *bytes, u8 length) //20261114-2 add
{
    while (length) {
        Feed_UART4(*bytes++);
        length--;
    }
    Wait_UART4(11);
}

/******************************************************************************
 * 函数名称       : Read_Reply
 * 功能描述       : 捕获实际发送寄存器的各字节并检查发送完成。
 * 输入参数       : length 预期应答字节数
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
static void Read_Reply(u8 length) //20261114-2 add
{
    u8 i;

    S4BUF = 0xA5;
    Snapshot4G_Process();
    for (i = 0; i < length; i++) {
        reply[i] = S4BUF;
        S4BUF = 0xA5;
        SET_TI4();
        UART4_int();
    }
    if (S4BUF != 0xA5 || (length && Test_CRC(reply, length) != 0)) {
        Test_Done();
    }
}

/******************************************************************************
 * 函数名称       : Sample_Zero_Current
 * 功能描述       : 注入零值转换结果并驱动采样批次发布。
 * 输入参数       : ticks 采样节拍数
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
static void Sample_Zero_ADC(u16 ticks) //20261114-2 add
{
    u16 tick;
    u8 conversion;

    for (tick = 0; tick < ticks; tick++) {
        MEASURE_timeOut();
        MEASURE_Pro();
        for (conversion = 0; conversion < 3; conversion++) {
            ADC_RES = 0;
            ADC_RESL = 0;
            MEASURE_Sample();
        }
    }
}
#endif

/******************************************************************************
 * 函数名称       : main
 * 功能描述       : 核对快照的线上字节、采样有效性和异常帧恢复。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void main(void) //20261114-2 add
{
    u8 i;
#if (Model_OUT == Model_1C1)
    u16 crc;
    u8 invalid_case;
    u8 size;
#endif

    sys_init();
    memset(&config, 0, sizeof(config));
    uart_init();
    config.Dev_Address = 1;
    config.Sensor_Rang = 10;
    config.Sensor_ON1 = 5;
    config.Sensor_STOP = 9;
    config.UVP_Value = 150;
    config.OVP_Value = 260;
    config.Set_OVERLOAD_Current1 = 100;
    config.AUTO_Timing_Run = 60;
    config.AUTO_Timing_Stop = 30;
    sys.Voltage_Line = 220;
#if (Model_OUT == Model_1C1)
    MEASURE_init();
    Sample_Zero_ADC(512);
    sys.snapshot_adc_valid |= SNAPSHOT_VOLTAGE_VALID;
#endif
    sys.shang_shuiwei = 1;
    sys.xia_shuiwei = 2;
    LCD_WATER_Hlevel1 = LCD_WATER_Hlevel2 = LCD_WATER_Hlevel3 = LCD_WATER_Hlevel4 = 1;
    LCD_WATER_Hlevel5 = LCD_WATER_Hlevel6 = 0;
    LCD_WATER_Llevel1 = LCD_WATER_Llevel2 = LCD_WATER_Llevel3 = 1;
    LCD_WATER_Llevel4 = LCD_WATER_Llevel5 = LCD_WATER_Llevel6 = 1;

#if (Model_OUT == Model_1C1)
    Test_Result = 1;
#if (Model_Voltage == Model_3PH)
    config.Auto_Mode = SENSORTYPE_WATER4_20MA;
    sys.Auto_Mode = SENSORTYPE_WATER4_20MA;
    sys.sensor_value_valid = 1;
    sys.SensorOut = 125;
    sys.Voltage_Line = 380;
    sys.Current_A1 = 38;
    sys.Current_B1 = 40;
    sys.Current_C1 = 39;
    sys.Current_A2 = 99;
    sys.Pump_A_state = 1;
    config.Sensor_Rang = 25;
    config.Sensor_STOP = 20;
    config.Sensor_overFlow = 24;
    config.UVP_Value = 320;
    config.OVP_Value = 480;
#endif
    Send_Frame(request, sizeof(request));
    Read_Reply(90);
    for (i = 0; i < sizeof(expected_single_phase); i++) {
#if (Model_Voltage == Model_3PH)
        if (reply[i] != expected_three_phase[i]) {
#else
        if (reply[i] != expected_single_phase[i]) {
#endif
            Test_Done();
        }
    }

    Test_Result = 2;
    sys.snapshot_adc_valid = 0;
    config.Auto_Mode = SENSORTYPE_PRESS0_5V;
    config.Sensor_Mode = CTROLTYPE_DRAIN;
    sys.Fault_Error = 0x1004UL;
    sys.shang_shuiwei = 2;
    Dis8_Flash = 1;
    Send_Frame(request, sizeof(request));
    Read_Reply(90);
    if ((reply[18] & 0x0F) || (reply[17] & 8) || reply[19] != 0xFF ||
        reply[22] != 0xFF || reply[42] != 0xFF || reply[43] != 1 || reply[47] != 1 ||
        reply[46] != SENSORTYPE_PRESS0_5V || reply[45] != 24 ||
        reply[30] != 3 || reply[33] != 0x10 || reply[34] != 4) {
        Test_Done();
    }

    Test_Result = 4;
    Sample_Zero_ADC(2200);
    Send_Frame(request, sizeof(request));
    Read_Reply(90);
    if ((reply[18] & 3) != 3 || reply[19] || reply[20] || reply[21] || reply[22]) {
        Test_Done();
    }

    Test_Result = 3;
    config.Dev_Address = 0x27;
    memcpy(frame, request, sizeof(request));
    frame[0] = 0x27;
    frame[4] = 2;
    crc = Test_CRC(frame, 8);
    frame[8] = (u8)crc;
    frame[9] = crc >> 8;
    Send_Frame(frame, sizeof(request));
    Read_Reply(13);
    if (reply[0] != 0x27 || reply[4] != 2 || reply[7] != 2 || reply[8] != 1) {
        Test_Done();
    }
    config.Dev_Address = 1;

    for (invalid_case = 0; invalid_case < 6; invalid_case++) {
        Test_Result = 10 + invalid_case;
        memcpy(frame, request, sizeof(request));
        switch (invalid_case) {
        case 0:
            frame[0] = 0;
            break;
        case 1:
            frame[0] = 0xFF;
            break;
        case 2:
            frame[0] = 2;
            break;
        case 3:
            frame[2] = 0;
            break;
        case 4:
            frame[1] = 6;
            break;
        }
        crc = Test_CRC(frame, 8);
        frame[8] = (u8)crc;
        frame[9] = (u8)(crc >> 8);
        if (invalid_case == 5) {
            frame[9] ^= 1;
        }
        Send_Frame(frame, sizeof(request));
        Read_Reply(0);
    }

    Test_Result = 20;
    memset(frame, 0, sizeof(frame));
    memcpy(frame, request, 8);
    frame[5] = 0x12;
    frame[9] = 43;
    crc = Test_CRC(frame, 53);
    frame[53] = (u8)crc;
    frame[54] = crc >> 8;
    Send_Frame(frame, 55);
    Read_Reply(13);
    if (reply[5] != 0x12 || reply[8] != 2 || reply[9] || reply[10]) {
        Test_Done();
    }
    for (i = 0; i < 56; i++) {
        Feed_UART4(frame[i]);
    }
    Wait_UART4(10);
    Send_Frame(request, sizeof(request));
    Read_Reply(0);

    Test_Result = 21;
    frame[9] = 44;
    for (i = 0; i < 10; i++) {
        Feed_UART4(frame[i]);
    }
    Wait_UART4(10);
    Send_Frame(request, sizeof(request));
    Read_Reply(0);

    Test_Result = 22;
    memset(&RecData, 0, sizeof(RecData));
    frame[0] = 1;
    frame[1] = 3;
    frame[2] = 0x21;
    frame[3] = 3;
    frame[4] = 0;
    frame[5] = 1;
    crc = Test_CRC(frame, 6);
    frame[6] = (u8)crc;
    frame[7] = crc >> 8;
    for (i = 0; i < sizeof(request); i++) {
        Feed_UART4(request[i]);
        if (i < 8) {
            Modbus_Receive_handle(frame[i]);
        }
    }
    for (size = 0; size < 11; size++) {
        UART4_Tick();
        Modbus_Timer_handle();
    }
    Modbus_analysis();
    Read_Reply(90);
    if (rs485_reply != sys.Voltage_Line || reply[8] != 0 || reply[10] != 77) {
        Test_Done();
    }

    Test_Result = 23;
    for (i = 0; i < 4; i++) {
        Feed_UART4(request[i]);
    }
    Wait_UART4(11);
    for (i = 4; i < sizeof(request); i++) {
        Feed_UART4(request[i]);
    }
    Read_Reply(0);
    Wait_UART4(11);
    Send_Frame(request, sizeof(request));
    Read_Reply(90);

    Test_Result = 24;
    Send_Frame(request, sizeof(request));
    Snapshot4G_Process();
    if (S4BUF != request[0]) {
        Test_Done();
    }
    Feed_UART4(0xAA);
    for (i = 1; i < sizeof(reply); i++) {
        SET_TI4();
        UART4_int();
        if (S4BUF != reply[i]) {
            Test_Done();
        }
    }
    SET_TI4();
    UART4_int();
    Wait_UART4(10);
    Send_Frame(request, sizeof(request));
    Read_Reply(0);
    Send_Frame(request, sizeof(request));
    Read_Reply(90);

    Test_Result = 25;
    for (i = 2; i <= 3; i++) {
        memcpy(frame, request, 8);
        frame[5] = i;
        frame[8] = 0;
        frame[9] = 0;
        crc = Test_CRC(frame, 10);
        frame[10] = (u8)crc;
        frame[11] = crc >> 8;
        Send_Frame(frame, 12);
        Read_Reply(13);
        if (reply[5] != i || reply[8] != 0x32 || reply[9] || reply[10]) {
            Test_Done();
        }
    }
#else
    Test_Result = 30;
    for (i = 0; i < sizeof(request); i++) {
        S4BUF = request[i];
        S4CON |= 1;
        UART4_int();
        if (RI4) {
            Test_Done();
        }
    }
    S4BUF = 0xA5;
    SET_TI4();
    UART4_int();
    if (TI4 || S4BUF != 0xA5) {
        Test_Done();
    }
#endif
    Test_Result = 0;
    Test_Done();
}
