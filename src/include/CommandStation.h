#pragma once
#include <stdint.h>
#include <WiFi.h>
#include "ThrottleState.h"
#include "Clock.h"

class CmdStnLocRef
{
  public:
    CmdStnLocRef();
    CmdStnLocRef(uint16_t locAddr, bool isLongAddr, uint8_t mrbusAddr);
    uint16_t locAddr;
    bool isLongAddr;
    uint8_t mrbusAddr;
};


class CommandStation
{
  public:
    virtual bool fastClockConnect(Clock* fastClock);
    virtual void fastClockDisconnect();
    virtual bool begin(WiFiClient &cmdStnConnection, uint32_t quirkFlags, uint8_t debugLvl);
    virtual bool end();
    virtual bool update();
    virtual bool locomotiveObjectGet(ThrottleState* locCmdStnRef, uint16_t addr, bool isLongAddr, uint8_t mrbusAddr);
    virtual bool locomotiveEmergencyStop(ThrottleState* locCmdStnRef);
    virtual bool locomotiveSpeedSet(ThrottleState* locCmdStnRef, uint8_t speed, bool isReverse);
    virtual bool locomotiveFunctionsGet(ThrottleState* locCmdStnRef, bool functionStates[]);
    virtual bool locomotiveFunctionSet(ThrottleState* locCmdStnRef, uint8_t funcNum, bool funcActive);
    virtual bool locomotiveDisconnect(ThrottleState* locCmdStnRef);
};
