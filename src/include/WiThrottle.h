#pragma once
#include "CommandStation.h"

#define WITHROTTLE_QUIRK_LNWI    0x00000001


class WiThrottle : public CommandStation
{
  private:
    WiFiClient *cmdStnConnection;
    bool lnwiMode;

  public:
    WiThrottle();
    ~WiThrottle();
    bool begin(WiFiClient &cmdStnConnection, uint32_t quirkFlags);
    bool end();
    bool locomotiveObjectGet(CmdStnLocRef* locCmdStnRef, uint16_t addr, bool isLongAddr, uint8_t mrbusAddr);
    bool locomotiveEmergencyStop(CmdStnLocRef locCmdStnRef);
    bool locomotiveSpeedSet(CmdStnLocRef locCmdStnRef, uint8_t speed, bool isReverse);
    bool locomotiveFunctionsGet(CmdStnLocRef locCmdStnRef, bool functionStates[]);
    bool locomotiveFunctionSet(CmdStnLocRef locCmdStnRef, uint8_t funcNum, bool funcActive);
};