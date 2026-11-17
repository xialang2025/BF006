//20261116-15 add 串口验收模拟入口，复用固件测试的寄存器和片内存储替身。
#define main bf4g_regression_main
#include "bf4g_uart_test.c"
#undef main

/******************************************************************************
 * 函数名称       : main
 * 功能描述       : 从标准输入收十六进制报文，经固件串口中断处理后返回应答。
 * 输入参数       : None
 * 返回值         : 测试退出码。
 * 日期            : 2026.11.16
 ******************************************************************************/
int main(void)
{
    char line[2 * BF4G_FRAME_MAX + 3];
    u8 request[BF4G_FRAME_MAX];
    u8 length;
    u8 sent;
    u8 ticks;

    sys_init();
    memset(flash, 0xff, sizeof(flash));
    Par_Init();
    uart_init();
    EA = 1;
    Boot_Flag = 1;
    water_status_ready = 1;
    current_sample_ready = voltage_sample_ready = 1;
    sys.Voltage_Line = 220;

    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\r\n")] = 0;
        length = decode_vector(line, request);
        if (length > 0) {
            send_request(request, length);
        }
        ticks = 0;
        while (!uart4_sending() && (T4T3M & 0x08) && length > 5 && request[5] == 0x11) {
            CHECK(++ticks <= 30);
            timer_expire();
            if (ticks % 10 == 0) {
                scan_control();
            }
            uart4_service();
        }
        sent = 0;
        while (uart4_sending() && !(T4T3M & 0x08)) {
            CHECK(sent++ < BF4G_FRAME_MAX);
            printf("%02X", (unsigned int)S4BUF);
            S4CON |= 2;
            UART4_int();
        }
        if (uart4_sending()) {
            timer_expire();
        }
        if (ticks == 0) {
            scan_control();
        }
        save_pro();
        puts("");
        fflush(stdout);
    }
    return 0;
}
