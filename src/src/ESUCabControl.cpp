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

bool ESUCabControl::locomotiveObjectGet(CmdStnLocRef** locCmdStnRef, uint16_t addr, bool isLongAddr, uint8_t mrbusAddr)
{
  *locCmdStnRef = new CmdStnLocRef(addr, isLongAddr, mrbusAddr);
  return true;
}

bool ESUCabControl::locomotiveEmergencyStop(CmdStnLocRef* locCmdStnRef)
{
  return true;
}

bool ESUCabControl::locomotiveSpeedSet(CmdStnLocRef* locCmdStnRef, uint8_t speed, bool isReverse)
{
  return true;
}

bool ESUCabControl::locomotiveFunctionsGet(CmdStnLocRef* locCmdStnRef, bool functionStates[])
{
  return true;
}

bool ESUCabControl::locomotiveFunctionSet(CmdStnLocRef* locCmdStnRef, uint8_t funcNum, bool funcActive)
{
  return true;
}

bool ESUCabControl::locomotiveDisconnect(CmdStnLocRef* locCmdStnRef)
{
  delete locCmdStnRef;
  return true;
}


