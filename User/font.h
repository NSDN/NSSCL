#ifndef __FONT_H
#define __FONT_H

#include <stdint.h>

const uint8_t* FONT_Get(uint8_t id);
void FONT_Size(const uint8_t* font, uint8_t* w, uint8_t* h, uint8_t* offset, uint8_t* end);

#endif
