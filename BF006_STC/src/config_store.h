#ifndef BF006_CONFIG_STORE_H
#define BF006_CONFIG_STORE_H

#include "system.h"

#if (Model_OUT == Model_1C1) && defined(BF006_CONFIG_SLOT_A) && defined(BF006_CONFIG_SLOT_B)
#define BF006_CONFIG_STORE_ENABLED 1
#else
#define BF006_CONFIG_STORE_ENABLED 0
#endif

#if BF006_CONFIG_STORE_ENABLED
#define CONFIG_STORE_SIZE 117
#define CONFIG_STORE_EMPTY 0
#define CONFIG_STORE_READY 1
#define CONFIG_STORE_BUSY 2
#define CONFIG_STORE_SAVED 3
#define CONFIG_STORE_FAILED 4
#define CONFIG_STORE_UNCERTAIN 5
#define CONFIG_STORE_LOCKED 6

typedef struct {
    u8 offset;
    u8 value;
} Config_Byte_Edit;

u8 Config_Store_Open(u16 first, u16 second, u8 *payload);
u8 Config_Store_Begin(const u8 *payload, const Config_Byte_Edit *edits, u8 count);
u8 Config_Store_Step(void);
void Config_Store_Cancel(void);

#define CONFIG_PANEL_EDITS 16
#define CONFIG_OFFSET(member) ((u8)((u8 *)&config.member - (u8 *)&config))
extern Config_Byte_Edit xdata panel_edits[CONFIG_PANEL_EDITS];
extern u8 config_trusted;
extern u32 config_revision;
void Config_Background(void);
u8 Config_Panel_Available(void);
u8 Config_Panel_Submit(u8 count);
void Config_Panel_Encode(u8 index, u8 offset, u16 value, u8 width);
u8 Config_Submit(u8 count);
u8 Config_Commit_Status(void);
u8 Config_Stage_Mode(u8 mode);
void Config_Observe_Changes(void);
#endif

#endif
