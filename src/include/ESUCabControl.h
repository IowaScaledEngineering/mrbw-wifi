#pragma once
#include "CommandStation.h"

class ESUCabControl : public CommandStation
{
  private:
    WiFiClient *cmdStnConnection;

  public:
    ESUCabControl();
    ~ESUCabControl();
    bool begin(WiFiClient &cmdStnConnection, uint32_t quirkFlags);
    bool end();
    bool update();
    bool locomotiveObjectGet(CmdStnLocRef** locCmdStnRef, uint16_t addr, bool isLongAddr, uint8_t mrbusAddr);
    bool locomotiveEmergencyStop(CmdStnLocRef* locCmdStnRef);
    bool locomotiveSpeedSet(CmdStnLocRef* locCmdStnRef, uint8_t speed, bool isReverse);
    bool locomotiveFunctionsGet(CmdStnLocRef* locCmdStnRef, bool functionStates[]);
    bool locomotiveFunctionSet(CmdStnLocRef* locCmdStnRef, uint8_t funcNum, bool funcActive);
    bool locomotiveDisconnect(CmdStnLocRef* locCmdStnRef);
};