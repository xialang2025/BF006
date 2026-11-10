
#include "Modbus.h"
#include "string.h"
#include "RS485.h"
#include "HT1622.h"
#include "Display.h"

#if (Function_RS485 == 1)

#define uchar unsigned char
#define uint unsigned int

const char dev[8] = "BF006  ";
const uint version = 200;

struct Uart_Type RecData;

extern void Record_Clr_and_Reset(void);

// CRC程序
void crc_16(unsigned char *Array, unsigned char *Rcvbuf, unsigned char Len)
{
    unsigned int IX, IY, CRC1;
    CRC1 = 0xFFFF; // set all 1

    if (Len <= 0)
    {
        CRC1 = 0;
    }
    else
    {
        for (IX = 0; IX <= Len - 1; IX++)
        {
            CRC1 = CRC1 ^ (unsigned int)(Array[IX]);
            for (IY = 0; IY <= 7; IY++)
            {
                if ((CRC1 & 1) != 0)
                {
                    CRC1 = (CRC1 >> 1) ^ 0xA001;
                }
                else
                {
                    CRC1 = CRC1 >> 1; //
                }
            }
        }
    }
    Rcvbuf[0] = (CRC1 & 0xff00) >> 8; // 高位置
    Rcvbuf[1] = CRC1 & 0x00ff;        // 低位置
}

void Modbus_Receive_handle(u8 rdata)
{
    RecData.Rec_Fifo[RecData.Rec_Cnt] = rdata;
    RecData.Rec_Cnt++;
    if (RecData.Rec_Cnt >= Usart_Rec_Len)
    {
        RecData.Rec_Cnt = 0;
    }

    RecData.Rec_Idle_Time = 0;
    RecData.Rec_Start_Flag = 1;
}

void Modbus_Timer_handle(void)
{
    // 串口帧识别
    //====================================================================
    if (RecData.Rec_Start_Flag == 1) // 串口帧开始接收
    {
        RecData.Rec_Idle_Time++;
        if (RecData.Rec_Idle_Time >= Usart_Idle_Time) // 如果离线10ms
        {
            RecData.Rec_Start_Flag = 0;        // 开始接收标志清零
            RecData.Rec_End_Flag = 1;          // 置位接收完成标志
            RecData.Rec_Len = RecData.Rec_Cnt; // 统计串口帧数据长度
            RecData.Rec_Cnt = 0;

            memcpy(RecData.Rec_Data, RecData.Rec_Fifo, RecData.Rec_Len); // 将缓存的数据移到处理单元
            memset(RecData.Rec_Fifo, 0, Usart_Rec_Len);
        }
    }
}

void Modbus_RevErr_ACK(unsigned char Fun, unsigned char ack)
{
    unsigned char Send_buff[5];
    unsigned char CRC_Rst[2] = {0x00, 0x00};

    Send_buff[0] = config.Dev_Address;
    Send_buff[1] = 0x80 + Fun;
    Send_buff[2] = ack;
    crc_16(Send_buff, CRC_Rst, 3);

    Send_buff[3] = CRC_Rst[1];
    Send_buff[4] = CRC_Rst[0];
    M_Uart_Send_Block(Send_buff, 5);
}

void Modbus_RevReg(unsigned int dat)
{
    unsigned char Send_buff[12];
    unsigned char CRC_Rst[2] = {0x00, 0x00};

    Send_buff[0] = config.Dev_Address;
    Send_buff[1] = 0x03;
    Send_buff[2] = 0x02;
    Send_buff[3] = dat >> 8;
    Send_buff[4] = dat & 0xFF;

    crc_16(Send_buff, CRC_Rst, 5);

    Send_buff[5] = CRC_Rst[1];
    Send_buff[6] = CRC_Rst[0];
    M_Uart_Send_Block(Send_buff, 7);
}

extern void M_Uart_Set_BaudRate(char baudrate);

u16 get_reg(u8 addr)
{
    u16 res = 0;
    switch (addr)
    {
    case 0:
        res = (Dev_Model); // 0xB3;//
        res <<= 8;
        res += (config.Set_OVERLOAD_Current1 == V1_Set_Current ? 0x90 : (config.Set_OVERLOAD_Current1 == V2_Set_Current ? 0x91 : (config.Set_OVERLOAD_Current1 == V3_Set_Current ? 0x92 : 0x93)));
        break;

    case 1:

        res = (config.Pump_AB_Mode == 2 ? 0x01 : 0x00) +
              (LCD_WATER_Hlevel2 == 1 ? 0x02 : 0x00) +
              (LCD_WATER_Hlevel4 == 1 ? 0x04 : 0x00) +
              (LCD_WATER_Hlevel6 == 1 ? 0x08 : 0x00) +
              (LCD_WATER_Llevel2 == 1 ? 0x10 : 0x00) +
              (LCD_WATER_Llevel4 == 1 ? 0x20 : 0x00) +
              (LCD_WATER_Llevel6 == 1 ? 0x40 : 0x00) +
              (sys.Pump_Add_Flag == 1 ? 0x80 : 0x00);
        res <<= 8;
        res += ((config.Hand_Auto_timer_Flag == RUNMODE_MANUAL) ? 0x01 : 0x00) +
               ((config.Hand_Auto_timer_Flag > 0) ? 0x02 : 0x00) + ((config.Hand_Auto_timer_Flag == RUNMODE_AUTO) ? 0x04 : 0x00) +
               ((config.Hand_Auto_timer_Flag == RUNMODE_TIMER) ? 0x08 : 0x00) +
               (sys.Pump_A_state == 1 ? 0x10 : 0x00) + (sys.Pump_B_state == 1 ? 0x20 : 0x00) +
               (config.Pump_AB_Mode == 0 ? 0x40 : (config.Pump_AB_Mode == 1 ? 0x80 : 0x00));

        break;
    case 2:
        res = (sys.xia_shuiwei == 0 ? 0x02 : 0x00) + (sys.shang_shuiwei == 2 ? 0x04 : 0x00);
        res <<= 8;
#if (Model_OUT == Model_1C2)
        res += ((sys.Fault_Error & ((u32)1 << ERR_UVP)) ? 0x01 : 0x00) + ((sys.Fault_Error & ((u32)1 << ERR_OVP)) ? 0x02 : 0x00) + ((sys.Fault_Error & (((u32)1 << ERR_OVERLOAD1) + ((u32)1 << ERR_OVERLOAD2))) ? 0x04 : 0x00) + ((sys.Fault_Error & (((u32)1 << ERR_STANDBY1) + ((u32)1 << ERR_STANDBY2))) ? 0x08 : 0x00) + ((sys.Fault_Error & (((u32)1 << ERR_LOSE_A1) + ((u32)1 << ERR_LOSE_B1) + ((u32)1 << ERR_LOSE_C1) + (1 << (u32)ERR_LOSE_A2) + (1 << (u32)ERR_LOSE_B2) + (1 << (u32)ERR_LOSE_C2))) ? 0x20 : 0x00);
#else
        res += ((sys.Fault_Error & ((u32)1 << (u32)ERR_UVP)) ? 0x01 : 0x00) + ((sys.Fault_Error & (1 << (u32)ERR_OVP)) ? 0x02 : 0x00) + ((sys.Fault_Error & ((1 << (u32)ERR_OVERLOAD1) + (1 << (u32)ERR_OVERLOAD2))) ? 0x04 : 0x00) + ((sys.Fault_Error & ((1 << (u32)ERR_STANDBY1) + (1 << (u32)ERR_STANDBY2))) ? 0x08 : 0x00) + ((sys.Fault_Error & ((1 << (u32)ERR_LOSE_A1) + (1 << (u32)ERR_LOSE_B1) + (1 << (u32)ERR_LOSE_C1) + (1 << (u32)ERR_LOSE_A2) + (1 << (u32)ERR_LOSE_B2) + (1 << (u32)ERR_LOSE_C2))) ? 0x20 : 0x00);
#endif
        break;
    case 3:
        res = sys.Voltage_Line;
        break;
    case 4:
        res = sys.Current_A1;
        break;
    case 5:
        res = sys.Current_A2;
        break;
    case 6:
        res = 0;
        break;
    case 7:
        res = sys.Sensor_ON2;
        break;
    case 8:
        res = sys.Sensor_STOP;
        break;
    case 9:
        res = sys.SensorOut;
        break;

    case 10:
        res = config.AUTO_Timing_Run;
        break;

    case 11:
        res = config.AUTO_Timing_Stop;
        break;

    case 12:
        res = config.Set_STANDBY_Current1;
        break;

    case 13:
        res = config.Set_OVERLOAD_Current1;
        break;

    case 14:
        res = config.Set_OVERLOAD_Current2;
        break;

    case 15:
        res = config.AUTO_FANGXIU;
        break;

    case 16:
        res = config.Timer_Unload_Restart;
        break;

    case 17:
        res = 0;
        break;
    case 18:
        res = 0;
        break;
    case 19:
        res = config.Record_Type[0];
        break;
    case 20:
        res = config.Record_Type[1];
        break;
    case 21:
        res = config.Record_Type[2];
        break;
    case 22:
        res = config.Record_Type[3];
        break;
    case 23:
        res = config.Record_Type[4];
        break;
    case 24:
        res = config.Record_Type[5];
        break;
    case 25:
        res = config.Record_Type[6];
        break;
    case 26:
        res = config.Record_Type[7];
        break;
    case 27:
        res = config.Record_Type[8];
        break;
    case 28:
        res = config.Record_Type[9];
        break;
    case 29:
        res = dev[0];
        break;
    case 30:
        res = dev[1];
        break;
    case 31:
        res = dev[2];
        break;
    case 32:
        res = dev[3];
        break;
    case 33:
        res = version;
        break;
    }

    return res;
}

void Modbus_analysis(void)
{
    unsigned char CRC_Rst[2] = {0x00, 0x00};
    unsigned int Modbus_Read_Start_REG = 0;
    unsigned int Modbus_Read_REG_Num = 0;
    unsigned int i = 0;
    unsigned int REG_Value = 0;
    unsigned int CMD_Value = 0;
    u16 res = 0;

    if (RecData.Rec_End_Flag == 1)
    {
        RecData.Rec_End_Flag = 0;

        if (RecData.Rec_Data[0] == 0xFF) // 广播地址
        {
            // CRC16计算
            crc_16(RecData.Rec_Data, CRC_Rst, RecData.Rec_Len - 2);

            if (RecData.Rec_Data[RecData.Rec_Len - 1] == CRC_Rst[0] && RecData.Rec_Data[RecData.Rec_Len - 2] == CRC_Rst[1])
            {
                Modbus_Read_Start_REG = RecData.Rec_Data[2];
                Modbus_Read_Start_REG = (Modbus_Read_Start_REG << 8) + RecData.Rec_Data[3];

                switch (RecData.Rec_Data[1])
                {
                case 0x41:

                    if (Modbus_Read_Start_REG == 50001)
                    {
                        M_Uart_Send_Block(RecData.Rec_Data, RecData.Rec_Len);

                        config.Dev_Address = RecData.Rec_Data[4];
                        M_Uart_Set_BaudRate(RecData.Rec_Data[5]);
                        sys.save_flag = 1;
                    }
                    else
                    {
                        Modbus_RevErr_ACK(RecData.Rec_Data[1], MSB_Error_Reg);
                    }

                    break;
                case 0x03: // 读DEVID
                    if (Modbus_Read_Start_REG == 50001)
                    {
                        REG_Value = config.Dev_Address;
                        REG_Value = (REG_Value << 8) + 0;
                        Modbus_RevReg(REG_Value);
                    }
                    else
                    {
                        Modbus_RevErr_ACK(RecData.Rec_Data[1], MSB_Error_Reg);
                    }
                    break;
                default:
                    Modbus_RevErr_ACK(RecData.Rec_Data[1], MBS_Error_Addr);
                    break;
                }
            }
        }
        else if (RecData.Rec_Data[0] == config.Dev_Address) // 本机地址
        {

            // CRC16计算
            crc_16(RecData.Rec_Data, CRC_Rst, RecData.Rec_Len - 2);

            if (RecData.Rec_Data[RecData.Rec_Len - 1] == CRC_Rst[0] && RecData.Rec_Data[RecData.Rec_Len - 2] == CRC_Rst[1])
            {
                switch (RecData.Rec_Data[1])
                {
                case 0x03:

                    Modbus_Read_Start_REG = RecData.Rec_Data[2];
                    Modbus_Read_Start_REG = (Modbus_Read_Start_REG << 8) + RecData.Rec_Data[3];
                    Modbus_Read_REG_Num = RecData.Rec_Data[4];
                    Modbus_Read_REG_Num = (Modbus_Read_REG_Num << 8) + RecData.Rec_Data[5];
                    /*                     if(Modbus_Read_Start_REG >= 40001)
                                        {
                                            if(Modbus_Read_Start_REG + Modbus_Read_REG_Num <= 40035)
                                            {
                                                unsigned char Send_Buff[75];
                                                unsigned char numb = 0;

                                                Send_Buff[numb++] = config.Dev_Address;
                                                Send_Buff[numb++] = 0x03;
                                                Send_Buff[numb++] = Modbus_Read_REG_Num*2;

                                                for(i= 0; i < Modbus_Read_REG_Num; i++ )
                                                {

                                                    res = get_reg(Modbus_Read_Start_REG+i - 40001);

                                                    Send_Buff[numb++] = (u8)(res>>8);
                                                    Send_Buff[numb++] = (u8)(res&0xff);
                                                }

                                                crc_16(Send_Buff,CRC_Rst,numb);

                                                Send_Buff[numb++] = CRC_Rst[1];
                                                Send_Buff[numb++] = CRC_Rst[0];

                                                M_Uart_Send_Block(Send_Buff,numb);

                                            } else {

                                                Modbus_RevErr_ACK(RecData.Rec_Data[1],MSB_Error_Reg);
                                            }
                                        } */
                    if (Modbus_Read_Start_REG >= 0)
                    {
                        if (Modbus_Read_Start_REG + Modbus_Read_REG_Num <= 35)
                        {
                            unsigned char Send_Buff[75];
                            unsigned char numb = 0;

                            Send_Buff[numb++] = config.Dev_Address;
                            Send_Buff[numb++] = 0x03;
                            Send_Buff[numb++] = Modbus_Read_REG_Num * 2;

                            for (i = 0; i < Modbus_Read_REG_Num; i++)
                            {

                                res = get_reg(Modbus_Read_Start_REG + i);

                                Send_Buff[numb++] = (u8)(res >> 8);
                                Send_Buff[numb++] = (u8)(res & 0xff);
                            }

                            crc_16(Send_Buff, CRC_Rst, numb);

                            Send_Buff[numb++] = CRC_Rst[1];
                            Send_Buff[numb++] = CRC_Rst[0];

                            M_Uart_Send_Block(Send_Buff, numb);
                        }
                        else
                        {

                            Modbus_RevErr_ACK(RecData.Rec_Data[1], MSB_Error_Reg);
                        }
                    }
                    else
                    {

                        Modbus_RevErr_ACK(RecData.Rec_Data[1], MSB_Error_Reg);
                    }

                    break;
                    /*                 case 0x06:

                                        REG_Value = RecData.Rec_Data[2];
                                        REG_Value = (REG_Value << 8) + RecData.Rec_Data[3];

                                        CMD_Value = RecData.Rec_Data[4];
                                        CMD_Value = (CMD_Value << 8) + RecData.Rec_Data[5];

                                        switch (REG_Value)
                                        {
                                        case 40037: // 总开关
                                            Speak_Con(0);
                                            if (CMD_Value == 0x5555)
                                            {
                                            }
                                            else if (CMD_Value == 0xAAAA)
                                            {
                                            }
                                            M_Uart_Send_Block(RecData.Rec_Data, RecData.Rec_Len);
                                            break;
                                        case 40035:
                                            Speak_Con(0);
                                            if (CMD_Value == 0x5555)
                                            {
                                                if ((config.Hand_Auto_timer_Flag == RUNMODE_MANUAL)) // 无故障情况下，才可手动开机
                                                {
                                                    if (sys.Pump_A_ON == 0 && sys.Fault_A_Flag == 0)
                                                    {
                                                        sys.Pump_A_ON = 1;
                                                    }
                                                }
                                            }
                                            else if (CMD_Value == 0xAAAA)
                                            {
                                                sys.Pump_A_ON = 0;
                                            }
                                            M_Uart_Send_Block(RecData.Rec_Data, RecData.Rec_Len);
                                            break;
                                        case 40036:
                                            Speak_Con(0);
                                            if (CMD_Value == 0x5555)
                                            {
                                                if ((config.Hand_Auto_timer_Flag == RUNMODE_MANUAL)) // 无故障情况下，才可手动开机
                                                {
                                                    if (sys.Pump_B_ON == 0 && sys.Fault_B_Flag == 0)
                                                    {
                                                        sys.Pump_B_ON = 1;
                                                    }
                                                }
                                            }
                                            else if (CMD_Value == 0xAAAA)
                                            {
                                                sys.Pump_B_ON = 0;
                                            }
                                            M_Uart_Send_Block(RecData.Rec_Data, RecData.Rec_Len);
                                            break;
                                        case 40038:
                                            Speak_Con(0);
                                            if (CMD_Value == 0x0001)
                                            {
                                                config.Pump_AB_Mode = 0;
                                                M_Uart_Send_Block(RecData.Rec_Data, RecData.Rec_Len);
                                            }
                                            else if (CMD_Value == 0x0002)
                                            {
                                                config.Pump_AB_Mode = 1;
                                                M_Uart_Send_Block(RecData.Rec_Data, RecData.Rec_Len);
                                            }
                                            else if (CMD_Value == 0x0003)
                                            {
                                                config.Pump_AB_Mode = 2;
                                                M_Uart_Send_Block(RecData.Rec_Data, RecData.Rec_Len);
                                            }
                                            else
                                            {
                                                Modbus_RevErr_ACK(RecData.Rec_Data[1], Msb_Error_Dat);
                                            }
                                            // EEPROM_Write(2,config.Pump_AB_Mode);
                                            sys.save_flag = 1;

                                            break;
                                        case 40039:
                                            Speak_Con(0);
                                            if (CMD_Value == 0x0000)
                                            {
                                                config.Hand_Auto_timer_Flag = RUNMODE_MANUAL;
                                                M_Uart_Send_Block(RecData.Rec_Data, RecData.Rec_Len);
                                            }
                                            else if (CMD_Value == 0x0001)
                                            {
                                                config.Hand_Auto_timer_Flag = RUNMODE_AUTO;
                                                M_Uart_Send_Block(RecData.Rec_Data, RecData.Rec_Len);
                                            }
                                            else
                                            {
                                                Modbus_RevErr_ACK(RecData.Rec_Data[1], Msb_Error_Dat);
                                            }
                                            sys.save_flag = 1;

                                            break;
                                        case 40040:
                                            Speak_Con(0);
                                            if (CMD_Value == 0x0000)
                                            {
                                                M_Uart_Send_Block(RecData.Rec_Data, RecData.Rec_Len);

                                                for (i = 0; i < 10; i++)
                                                {
                                                    config.Record_Type[i] = 0;
                                                    config.Record_Dat1[i] = 0;
                                                    config.Record_Dat2[i] = 0;
                                                }
                                                sys.save_flag = 1;
                                                Record_Clr_and_Reset();
                                            }
                                            else
                                            {
                                                Modbus_RevErr_ACK(RecData.Rec_Data[1], Msb_Error_Dat);
                                            }

                                            break;
                                        case 40011:
                                            Speak_Con(0);
                                            M_Uart_Send_Block(RecData.Rec_Data, RecData.Rec_Len);

                                            config.AUTO_Timing_Run = RecData.Rec_Data[4];
                                            config.AUTO_Timing_Run = (config.AUTO_Timing_Run << 8) + RecData.Rec_Data[5];
                                            // EEPROM_Write_Word(3,config.AUTO_Timing_Run);
                                            sys.save_flag = 1;
                                            break;
                                        case 40012:
                                            Speak_Con(0);
                                            M_Uart_Send_Block(RecData.Rec_Data, RecData.Rec_Len);

                                            config.AUTO_Timing_Stop = RecData.Rec_Data[4];
                                            config.AUTO_Timing_Stop = (config.AUTO_Timing_Stop << 8) + RecData.Rec_Data[5];
                                            // EEPROM_Write_Word(5,config.AUTO_Timing_Stop);
                                            sys.save_flag = 1;
                                            break;
                                        case 40013:
                                            Speak_Con(0);
                                            M_Uart_Send_Block(RecData.Rec_Data, RecData.Rec_Len);

                                            config.Set_STANDBY_Current1 = RecData.Rec_Data[4];
                                            config.Set_STANDBY_Current1 = (config.Set_STANDBY_Current1 << 8) + RecData.Rec_Data[5];

                                            config.Set_STANDBY_Current2 = config.Set_STANDBY_Current1;

                                            sys.save_flag = 1;
                                            break;
                                        case 40014:
                                            Speak_Con(0);
                                            M_Uart_Send_Block(RecData.Rec_Data, RecData.Rec_Len);

                                            config.Set_OVERLOAD_Current1 = RecData.Rec_Data[4];
                                            config.Set_OVERLOAD_Current1 = (config.Set_OVERLOAD_Current1 << 8) + RecData.Rec_Data[5];
                                            // EEPROM_Write_Word(9,config.Set_OVERLOAD_Current);
                                            sys.save_flag = 1;
                                            break;
                                        case 40015:
                                            Speak_Con(0);
                                            M_Uart_Send_Block(RecData.Rec_Data, RecData.Rec_Len);

                                            config.Set_OVERLOAD_Current1 = RecData.Rec_Data[4];

                                            config.Set_OVERLOAD_Current1 = (config.Set_OVERLOAD_Current1 << 8) + RecData.Rec_Data[5];
                                            // EEPROM_Write_Word(9,config.Set_OVERLOAD_Current);
                                            sys.save_flag = 1;
                                            break;
                                        case 40016:
                                            Speak_Con(0);
                                            M_Uart_Send_Block(RecData.Rec_Data, RecData.Rec_Len);

                                            config.AUTO_FANGXIU = RecData.Rec_Data[4];
                                            config.AUTO_FANGXIU = (config.AUTO_FANGXIU << 8) + RecData.Rec_Data[5];
                                            // EEPROM_Write_Word(11,config.AUTO_FANGXIU);
                                            sys.save_flag = 1;
                                            break;
                                        case 40017:
                                            Speak_Con(0);
                                            M_Uart_Send_Block(RecData.Rec_Data, RecData.Rec_Len);

                                            config.Timer_Unload_Restart = RecData.Rec_Data[4];
                                            config.Timer_Unload_Restart = (config.Timer_Unload_Restart << 8) + RecData.Rec_Data[5];
                                            // EEPROM_Write_Word(14,config.Timer_Unload_Restart);
                                            sys.save_flag = 1;
                                            break;
                                        default:
                                            Modbus_RevErr_ACK(RecData.Rec_Data[1], MBS_Error_Addr);
                                            break;
                                        } */
                case 0x06:

                    REG_Value = RecData.Rec_Data[2];
                    REG_Value = (REG_Value << 8) + RecData.Rec_Data[3];

                    CMD_Value = RecData.Rec_Data[4];
                    CMD_Value = (CMD_Value << 8) + RecData.Rec_Data[5];

                    switch (REG_Value)
                    {
                    case 35:
                        Speak_Con(0);
                        if (CMD_Value == 0x5555)
                        {
                            if ((config.Hand_Auto_timer_Flag == RUNMODE_MANUAL)) // 无故障情况下，才可手动开机
                            {
                                if (sys.Pump_A_ON == 0 && sys.Fault_A_Flag == 0)
                                {
                                    sys.Pump_A_ON = 1;
                                }
                            }
                        }
                        else if (CMD_Value == 0xAAAA)
                        {
                            sys.Pump_A_ON = 0;
                        }
                        M_Uart_Send_Block(RecData.Rec_Data, RecData.Rec_Len);
                        break;
                    case 36:
                        Speak_Con(0);
                        if (CMD_Value == 0x5555)
                        {
                            if ((config.Hand_Auto_timer_Flag == RUNMODE_MANUAL)) // 无故障情况下，才可手动开机
                            {
                                if (sys.Pump_B_ON == 0 && sys.Fault_B_Flag == 0)
                                {
                                    sys.Pump_B_ON = 1;
                                }
                            }
                        }
                        else if (CMD_Value == 0xAAAA)
                        {
                            sys.Pump_B_ON = 0;
                        }
                        M_Uart_Send_Block(RecData.Rec_Data, RecData.Rec_Len);
                        break;
                    default:
                        Modbus_RevErr_ACK(RecData.Rec_Data[1], MBS_Error_Addr);
                        break;
                    }
                    break;
                default:
                    Modbus_RevErr_ACK(RecData.Rec_Data[1], MBS_Error_Addr);
                    break;
                }
            }
        }
    }
}

#endif
