#include "WiThrottle.h"
#include "commonFuncs.h"

WiThrottle::WiThrottle()
{
  this->lnwiMode = false;
  this->cmdStnConnection = NULL;
  this->rxBuffer = new uint8_t[WITHROTTLE_RX_BUFFER_SZ];
  memset(this->rxBuffer, 0, WITHROTTLE_RX_BUFFER_SZ);
  this->rxBufferUsed = 0;
  this->keepaliveTimer.setup(10000);
}

WiThrottle::~WiThrottle()
{
  delete[] this->rxBuffer;
}

WiThrottleLocRef::WiThrottleLocRef(uint16_t locAddr, bool isLongAddr, uint8_t mrbusAddr, uint8_t multiThrottleLetter)
{
  this->locAddr = locAddr;
  this->isLongAddr = isLongAddr;
  this->mrbusAddr = mrbusAddr;
  this->multiThrottleLetter = multiThrottleLetter;
}


void WiThrottle::rxtx()
{
  this->rxtx(NULL);
}

void WiThrottle::rxtx(const char* cmdStr)
{
  if (NULL != cmdStr)
  {
    this->cmdStnConnection->write(cmdStr);
    this->keepaliveTimer.reset();
    Serial.printf("JMRI TX: [%s]\n", cmdStr);
  }

  int32_t available = 0;
  available = this->cmdStnConnection->available();
  
  if (available <= 0)
    return;

  uint32_t bytesRead = this->cmdStnConnection->readBytes(this->rxBuffer + this->rxBufferUsed, MIN(available, WITHROTTLE_RX_BUFFER_SZ - this->rxBufferUsed - 1));
  this->rxBufferUsed += bytesRead;
  if (0 == bytesRead)
    return;

  //Serial.printf("JMRI RX: [%s]\n", this->rxBuffer);

  uint8_t *ptr = this->rxBuffer;
  uint8_t *endPtr = NULL; 
  while (NULL != (endPtr = (uint8_t*)strchr((const char*)ptr, '\n')))
  {
    uint32_t len = endPtr - ptr;
    *endPtr = 0; // Replace the \n with a NULL - maybe we can process it in place?
    this->processResponse(ptr, len);

    memmove(ptr, endPtr+1, this->rxBufferUsed - (len+1));
    this->rxBufferUsed -= len+1;
    this->rxBuffer[rxBufferUsed] = 0;
  }
  return;
}

uint8_t WiThrottle::getMultiThrottleLetter(uint8_t mrbusAddr)
{
  char validLetters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  // For now, this is going to be algorithmic - it's throttle mrbus addr - base + 'A'
  uint8_t offset = mrbusAddr - MRBUS_THROTTLE_BASE_ADDR;

  if (offset >= sizeof(validLetters))  // Eh, now what?
    return 0;
  
  return validLetters[offset];
}

void WiThrottle::releaseMultiThrottleLetter(uint8_t mrbusAddr)
{
  return; // Do nothing right now since it's algorithmic
}


void WiThrottle::processResponse(const uint8_t* rxData, uint32_t rxDataLen)
{
  char* buffer = new char[rxDataLen+1];
  char *rxStr = buffer; 
  memcpy(buffer, rxData, rxDataLen);
  buffer[rxDataLen] = 0;
  rxStr = trim(rxStr);

  if (0 == strncmp(rxStr, "PPA", 3))
  {
    // Track Power State
    if(rxStr[3] == '1')
      this->trackPowerOn = true;
    else if(rxStr[3] == '0')
      this->trackPowerOn = false;
    else
      this->trackPowerOn = true;
  }
  else if (0 == strncmp(rxStr, "VN", 2))
  {
    // Protocol version
    Serial.printf("JMRI RX: Version [%s]\n", rxStr+2);
  }
  else if (0 == strncmp(rxStr, "RL", 2))
  {
    // Roster List
  }
  else if (0 == strncmp(rxStr, "PT", 2))
  {
    // Turnout List
  }
  else if (0 == strncmp(rxStr, "PR", 2))
  {
    // Turnout List
  }
  else if (0 == strncmp(rxStr, "*", 1))
  {
    // Heartbeat Interval
    this->keepaliveMaxInterval = atoi(rxStr + 1);
    if (this->keepaliveMaxInterval < 2) // Set a lower bound on this thing
      this->keepaliveMaxInterval = 2;
    this->keepaliveTimer.setup((this->keepaliveMaxInterval / 2) * 1000);
    this->keepaliveTimer.reset();
  }
  else if (0 == strncmp(rxStr, "N", 1))
  {
    // Host controller name
    Serial.printf("JMRI RX: Host Name [%s]\n", rxStr+1);
  }
  else if (0 == strncmp(rxStr, "U", 1))
  {
    // Host controller ID
    Serial.printf("JMRI RX: Host ID [%s]\n", rxStr+1);
  }
  else if (0 == strncmp(rxStr, "M", 1))
  {
    // Multithrottle update 
    // FIXME: Lots to do here
  }
  else
  {
    Serial.printf("JMRI RX:  Unhandled host->client [%s]\n", rxStr);
  }

  free(buffer);
}

bool WiThrottle::begin(WiFiClient &cmdStnConnection, uint32_t quirkFlags)
{
  this->cmdStnConnection = &cmdStnConnection;
  if (quirkFlags & WITHROTTLE_QUIRK_LNWI)
    this->lnwiMode = true;

  this->keepaliveTimer.reset();
  Serial.printf("JMRI begin()\n");

  this->rxtx("NProtoThrottle Bridge\n");
  this->rxtx("HUProtoThrottle Bridge\n");

  return true;
}

bool WiThrottle::end()
{
  this->cmdStnConnection = NULL;
  return true;
}

bool WiThrottle::update()
{
  // FIXME:  We should send a keepalive here if we have a period of inactivity
  if (this->keepaliveTimer.test(true))
    this->rxtx("*\n");
  else
    this->rxtx();


  return true;

}

bool WiThrottle::locomotiveObjectGet(CmdStnLocRef** locCmdStnRef, uint16_t addr, bool isLongAddr, uint8_t mrbusAddr)
{
  char cmdBuffer[64];

  Serial.printf("JMRI locomotiveObjectGet(%c:%04d, 0x%02X)\n", isLongAddr?'L':'S', mrbusAddr);
  uint8_t multiThrottleLetter = this->getMultiThrottleLetter(mrbusAddr);
  *locCmdStnRef = new WiThrottleLocRef(addr, isLongAddr, mrbusAddr, mrbusAddr);
  
  snprintf(cmdBuffer, sizeof(cmdBuffer), "M%c-*<;>r\n", multiThrottleLetter);
  this->rxtx(cmdBuffer);

  char longShort = isLongAddr?'L':'S';
  snprintf(cmdBuffer, sizeof(cmdBuffer), "M%c+%c%d<;>%c%d\n", multiThrottleLetter, longShort, addr, longShort, addr);
  this->rxtx(cmdBuffer);
 
  // FIXME:  Wait for function statuses from command station


  // If track power is off, turn it back on
  if (this->trackPowerOn = false)
    this->rxtx("PPA1\n");

  return true;
}

bool WiThrottle::locomotiveEmergencyStop(CmdStnLocRef* locCmdStnRef)
{
  char cmdBuffer[32];
  snprintf(cmdBuffer, sizeof(cmdBuffer), "M%cA*<;>X\n", ((WiThrottleLocRef*)locCmdStnRef)->multiThrottleLetter);
  this->rxtx(cmdBuffer);
  return true;
}

bool WiThrottle::locomotiveSpeedSet(CmdStnLocRef* locCmdStnRef, uint8_t speed, bool isReverse)
{
  // Sets the speed and direction of a locomotive via a handle that has been previously acquired with locomotiveObjectGet().  
  // Speed is 0-127, Direction is 0=forward, 1=reverse.
  Serial.printf("JMRI locomotiveSpeedSet(%d): set speed %d %s", ((WiThrottleLocRef*)locCmdStnRef)->locAddr, speed, isReverse?"REV":"FWD");

  // Bound speed to allowed values
  speed = MIN(speed, 127);

  char cmdBuffer[32];
  snprintf(cmdBuffer, sizeof(cmdBuffer), "M%cA*<;>V%d\n", ((WiThrottleLocRef*)locCmdStnRef)->multiThrottleLetter, speed);
  // Direction is 0=REV, 1=FWD on WiThrottle
  snprintf(cmdBuffer, sizeof(cmdBuffer), "M%cA*<;>R%d\n", ((WiThrottleLocRef*)locCmdStnRef)->multiThrottleLetter, isReverse?0:1);
  return true;
}

bool WiThrottle::locomotiveFunctionsGet(CmdStnLocRef* locCmdStnRef, bool functionStates[])
{
  return true;
}

bool WiThrottle::locomotiveFunctionSet(CmdStnLocRef* locCmdStnRef, uint8_t funcNum, bool funcActive)
{
  return true;
}

bool WiThrottle::locomotiveDisconnect(CmdStnLocRef* locCmdStnRef)
{
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "M%c-*<;>r\n", ((WiThrottleLocRef*)locCmdStnRef)->multiThrottleLetter);
  this->rxtx(buffer);
  this->releaseMultiThrottleLetter(locCmdStnRef->mrbusAddr);
  delete locCmdStnRef;
  return true;
}