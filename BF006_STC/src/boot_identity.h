#ifndef BF006_BOOT_IDENTITY_H
#define BF006_BOOT_IDENTITY_H

#include "system.h"

u32 Boot_Identity_Advance(u16 first, u16 second, u16 size);
u8 Boot_Identity_Provision(u16 first, u16 second, u16 size, u32 seed);

#endif
