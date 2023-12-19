#ifndef __OLED_H_
#define __OLED_H_


#include "ch32v20x.h"

#include <stdint.h>

#define OLED_ADDR   0x78
#define OLED_WIDTH  128
#define OLED_HEIGHT 32
#define IOBUF_SIZE  16

void OLED_Init();
void OLED_Clear();
void OLED_Switch(uint8_t state);
void OLED_Char(uint8_t x, uint8_t y, uint8_t fid, uint8_t color, char c);
void OLED_Print(uint8_t x, uint8_t y, uint8_t fid, uint8_t color, char* str);
int OLED_Printf(uint8_t x, uint8_t y, uint8_t fid, uint8_t color, const char* format, ...);
int OLED_Printfc(uint8_t y, uint8_t fid, uint8_t color, const char* format, ...);


#endif
