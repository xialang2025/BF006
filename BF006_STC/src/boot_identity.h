#ifndef BF006_BOOT_IDENTITY_H
#define BF006_BOOT_IDENTITY_H

#include "system.h"
#include "config_store.h"

#if BF006_CONFIG_STORE_ENABLED && defined(BF006_BOOT_SLOT_A) && defined(BF006_BOOT_SLOT_B) && defined(BF006_TIMER0_VERIFIED)
#define BF006_BOARD_CONTEXT 1
void Boot_Context_Init(void);
void Boot_Context_Tick(void);
u8 Boot_Context_Read(u32 *boot, u32 *revision, u32 *uptime);
#else
#define BF006_BOARD_CONTEXT 0
#endif

u32 Boot_Identity_Advance(u16 first, u16 second, u16 size);
u8 Boot_Identity_Provision(u16 first, u16 second, u16 size, u32 seed);

#endif
