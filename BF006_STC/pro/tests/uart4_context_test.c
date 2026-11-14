#include "snapshot4g.h"
#include "uart4.h"
#include "Modbus_New.h"

volatile u8 xdata Test_Result _at_ 0x0BFE;
static u8 xdata frame[14];
static u8 xdata reply[127];
static u8 xdata baseline[77];

extern void UART4_int(void);

/******************************************************************************
 * 函数名称       : M_Uart_Send_Block
 * 功能描述       : 提供未使用的现场串口输出边界。
 * 输入参数       : bytes 帧内容，length 字节数
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void M_Uart_Send_Block(u8 *bytes, u8 length)
{
    bytes = bytes;
    length = length;
}

/******************************************************************************
 * 函数名称       : Snapshot_Test_Context
 * 功能描述       : 提供可重复的离线上下文用于核对协议编码。
 * 输入参数       : boot 代次，revision 配置版本，uptime 毫秒数
 * 返回值         : 上下文可用时返回一
 * 日期            : 2026.11.14
 ******************************************************************************/
u8 Snapshot_Test_Context(u32 *boot, u32 *revision, u32 *uptime)
{
    *boot = 1;
    *revision = 7;
    *uptime = 60000;
    return 1;
}

/******************************************************************************
 * 函数名称       : Test_Done
 * 功能描述       : 在可读取测试结果的位置停止。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void Test_Done(void)
{
    while (1) {
    }
}

/******************************************************************************
 * 函数名称       : Exchange
 * 功能描述       : 向串口四注入一帧并捕获真实发送寄存器内容。
 * 输入参数       : request_size 请求长度，reply_size 应答长度
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
static void Exchange(u8 request_size, u8 reply_size) //20261114-6 add
{
    u8 i;
    u16 crc;

    crc = BF006_Modbus_CRC(frame, request_size - 2);
    frame[request_size - 2] = (u8)crc;
    frame[request_size - 1] = (u8)(crc >> 8);
    for (i = 0; i < request_size; i++) {
        S4BUF = frame[i];
        S4CON |= 1;
        UART4_int();
    }
    for (i = 0; i < 11; i++) {
        UART4_Tick();
    }
    Snapshot4G_Process();
    for (i = 0; i < reply_size; i++) {
        reply[i] = S4BUF;
        S4BUF = 0xA5;
        SET_TI4();
        UART4_int();
    }
    if (S4BUF != 0xA5 || BF006_Modbus_CRC(reply, reply_size) != 0 ||
        reply[10] != reply_size - 13) {
        Test_Done();
    }
}

/******************************************************************************
 * 函数名称       : main
 * 功能描述       : 对照原快照逐项核对短应答和完整应答。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void main(void) //20261114-6 add
{
    static const u8 code widths[] = {2,2,2,2,2,2,4,1,1,1,1,4,1,2,1,1,2,2,2,2,2,2,2,2,2,2,2,2};
    u8 field;
    u8 mode;
    u8 i;
    u8 offset;
    u8 expected_result;
    u32 expected;
    u32 actual;

    sys_init();
    memset(&config, 0, sizeof(config));
    config.Dev_Address = 1;
    config.Sensor_Rang = 9999;
    config.Sensor_ON1 = 654;
    config.Sensor_STOP = 12345;
    config.Sensor_Mode = CTROLTYPE_DRAIN;
    config.Auto_Mode = SENSORTYPE_PRESS0_5V;
    sys.Auto_Mode = config.Auto_Mode;
    sys.sensor_value_valid = 1;
    sys.SensorOut = 99990;
    sys.Fault_Error = 0x1004UL;
    sys.snapshot_adc_valid = SNAPSHOT_CURRENT_VALID;
    uart_init();
    frame[0] = 1;
    frame[1] = 0x43;
    frame[2] = 0xBF;
    frame[3] = 6;
    frame[4] = 1;
    frame[5] = 1;
    frame[6] = 0;
    frame[7] = 8;
    Test_Result = 1;
    Exchange(10, 90);
    memcpy(baseline, &reply[11], sizeof(baseline));

    offset = 8;
    for (field = 1; field <= 28; field++) {
        Test_Result = field + 1;
        expected_result = 0;
        expected = 0;
        if (field == 1) {
            expected_result = 0x35;
        }
#if (Model_Voltage == Model_1PH)
        if (field == 3 || field == 4) {
            expected_result = 0x29;
        }
#endif
        if (!expected_result) {
            for (i = 0; i < widths[field - 1]; i++) {
                expected = (expected << 8) | baseline[offset + i];
            }
        }
        offset += widths[field - 1];
        for (mode = 0; mode <= 1; mode++) {
            frame[5] = 4;
            frame[8] = 0;
            frame[9] = 2;
            frame[10] = field;
            frame[11] = mode;
            Exchange(14, mode ? 127 : 50);
            actual = 0;
            for (i = 36; i < 40; i++) {
                actual = (actual << 8) | reply[i];
            }
            if (reply[8] != expected_result || reply[35] != field || expected != actual ||
                (reply[32] & 2) != (expected_result ? 0 : 2) ||
                (reply[32] & 16) != (mode ? 16 : 0) || reply[22] != 1 || reply[26] != 7 ||
                (mode && memcmp(&reply[48], baseline, sizeof(baseline)))) {
                Test_Done();
            }
        }
    }
    Test_Result = 30;
    frame[10] = 29;
    frame[11] = 0;
    Exchange(14, 50);
    if (reply[8] != 0x29 || reply[32] & 2) {
        Test_Done();
    }
    frame[10] = 2;
    frame[11] = 2;
    Exchange(14, 50);
    if (reply[8] != 0x33 || reply[32] & 2) {
        Test_Done();
    }
    frame[9] = 1;
    Exchange(13, 13);
    if (reply[8] != 3) {
        Test_Done();
    }
    Test_Result = 0;
    Test_Done();
}
