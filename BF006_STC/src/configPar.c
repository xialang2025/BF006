#include "configPar.h"
#include "eeprom.h"
#include "system.h"
#include "user.h"
#include "config_store.h" //20261114-4 add
//20261113-0 del 封存旧协议头文件引用：#include "updata4G.h"

#define BIAS_ADDR 0x0000 //20260729-13 change 恢复 7 月 14 日版本的参数单副本存储地址
#define BAN_BEN 18 //20260430-3 change update version after adding total-time remainder fields

config_t config;

#if BF006_CONFIG_STORE_ENABLED
#if BF006_CONFIG_SLOT_A < 512 || BF006_CONFIG_SLOT_B < 512 || BF006_CONFIG_SLOT_A == BF006_CONFIG_SLOT_B || BF006_CONFIG_SLOT_A % 512 || BF006_CONFIG_SLOT_B % 512
#error BF006_CONFIG_SLOT_LAYOUT
#endif

u8 config_trusted; //20261114-4 add 当前配置存在可识别的存储来源
u32 config_revision;
static u8 storage_saving;
static u8 storage_rescan;
static u8 panel_pending;
static u8 panel_saved_dirty;
static u16 settings_crc;
Config_Byte_Edit xdata panel_edits[CONFIG_PANEL_EDITS];

/******************************************************************************
 * 函数名称       : Config_Settings_CRC
 * 功能描述       : 识别影响现场编辑依据的配置字节变化。
 * 输入参数       : None
 * 返回值         : 配置校验值
 * 日期            : 2026.11.14
 ******************************************************************************/
static u16 Config_Settings_CRC(void) //20261114-4 add
{
    u8 *bytes;
    u8 i;
    u8 bit_index;
    u16 checksum;

    bytes = (u8 *)&config;
    checksum = 0xFFFF;
    for (i = 0; i < CONFIG_STORE_SIZE; i++) {
        if ((i >= (u8)((u8 *)&config.Record_Type - bytes) &&
             i < (u8)((u8 *)&config.Auto_Mode - bytes)) ||
            i >= (u8)((u8 *)&config.Total_Run_Hour - bytes)) {
            continue; // 历史、累计时间和存储格式不改变编辑依据
        }
        checksum ^= bytes[i];
        for (bit_index = 0; bit_index < 8; bit_index++) {
            checksum = (checksum & 1) ? (checksum >> 1) ^ 0xA001 : checksum >> 1;
        }
    }
    return checksum;
}

/******************************************************************************
 * 函数名称       : Config_Observe_Changes
 * 功能描述       : 发现现场写者已改变配置时使旧面板编辑失效。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void Config_Observe_Changes(void) //20261114-4 add
{
    u16 current;

    if (!config_trusted) {
        return;
    }
    current = Config_Settings_CRC();
    if (current != settings_crc) {
        settings_crc = current;
        sys.save_flag = 1;
        if (config_revision != 0xFFFFFFFFUL) {
            config_revision++;
        }
    }
}
#endif

extern u16 Limit_Current;

static u8 Total_Run_Second_Cnt = 0;   //20260501-1 add run-time second counter, reset by the clear command
static u8 Total_Power_Second_Cnt = 0; //20260501-1 add power-on second counter, reset by the clear command

/******************************************************************************
 * 函数名称       : Par_Init
 * 功能描述       : 按 7 月 14 日版本从 EEPROM 固定地址读取参数。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.07.29
 ******************************************************************************/
void Par_Init(void) //20260729-13 change
{
    u8 i = 0;
#if BF006_CONFIG_STORE_ENABLED
    u8 storage;

    storage_saving = 0;
    storage_rescan = 0;
    panel_pending = 0;
    panel_saved_dirty = 0;
    config_revision = 1;
    config_trusted = 0;
    storage = Config_Store_Open(BF006_CONFIG_SLOT_A, BF006_CONFIG_SLOT_B, (u8 *)&config);
    if (storage == CONFIG_STORE_READY && config.banben == BAN_BEN) {
        config_trusted = 1;
        goto publish;
    }
    if (storage != CONFIG_STORE_EMPTY) {
        memset(&config, 0, sizeof(config));
        config.Dev_Address = 1;
        goto publish;
    }
#endif
    EEPROM_read_n(BIAS_ADDR, (u8 *)&config, sizeof(config_t)); // 使用老版本固定地址读取完整参数结构

#if BF006_CONFIG_STORE_ENABLED
    if (config.banben == 17 || config.banben == BAN_BEN) {
        config_trusted = 1;
        sys.save_flag = 1; // 旧参数区只读，迁移只写候选槽
    } else {
        memset(&config, 0, sizeof(config));
        config.Dev_Address = 1;
        goto publish;
    }
#endif

    if (config.banben == 17) //20260430-3 add preserve existing parameters when upgrading from the placeholder version and only initialize new remainder fields
    {
        config.banben = BAN_BEN;
        config.Total_Run_Minute = 0;
        config.Total_Power_Hour = 0;
        config.Total_Power_Minute = 0;
        sys.save_flag = 1;
    }

    if (config.banben != BAN_BEN)
    {
        config.banben = BAN_BEN;
        config.AUTO_Timing_Run = 30;
        config.AUTO_Timing_Stop = 60;
        config.Timer_Standby_Restart = 30;
        config.Timer_Unload_Restart = 30;
        config.AUTO_FANGXIU = 99;
#if (VoltageValue == Vol_380)
        config.OVP_Value = Value_OVP;
        config.UVP_Value = 320;
#else
        config.OVP_Value = 260;
        config.UVP_Value = 150;
#endif
        config.Dev_type = 0;
        config.Set_OVERLOAD_Current1 = 100;
        config.Set_STANDBY_Current1 = 0;
        config.A_Turn = 0;
        config.Set_OVERLOAD_Current2 = 100;
        config.Set_STANDBY_Current2 = 0;
        config.B_Turn = 0;
        config.Hand_Auto_timer_Flag = RUNMODE_MANUAL;
        config.Pump_AB_Mode = 0;
        config.Dev_Address = 1;

        config.Sensor_Rang = 10;
        config.Sensor_overFlow = 10;
        config.Sensor_STOP = 9;
        config.Sensor_ON1 = 5;
        config.Sensor_ON2 = 1;
        config.Sensor_Mode = CTROLTYPE_PUMP;
        config.Lock1 = 0;
        config.Lock2 = 0;
        config.Auto_Mode = 0;
        config.fault_mask = 0;
        config.Total_Run_Hour = 0;     //20260430-3 change default total run time
        config.Total_Run_Minute = 0;   //20260430-3 add default run-time minute remainder
        config.Total_Power_Day = 0;    //20260430-3 change default total power-on time
        config.Total_Power_Hour = 0;   //20260430-3 add default power-on hour remainder
        config.Total_Power_Minute = 0; //20260430-3 add default power-on minute remainder

        for (i = 0; i < 5; i++)
        {
            config.RF_Family[i] = 0;
        }

        for (i = 0; i < 10; i++)
        {
            config.Record_Type[i] = ERR_NUMB;
            config.Record_Dat1[i] = 0;
            config.Record_Dat2[i] = 0;
        }

        config.run_fen = 0;
        config.run_xiaoshi = 0;
#if 0 //20261113-0 del 恢复默认参数时停用旧协议的全量上报。
        Send_dat.updataAll = 1;
        Send_dat.GuZhangJiLu = 1;
#endif
    }

#if BF006_CONFIG_STORE_ENABLED
publish:
    settings_crc = Config_Settings_CRC();
#endif
    switch (config.Dev_type)
    {

    case 1:
        Limit_Current = V2_Limit_Current;
        break;
    case 2:
        Limit_Current = V3_Limit_Current;
        break;
    case 3:
        Limit_Current = V4_Limit_Current;
        break;
    case 4:
        Limit_Current = V5_Limit_Current;
        break;
    default:
        Limit_Current = V1_Limit_Current;
        break;
    }

    updatSysdata();
}

/******************************************************************************
 * Function       : Total_Time_Second_Handle
 * Description       : Maintain P-21 total run time and P-22 total power-on time once per second.
 * Input       : pump_running
 * Return         : None
 * Date            : 2026.04.30
 ******************************************************************************/
void Total_Time_Second_Handle(u8 pump_running) //20260430-3 add
{
    Total_Power_Second_Cnt++;            // accumulate power-on time while the program is running
    if (Total_Power_Second_Cnt >= 60)
    {
        Total_Power_Second_Cnt = 0;
#if (TOTAL_TIME_DEBUG_MODE == 1)
        if (config.Total_Power_Day < 9999)
        {
            config.Total_Power_Day++;    // in debug mode, P-22 increments once per minute and uses minutes as the unit
            sys.save_flag = 1;
        }
#else
        if (config.Total_Power_Day < 9999)
        {
            config.Total_Power_Minute++; // in normal mode, save the minute remainder below one day
            if (config.Total_Power_Minute >= 60)
            {
                config.Total_Power_Minute = 0;
                config.Total_Power_Hour++;
                if (config.Total_Power_Hour >= 24)
                {
                    config.Total_Power_Hour = 0;
                    config.Total_Power_Day++;
                }
            }
            sys.save_flag = 1;
        }
#endif
    }

    if (pump_running == 0)
    {
        Total_Run_Second_Cnt = 0;        // do not accumulate run time when the pump is stopped; discard less than one minute
        return;
    }

    Total_Run_Second_Cnt++;
    if (Total_Run_Second_Cnt >= 60)
    {
        Total_Run_Second_Cnt = 0;
#if (TOTAL_TIME_DEBUG_MODE == 1)
        if (config.Total_Run_Hour < 9999)
        {
            config.Total_Run_Hour++;     // in debug mode, P-21 increments once per running minute and uses minutes as the unit
            sys.save_flag = 1;
        }
#else
        if (config.Total_Run_Hour < 9999)
        {
            config.Total_Run_Minute++;   // in normal mode, save the minute remainder below one hour
            if (config.Total_Run_Minute >= 60)
            {
                config.Total_Run_Minute = 0;
                config.Total_Run_Hour++;
            }
            sys.save_flag = 1;
        }
#endif
    }
}

/******************************************************************************
 * Function       : Total_Run_Time_Clear
 * Description       : Clear P-21 total run time and the temporary counter below one minute.
 * Input       : None
 * Return         : None
 * Date            : 2026.05.01
 ******************************************************************************/
void Total_Run_Time_Clear(void) //20260501-1 add
{
    Total_Run_Second_Cnt = 0;
    config.Total_Run_Hour = 0;
    config.Total_Run_Minute = 0;
    sys.save_flag = 1;                  // write to EEPROM through the original save flow after clearing
}

/******************************************************************************
 * Function       : Total_Power_Time_Clear
 * Description       : Clear P-22 total power-on time and the temporary counter below one minute.
 * Input       : None
 * Return         : None
 * Date            : 2026.05.01
 ******************************************************************************/
void Total_Power_Time_Clear(void) //20260501-1 add
{
    Total_Power_Second_Cnt = 0;
    config.Total_Power_Day = 0;
    config.Total_Power_Hour = 0;
    config.Total_Power_Minute = 0;
    sys.save_flag = 1;                  // write to EEPROM through the original save flow after clearing
}

#if BF006_CONFIG_STORE_ENABLED
/******************************************************************************
 * 函数名称       : Config_Panel_Available
 * 功能描述       : 判断面板暂存区是否可用于下一次编辑。
 * 输入参数       : None
 * 返回值         : 可用时返回一
 * 日期            : 2026.11.14
 ******************************************************************************/
u8 Config_Panel_Available(void) //20261114-4 add
{
    return config_trusted && !storage_saving && !storage_rescan &&
           config_revision != 0xFFFFFFFFUL;
}

/******************************************************************************
 * 函数名称       : Config_Panel_Encode
 * 功能描述       : 将单字节或双字节现场参数放入面板候选。
 * 输入参数       : index 候选索引，offset 参数偏移，value 参数值，width 字节数
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void Config_Panel_Encode(u8 index, u8 offset, u16 value, u8 width) //20261114-4 add
{
    u8 *bytes;

    bytes = (u8 *)&value;
    panel_edits[index].offset = offset;
    panel_edits[index].value = (width == 1) ? (u8)value : bytes[0];
    if (width == 2) {
        panel_edits[index + 1].offset = offset + 1;
        panel_edits[index + 1].value = bytes[1];
    }
}

/******************************************************************************
 * 函数名称       : Config_Panel_Submit
 * 功能描述       : 提交面板整组候选，确认后才写入运行配置。
 * 输入参数       : count 候选字节数，按配置偏移递增
 * 返回值         : 已受理或同值时返回一
 * 日期            : 2026.11.14
 ******************************************************************************/
u8 Config_Panel_Submit(u8 count) //20261114-4 add
{
    u8 result;

    if (!count || count > CONFIG_PANEL_EDITS || !Config_Panel_Available()) {
        return 0;
    }
    result = Config_Store_Begin((u8 *)&config, panel_edits, count);
    storage_saving = (result == CONFIG_STORE_BUSY);
    panel_pending = storage_saving ? count : 0;
    if (storage_saving) {
        panel_saved_dirty = sys.save_flag;
        sys.save_flag = 0;
    }
    return storage_saving || result == CONFIG_STORE_READY;
}

/******************************************************************************
 * 函数名称       : Config_Background
 * 功能描述       : 每次主循环推进一次存储操作或处理待确认的提交。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.11.14
 ******************************************************************************/
void Config_Background(void) //20261114-4 add
{
    u8 result;
    u8 i;
    u8 matched;

    if (storage_rescan) {
        storage_rescan = 0;
        result = Config_Store_Open(BF006_CONFIG_SLOT_A, BF006_CONFIG_SLOT_B, (u8 *)&config);
        config_trusted = (result == CONFIG_STORE_READY && config.banben == BAN_BEN);
        if (config_trusted) {
            updatSysdata();
            settings_crc = Config_Settings_CRC();
            matched = panel_pending;
            for (i = 0; i < panel_pending; i++) {
                if (((u8 *)&config)[panel_edits[i].offset] != panel_edits[i].value) {
                    matched = 0;
                }
            }
            if (matched && config_revision != 0xFFFFFFFFUL) {
                config_revision++;
            }
        }
        panel_pending = 0;
        return;
    }
    if (!config_trusted || !storage_saving) {
        return;
    }
    if (sys.save_flag) {
        Config_Store_Cancel(); // 原有写者已改变配置，取消尚未落标的候选
    }
    result = Config_Store_Step();
    if (result == CONFIG_STORE_BUSY) {
        return;
    }
    storage_saving = 0;
    if (result == CONFIG_STORE_SAVED) {
        updatSysdata();
        if (panel_pending) {
            config_revision++;
            settings_crc = Config_Settings_CRC();
        }
    } else if (result == CONFIG_STORE_UNCERTAIN) {
        storage_rescan = 1;
        return;
    } else if (!panel_pending || panel_saved_dirty) {
        sys.save_flag = 1;
    }
    panel_pending = 0;
}
#endif

/******************************************************************************
 * 函数名称       : save_pro
 * 功能描述       : 按 7 月 14 日版本在保存标志有效且延时结束后擦写 EEPROM 参数。
 * 输入参数       : None
 * 返回值         : None
 * 日期            : 2026.07.29
 ******************************************************************************/
void save_pro(void) //20261114-4 change
{
#if BF006_CONFIG_STORE_ENABLED
    if (config_trusted && !storage_saving && !storage_rescan && sys.save_flag) {
        if (Config_Store_Begin((u8 *)&config, 0, 0) == CONFIG_STORE_BUSY) {
            storage_saving = 1;
            sys.save_flag = 0;
        }
    }
#else
    static u8 tick = 0;
    if ((sys.save_flag == 1) && (tick == 0)) // 恢复老版本保存节奏，避免新增掉电安全逻辑改变现场已验证行为
    {
        EEPROM_SectorErase(BIAS_ADDR);

        EEPROM_write_n(BIAS_ADDR, (u8 *)&config, sizeof(config_t));
        sys.save_flag = 0;
        tick = 200;
    }
    else
    {
        if (tick > 0)
        {
            tick--;
        }
    }
#endif
}
