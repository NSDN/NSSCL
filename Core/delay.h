#ifndef __DELAY_H
#define __DELAY_H

#include "stdio.h"
#include "ch32v20x.h"

#ifdef __cplusplus
extern "C" {
#endif

void Delay_Init(void);
void Delay_Us(uint32_t n);
void Delay_Ms(uint32_t n);

#ifdef __cplusplus
}
#endif

#endif
