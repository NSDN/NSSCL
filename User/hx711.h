#ifndef __HX711_H_
#define __HX711_H_


#include "ch32v20x.h"

#include <stdint.h>

#define HX711_HS        1
#define HX711_LS        0

#define HX711_A_128     25
#define HX711_B_32      26
#define HX711_A_64      27

void HX711_Init(uint8_t high);
void HX711_Switch(uint8_t ck);
uint8_t HX711_Ready();
int32_t HX711_GetValue();


#endif
