#pragma once

#include <stdint.h>
void ws2812Set(uint32_t rgb);

#define WS2812_GREEN   0x000f00
#define WS2812_YELLOW  0x0f0f00
#define WS2812_RED     0x0f0000 
#define WS2812_CYAN    0x000f0f
#define WS2812_BLUE    0x00000f