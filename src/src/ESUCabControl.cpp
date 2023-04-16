#include "ESUCabControl.h"

ESUCabControl::ESUCabControl()
{
  this->cmdStnConnection = NULL;
}

bool ESUCabControl::begin(WiFiClient &cmdStnConnection, uint32_t quirkFlags)
{
  this->cmdStnConnection = &cmdStnConnection;
  return true;
}

bool ESUCabControl::end()
{
  this->cmdStnConnection = NULL;
  return true;
}

bool ESUCabControl::update()
{
  return true;
}

bool ESUCabControl::locomotiveObjectGet(ThrottleState* tState, uint16_t addr, bool isLongAddr, uint8_t mrbusAddr)
{
  tState->locCmdStnRef = new CmdStnLocRef(addr, isLongAddr, mrbusAddr);
  return true;
}

bool ESUCabControl::locomotiveEmergencyStop(ThrottleState* tState)
{
  return true;
}

bool ESUCabControl::locomotiveSpeedSet(ThrottleState* tState, uint8_t speed, bool isReverse)
{
  return true;
}

bool ESUCabControl::locomotiveFunctionsGet(ThrottleState* tState, bool functionStates[])
{
  return true;
}

bool ESUCabControl::locomotiveFunctionSet(ThrottleState* tState, uint8_t funcNum, bool funcActive)
{
  return true;
}

bool ESUCabControl::locomotiveDisconnect(ThrottleState* tState)
{
  delete (CmdStnLocRef*)tState->locCmdStnRef;
  return true;
}


