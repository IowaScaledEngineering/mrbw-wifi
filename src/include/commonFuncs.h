#pragma once

#include <stdint.h>
#include <string.h>
#include <ctype.h>


// Common constants used all over the place

#define MAX_THROTTLES  26
#define MAX_FUNCTIONS  29
#define MRBUS_THROTTLE_BASE_ADDR  0x30
#define THROTTLE_TIMEOUT_SECONDS  30
#define WDT_TIMEOUT    15


#ifndef MIN
#define MIN(a,b) ((a<b)?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) ((a>b)?(a):(b))
#endif

char* ltrim(char* in);
char* rtrim(char* in);
char* trim(char* in);