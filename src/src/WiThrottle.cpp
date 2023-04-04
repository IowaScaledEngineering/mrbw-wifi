#include "WiThrottle.h"

WiThrottle::WiThrottle()
{
  this->lnwiMode = false;
  this->cmdStnConnection = NULL;
}

bool WiThrottle::begin(WiFiClient &cmdStnConnection, uint32_t quirkFlags)
{
  this->cmdStnConnection = &cmdStnConnection;
  if (quirkFlags & WITHROTTLE_QUIRK_LNWI)
    this->lnwiMode = true;
  return true;
}

bool WiThrottle::end()
{
  this->cmdStnConnection = NULL;
  return true;
}



bool WiThrottle::locomotiveObjectGet(CmdStnLocRef* locCmdStnRef, uint16_t addr, bool isLongAddr, uint8_t mrbusAddr)
{
  return true;
}

bool WiThrottle::locomotiveEmergencyStop(CmdStnLocRef locCmdStnRef)
{
  return true;
}

bool WiThrottle::locomotiveSpeedSet(CmdStnLocRef locCmdStnRef, uint8_t speed, bool isReverse)
{
  return true;
}

bool WiThrottle::locomotiveFunctionsGet(CmdStnLocRef locCmdStnRef, bool functionStates[])
{
  return true;
}

bool WiThrottle::locomotiveFunctionSet(CmdStnLocRef locCmdStnRef, uint8_t funcNum, bool funcActive)
{
  return true;
}
