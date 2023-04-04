// This file defines a union that is used to hold the referencing identifier for each type of command station
// For example, an ESU system uses an object ID, which is a number, whereas a WiThrottle implementation is going to use a character
// It's done as a union since there will only ever be one type of command station at a time (famous last words)
#pragma once
#include <stdint.h>

typedef union 
{
    int32_t esuObjID;     // occupies 4 bytes
    uint8_t withrottleLetter;
} CmdStnLocRef;  