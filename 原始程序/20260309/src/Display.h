#ifndef	__DISPLAY_H
#define	__DISPLAY_H

#include "system.h"

extern const u8 LED_LIST[13];
extern u8 Dis1_Flash;
extern u8 Dis2_Flash;
extern u8 Dis3_Flash;
extern u8 Dis4_Flash;
extern u8 Dis5_Flash;
extern u8 Dis6_Flash;
extern u8 Dis7_Flash;
extern u8 Dis8_Flash;

void Display_Init(void);
void Display_Update(void);
void Power_Up_Dis(void);

void Beep(void);
void Speak_Con(u8 type);
void Speak_Stop(void);


#endif