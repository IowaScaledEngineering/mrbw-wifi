#include "CommandStation.h"

CmdStnLocRef::CmdStnLocRef()
{
  return;
}

CmdStnLocRef::CmdStnLocRef(uint16_t locAddr, bool isLongAddr, uint8_t mrbusAddr)
{
  this->locAddr = locAddr;
  this->isLongAddr = isLongAddr;
  this->mrbusAddr = mrbusAddr;
}
