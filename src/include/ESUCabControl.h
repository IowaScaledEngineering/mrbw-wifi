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
    bool locomotiveObjectGet(ThrottleState* locCmdStnRef, uint16_t addr, bool isLongAddr, uint8_t mrbusAddr);
    bool locomotiveEmergencyStop(ThrottleState* locCmdStnRef);
    bool locomotiveSpeedSet(ThrottleState* locCmdStnRef, uint8_t speed, bool isReverse);
    bool locomotiveFunctionsGet(ThrottleState* locCmdStnRef, bool functionStates[]);
    bool locomotiveFunctionSet(ThrottleState* locCmdStnRef, uint8_t funcNum, bool funcActive);
    bool locomotiveDisconnect(ThrottleState* locCmdStnRef);
};