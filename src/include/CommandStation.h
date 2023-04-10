#pragma once
#include <stdint.h>
#include <WiFi.h>

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
    virtual bool begin(WiFiClient &cmdStnConnection, uint32_t quirkFlags);
    virtual bool end();
    virtual bool update();
    virtual bool locomotiveObjectGet(CmdStnLocRef** locCmdStnRef, uint16_t addr, bool isLongAddr, uint8_t mrbusAddr);
    virtual bool locomotiveEmergencyStop(CmdStnLocRef* locCmdStnRef);
    virtual bool locomotiveSpeedSet(CmdStnLocRef* locCmdStnRef, uint8_t speed, bool isReverse);
    virtual bool locomotiveFunctionsGet(CmdStnLocRef* locCmdStnRef, bool functionStates[]);
    virtual bool locomotiveFunctionSet(CmdStnLocRef* locCmdStnRef, uint8_t funcNum, bool funcActive);
    virtual bool locomotiveDisconnect(CmdStnLocRef* locCmdStnRef);
};
