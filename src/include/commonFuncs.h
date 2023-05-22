#pragma once

#include <stdint.h>
#include <string.h>
#include <ctype.h>


// Common constants used all over the place

#define MAX_THROTTLES  26
#define MAX_FUNCTIONS  29
#define MRBUS_THROTTLE_BASE_ADDR  0x30
#define THROTTLE_TIMEOUT_SECONDS  30
#define WDT_TIMEOUT    20


#ifndef MIN
#define MIN(a,b) ((a<b)?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) ((a>b)?(a):(b))
#endif

char* ltrim(char* in);
char* rtrim(char* in);
char* trim(char* in);


#define DBGLVL_ERR         0
#define DBGLVL_WARN        1
#define DBGLVL_INFO        2
#define DBGLVL_DEBUG       3

#define IS_DBGLVL_ERR    (true)
#define IS_DBGLVL_WARN   ((this->debug) >= DBGLVL_WARN)
#define IS_DBGLVL_INFO   ((this->debug) >= DBGLVL_INFO)
#define IS_DBGLVL_DEBUG  ((this->debug) >= DBGLVL_DEBUG)

// Info should be 0-2
// Debug should be 0-3
