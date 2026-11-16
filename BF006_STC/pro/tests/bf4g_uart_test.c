#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uart4.h"
#include "configPar.h"

u8 S4CON, S4BUF, ACC, IE2, P_SW2, T4T3M, TH4, TL4, TH3, TL3, AUXINTIF, EA;
config_t config;

void UART4_int(void);
void UART4_Timer3_int(void);

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "%d: %s\n", __LINE__, #expr); exit(1); } } while (0)

static const u8 info_request[] = {0x01, 0x43, 0xbf, 0x06, 0x03, 0x01,
                                  0x00, 0x01, 0x00, 0x49, 0x84};

/******************************************************************************
 * 函数名称       : receive_byte
 * 功能描述       : 通过串口接收中断输入一个完整字节。
 * 输入参数       : value，接收字节。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void receive_byte(u8 value) //20261116-0 add
{
    S4BUF = value;
    S4CON |= 1;
    UART4_int();
}

/******************************************************************************
 * 函数名称       : timer_expire
 * 功能描述       : 模拟定时器三达到重装时长后的溢出中断。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void timer_expire(void) //20261116-0 add
{
    CHECK(T4T3M & 0x08);
    AUXINTIF |= 2;
    UART4_Timer3_int();
}

/******************************************************************************
 * 函数名称       : send_request
 * 功能描述       : 经接收中断输入请求，模拟静默并处理主循环任务。
 * 输入参数       : frame，报文；length，字节数。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void send_request(const u8 *frame, u8 length) //20261116-0 add
{
    u8 i;

    for (i = 0; i < length; i++) {
        receive_byte(frame[i]);
        uart4_service();
        CHECK(!uart4_sending());
    }
    CHECK(((u16)TH3 << 8 | TL3) == 65536UL - MAIN_Fosc / 12 / 100);
    timer_expire();
    uart4_service();
}

/******************************************************************************
 * 函数名称       : collect_reply
 * 功能描述       : 模拟发送中断逐字节收集应答，检查最后停止位的等待。
 * 输入参数       : out，收集位置；length，应答长度；interfere，途中是否注入字节。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void collect_reply(u8 *out, u8 length, u8 interfere) //20261116-0 add
{
    u8 i;

    for (i = 0; i < length; i++) {
        CHECK(uart4_sending());
        out[i] = S4BUF;
        if (i == 2 && interfere) {
            receive_byte(0x01);
            receive_byte(0x43);
        }
        S4CON |= 2;
        UART4_int();
    }
    CHECK(uart4_sending());
    CHECK(65536UL - ((u16)TH3 << 8 | TL3) > MAIN_Fosc / 12 / 9600);
    timer_expire();
    CHECK(!uart4_sending());
}

/******************************************************************************
 * 函数名称       : crc
 * 功能描述       : 独立按从高位到低位反馈的等价算法校验报文。
 * 输入参数       : frame，报文；length，计算字节数。
 * 返回值         : 低字节先传的校验值。
 * 日期            : 2026.11.16
 ******************************************************************************/
static u16 crc(const u8 *frame, u8 length) //20261116-0 add
{
    u16 state = 0xffff;
    u16 result = 0;
    u8 mask;
    u8 i;

    for (i = 0; i < length; i++) {
        for (mask = 1; mask; mask <<= 1) {
            if (((state >> 15) & 1) != ((frame[i] & mask) != 0)) {
                state = (state << 1) ^ 0x8005;
            } else {
                state <<= 1;
            }
        }
    }
    for (i = 0; i < 16; i++) {
        result = (result << 1) | (state & 1);
        state >>= 1;
    }
    return result;
}

/******************************************************************************
 * 函数名称       : fill_crc
 * 功能描述       : 补齐测试请求的校验字节。
 * 输入参数       : frame，报文；length，含校验的总长度。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void fill_crc(u8 *frame, u8 length) //20261116-0 add
{
    u16 check = crc(frame, length - 2);

    frame[length - 2] = check;
    frame[length - 1] = check >> 8;
}

/******************************************************************************
 * 函数名称       : check_info
 * 功能描述       : 逐字段检查设备信息和校验值。
 * 输入参数       : interfere，是否模拟发送期收到字节。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_info(u8 interfere) //20261116-0 add
{
    u8 reply[27];
    static const u8 expected[] = {
        0x01, 0x43, 0xbf, 0x06, 0x03, 0x81, 0x00, 0x01, 0x10,
        0x00, 0x00, 0xa3, 0x93, 0x00, 0xc9, 0x01, 0x02, 0x00,
        0xdc, 0x00, 0x00, 0x61, 0x10, 0x09, 0x08
    };

    send_request(info_request, sizeof(info_request));
    collect_reply(reply, sizeof(reply), interfere);
    CHECK(memcmp(reply, expected, sizeof(expected)) == 0);
    CHECK(crc(reply, sizeof(reply)) == 0);
}

/******************************************************************************
 * 函数名称       : check_error
 * 功能描述       : 核对错误应答合同和请求回显。
 * 输入参数       : request，请求；length，长度；status，预期错误码。
 * 返回值         : None
 * 日期            : 2026.11.16
 ******************************************************************************/
static void check_error(u8 *request, u8 length, u8 status) //20261116-0 add
{
    u8 reply[21];
    u8 i;

    fill_crc(request, length);
    send_request(request, length);
    collect_reply(reply, sizeof(reply), 0);
    CHECK(memcmp(reply, request, 4) == 0);
    CHECK(reply[4] == 3 && reply[5] == (request[5] | 0x80));
    CHECK(reply[6] == request[6] && reply[7] == request[7]);
    CHECK(reply[8] == 10 && reply[9] == status && reply[10] == 0xff);
    for (i = 11; i < 19; i++) {
        CHECK(reply[i] == 0);
    }
    CHECK(crc(reply, sizeof(reply)) == 0);
}

/******************************************************************************
 * 函数名称       : main
 * 功能描述       : 从串口入口验证设备信息、报文破坏和静默恢复。
 * 输入参数       : None
 * 返回值         : 测试退出码。
 * 日期            : 2026.11.16
 ******************************************************************************/
int main(void) //20261116-0 add
{
    u8 bad[98];
    u8 i;

    config.Dev_Address = 1;
    uart_init();
    CHECK((S4CON & 0xf0) == 0x50);
    CHECK((P_SW2 & 4) != 0);
    CHECK(((u16)TH4 << 8 | TL4) == 65536UL - MAIN_Fosc / 4 / 9600);
    check_info(0);
    check_info(0);                      // 相同关联号仍正常应答。
    check_info(1);
    send_request(info_request, sizeof(info_request)); // 发送期输入后的尾部仍丢弃。
    CHECK(!uart4_sending());
    check_info(0);

    memcpy(bad, info_request, sizeof(info_request));
    bad[10] ^= 1;
    send_request(bad, sizeof(info_request));
    CHECK(!uart4_sending());
    send_request(info_request, sizeof(info_request) - 1);
    CHECK(!uart4_sending());
    bad[0] = 2;
    fill_crc(bad, sizeof(info_request));
    send_request(bad, sizeof(info_request));
    CHECK(!uart4_sending());

    memcpy(bad, info_request, sizeof(info_request));
    for (i = sizeof(info_request); i < sizeof(bad); i++) {
        bad[i] = 0;
    }
    send_request(bad, sizeof(bad));     // 超长帧拥有合法前缀也不能应答。
    CHECK(!uart4_sending());
    check_info(0);

    memcpy(bad, info_request, sizeof(info_request));
    bad[4] = 2;
    check_error(bad, sizeof(info_request), 2);
    bad[8] = 1;
    fill_crc(bad, sizeof(info_request));
    send_request(bad, sizeof(info_request));
    CHECK(!uart4_sending());
    bad[4] = 3;
    check_error(bad, sizeof(info_request), 3);
    bad[9] = 0x42;
    check_error(bad, 12, 3);
    bad[8] = 0;
    bad[7] = 0;
    check_error(bad, sizeof(info_request), 3);
    bad[6] = 0x12;
    bad[7] = 0x34;
    bad[5] = 0x35;
    check_error(bad, sizeof(info_request), 1);
    bad[5] = 0x81;
    fill_crc(bad, sizeof(info_request));
    send_request(bad, sizeof(info_request));
    CHECK(!uart4_sending());
    check_info(0);

    puts("BF4G_UART_TEST=PASS");
    return 0;
}
