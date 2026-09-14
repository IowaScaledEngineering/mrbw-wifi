#include "WiThrottle.h"
#include "commonFuncs.h"

WiThrottle::WiThrottle()
{
  WiThrottle(NULL);
}

WiThrottle::WiThrottle(const char* additionalIdentStr)
{
  memset(this->additionalIdentStr, 0, sizeof(this->additionalIdentStr));
  this->debug = DBGLVL_INFO;
  this->fastClock = NULL;
  this->lnwiMode = false;
  this->dccexMode = false;
  this->cmdStnConnection = NULL;
  this->rxBuffer = new uint8_t[WITHROTTLE_RX_BUFFER_SZ];
  memset(this->rxBuffer, 0, WITHROTTLE_RX_BUFFER_SZ);
  this->rxBufferUsed = 0;
  this->keepaliveTimer.setup(10000);
  for(uint32_t i=0; i<MAX_THROTTLES; i++)
    this->throttleStates[i] = NULL;

  if (NULL != additionalIdentStr)
    strncpy(this->additionalIdentStr, additionalIdentStr, sizeof(this->additionalIdentStr)-1);

}

WiThrottle::~WiThrottle()
{
  delete[] this->rxBuffer;
}

static bool splitWiThrottleCommandPayload(char* line, char** commandOut, char** payloadOut)
{
  char* payloadSep = strstr(line, "<;>");
  if (NULL == payloadSep)
    return false;

  *payloadSep = '\0';
  *commandOut = line;
  *payloadOut = payloadSep + 3;
  return true;
}

void WiThrottle::resetRxBuffer()
{
  memset(this->rxBuffer, 0, WITHROTTLE_RX_BUFFER_SZ);
  this->rxBufferUsed = 0;
}

void WiThrottle::consumeRxBuffer()
{
  uint8_t* ptr = this->rxBuffer;
  while (true)
  {
    uint8_t* endPtr = (uint8_t*)strchr((const char*)ptr, '\n');
    if (NULL == endPtr)
      break;

    uint32_t len = endPtr - ptr;
    uint32_t consumeLen = len + 1;
    if (len >= WITHROTTLE_LINE_BUFFER_SZ)
    {
      if (IS_DBGLVL_WARN)
        Serial.printf("[WTHR]: RX line too large, trimming\n");

      memmove(ptr, endPtr + 1, this->rxBufferUsed - consumeLen);
      this->rxBufferUsed -= consumeLen;
      this->rxBuffer[this->rxBufferUsed] = 0;
      continue;
    }

    if (len > 0)
    {
      char lineBuffer[WITHROTTLE_LINE_BUFFER_SZ];
      memcpy(lineBuffer, ptr, len);
      lineBuffer[len] = 0;
      if (IS_DBGLVL_DEBUG)
        Serial.printf("[WTHR]: RX [%s]\n", lineBuffer);
      this->processResponse((const uint8_t*)lineBuffer, len);
    }

    memmove(ptr, endPtr + 1, this->rxBufferUsed - consumeLen);
    this->rxBufferUsed -= consumeLen;
    this->rxBuffer[this->rxBufferUsed] = 0;
  }
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
    if (IS_DBGLVL_DEBUG)
      Serial.printf("[WTHR]: TX [%s]\n", cmdStr);
  }

  while (this->cmdStnConnection->available() > 0)
  {
    if (this->rxBufferUsed >= WITHROTTLE_RX_BUFFER_SZ - 1)
    {
      if (IS_DBGLVL_WARN)
        Serial.printf("[WTHR]: RX buffer full, dumping\n");
      this->resetRxBuffer();
    }

    uint32_t spaceRemaining = WITHROTTLE_RX_BUFFER_SZ - this->rxBufferUsed - 1;
    if (spaceRemaining == 0)
    {
      this->resetRxBuffer();
      continue;
    }

    uint32_t bytesAvailable = (uint32_t)this->cmdStnConnection->available();
    uint32_t bytesToRead = MIN(bytesAvailable, spaceRemaining);
    uint32_t bytesRead = this->cmdStnConnection->readBytes(this->rxBuffer + this->rxBufferUsed, bytesToRead);
    if (0 == bytesRead)
      break;

    this->rxBufferUsed += bytesRead;
    this->rxBuffer[this->rxBufferUsed] = 0;
    this->consumeRxBuffer();
  }
}

uint8_t WiThrottle::getMultiThrottleLetter(uint8_t mrbusAddr, ThrottleState* tState)
{
  // For now, this is going to be algorithmic - it's throttle mrbus addr - base + 'A'
  uint8_t offset = mrbusAddr - MRBUS_THROTTLE_BASE_ADDR;

  if (offset >= MAX_THROTTLES)  // Eh, now what?
    return 0;
  
  this->throttleStates[offset] = tState;
  return this->validThrottleLetters[offset];
}

void WiThrottle::releaseMultiThrottleLetter(uint8_t mrbusAddr)
{
  uint8_t offset = mrbusAddr - MRBUS_THROTTLE_BASE_ADDR;
  this->throttleStates[offset] = NULL;
  return;
}


bool WiThrottle::handlePowerUpdate(const char* rxStr)
{
  if (3 > strlen(rxStr))
    return false;

  if (rxStr[3] == '1')
    this->trackPowerOn = true;
  else if (rxStr[3] == '0')
    this->trackPowerOn = false;
  else
    this->trackPowerOn = true;

  return true;
}

bool WiThrottle::handleFastClockUpdate(const char* rxStr)
{
  if (NULL == this->fastClock)
  {
    if (IS_DBGLVL_INFO)
      Serial.printf("[WTHR]: Fast clock [%s] rx, unused\n", rxStr);
    return true;
  }

  uint32_t seconds = 0;
  float ratioFloat = 0.0;

  if (2 != sscanf(rxStr, "PFT%lu<;>%f", &seconds, &ratioFloat))
  {
    if (IS_DBGLVL_INFO)
      Serial.printf("[WTHR]: looked like fast time, didn't parse [%s]\n", rxStr);
    return false;
  }

  seconds %= 86400;
  uint32_t ratio = ratioFloat * 1000;

  if (IS_DBGLVL_INFO)
    Serial.printf("[WTHR]: Got fast time of %02lu:%02lu, ratio %lu\n", seconds / 3600, (seconds / 60) % 60, ratio);

  if (0 == ratio)
  {
    this->fastClock->stop();
    this->fastClock->setTime(seconds, ratio);
  }
  else
  {
    this->fastClock->setTime(seconds, ratio);
    this->fastClock->start();
  }

  return true;
}

bool WiThrottle::handleMultithrottleUpdate(char* rxStr)
{
  if (IS_DBGLVL_INFO)
    Serial.printf("[WTHR]: RX Multithrottle Update [%s]\n", rxStr);

  char* cmd = NULL;
  char* data = NULL;
  if (!splitWiThrottleCommandPayload(rxStr, &cmd, &data))
  {
    if (IS_DBGLVL_WARN)
      Serial.printf("[WTHR]: RX malformed multithrottle update [%s]\n", rxStr);
    return false;
  }

  size_t cmdLen = strlen(cmd);
  size_t dataLen = strlen(data);
  if (cmdLen < 3)
  {
    if (IS_DBGLVL_WARN)
      Serial.printf("[WTHR]: RX multithrottle command too short [%s]\n", rxStr);
    return false;
  }

  ThrottleState* tState = this->getThrottleStateForMultiThrottleLetter(cmd[1]);
  if (NULL == tState)
    return true;

  if (IS_DBGLVL_DEBUG)
    Serial.printf("[%s] -> [%s]\n", cmd, data);

  if ('S' == cmd[2])
  {
    char cmdBuffer[32];
    snprintf(cmdBuffer, sizeof(cmdBuffer), "%s\n", rxStr);
    if (IS_DBGLVL_INFO)
      Serial.printf("[WTHR]: TX Steal [%s]\n", cmdBuffer);
    this->cmdStnConnection->write(cmdBuffer);
    return true;
  }

  if ('A' != cmd[2])
    return true;

  if (dataLen >= 3 && 'F' == data[0])
  {
    char* endPtr = NULL;
    uint8_t funcNum = (uint8_t)strtoul(data + 2, &endPtr, 10);
    if (endPtr == data + 2 || '\0' != *endPtr)
      return true;

    bool funcVal = ('1' == data[1]) ? true : false;
    if (funcNum < MAX_FUNCTIONS)
      tState->locFunctions[funcNum] = funcVal;

    if (funcNum >= 28)
      tState->locFunctionsGood = true;

    if (IS_DBGLVL_DEBUG)
      Serial.printf("[WTHR]: RX Throttle %c func %02d = %d\n", cmd[1], funcNum, tState->locFunctions[funcNum] ? 1 : 0);

    return true;
  }

  if (dataLen >= 2 && 'V' == data[0])
  {
    char* endPtr = NULL;
    uint8_t speed = (uint8_t)strtoul(data + 1, &endPtr, 10);
    if (endPtr != data + 1)
      tState->locSpeed = speed;
    if (IS_DBGLVL_DEBUG)
      Serial.printf("[WTHR]: RX Throttle %c speed %d\n", cmd[1], tState->locSpeed);
    return true;
  }

  if (dataLen >= 2 && 'R' == data[0])
  {
    tState->locRevDirection = ('0' != data[1]);
    if (IS_DBGLVL_DEBUG)
      Serial.printf("[WTHR]: RX Throttle %c direction %c\n", cmd[1], tState->locRevDirection ? 'R' : 'F');
    return true;
  }

  if (IS_DBGLVL_WARN)
    Serial.printf("[WTHR]: RX Unhandled multithrottle Update [%s]\n", rxStr);

  return true;
}

void WiThrottle::processResponse(const uint8_t* rxData, uint32_t rxDataLen)
{
  char buffer[WITHROTTLE_LINE_BUFFER_SZ];
  char *rxStr = buffer;
  if (rxDataLen >= sizeof(buffer))
    return;

  memcpy(buffer, rxData, rxDataLen);
  buffer[rxDataLen] = 0;
  rxStr = trim(rxStr);

  if (0 == strlen(rxStr))
    return;

  if (IS_DBGLVL_DEBUG)
    Serial.printf("[WTHR]: RX processResponse [%s]\n", rxStr);

  // Oh Digitrax, you are so very special.  You send us gibberish that we have to ignore, and it is not even a valid command.
  // It looks like this:  AT+CIPSENDBUF=0,1
  if (0 == strncmp(rxStr, "AT+CIPSENDBUF", 13))
  {
    if (IS_DBGLVL_DEBUG)
      Serial.printf("[WTHR]: LNWI Gibberish: [%s]\n", rxStr);
    uint32_t len = strlen(rxStr) - 13;
    memmove(rxStr, rxStr + 13, len);
    rxStr[len] = 0;
  }

  if (0 == strncmp(rxStr, "PPA", 3))
  {
    this->handlePowerUpdate(rxStr);
  }
  else if (0 == strncmp(rxStr, "VN", 2))
  {
    if (IS_DBGLVL_DEBUG)
      Serial.printf("[WTHR]: WiThrottle Protocol Version [%s]\n", rxStr+2);
  }
  else if (0 == strncmp(rxStr, "HT", 2))
  {
    if (IS_DBGLVL_INFO)
      Serial.printf("[WTHR]: Server type [%s]\n", rxStr+2);
  }
  else if (0 == strncmp(rxStr, "Ht", 2))
  {
    if (IS_DBGLVL_INFO)
      Serial.printf("[WTHR]: Server description [%s]\n", rxStr+2);
  }
  else if (0 == strncmp(rxStr, "HM", 2))
  {
    if (IS_DBGLVL_INFO)
      Serial.printf("[WTHR]: Server alert message [%s]\n", rxStr+2);
  }
  else if (0 == strncmp(rxStr, "Hm", 2))
  {
    if (IS_DBGLVL_INFO)
      Serial.printf("[WTHR]: Server info message [%s]\n", rxStr+2);
  }
  else if (0 == strncmp(rxStr, "RL", 2))
  {
    if (IS_DBGLVL_INFO)
      Serial.printf("[WTHR]: Got roster list, ignoring\n");
    // Roster List
  }
  else if (0 == strncmp(rxStr, "PT", 2))
  {
    if (IS_DBGLVL_INFO)
      Serial.printf("[WTHR]: Got turnout list, ignoring\n");
  }
  else if (0 == strncmp(rxStr, "PR", 2))
  {
    if (IS_DBGLVL_INFO)
      Serial.printf("[WTHR]: Got route list, ignoring\n");
  }
  else if (0 == strncmp(rxStr, "*", 1))
  {
    this->keepaliveMaxInterval = atoi(rxStr + 1);
    if (this->keepaliveMaxInterval < 2)
      this->keepaliveMaxInterval = 2;
    this->keepaliveTimer.setup((this->keepaliveMaxInterval / 2) * 1000);
    this->keepaliveTimer.reset();
    if (IS_DBGLVL_INFO)
      Serial.printf("[WTHR]: Heartbeat set %lus\n", this->keepaliveMaxInterval);
  }
  else if (0 == strncmp(rxStr, "N", 1))
  {
    if (IS_DBGLVL_INFO)
      Serial.printf("[WTHR]: RX Host Name [%s]\n", rxStr+1);
  }
  else if (0 == strncmp(rxStr, "U", 1))
  {
    if (IS_DBGLVL_INFO)
      Serial.printf("[WTHR]: RX Host ID [%s]\n", rxStr+1);
  }
  else if (0 == strncmp(rxStr, "M", 1))
  {
    this->handleMultithrottleUpdate(rxStr);
  }
  else if (0 == strncmp(rxStr, "PFT", 3))
  {
    this->handleFastClockUpdate(rxStr);
  }
  else if (IS_DBGLVL_WARN)
  {
    Serial.printf("[WTHR]: RX Unhandled host->client [%s]\n", rxStr);
  }
}

bool WiThrottle::fastClockConnect(Clock* fastClock)
{
  this->fastClock = fastClock;
  return true;
}

void WiThrottle::fastClockDisconnect()
{
  this->fastClock = NULL;
}


bool WiThrottle::begin(WiFiClient &cmdStnConnection, uint32_t quirkFlags, uint8_t debugLvl)
{
  this->fastClock = NULL;
  this->debug = debugLvl;
  this->cmdStnConnection = &cmdStnConnection;
  if (quirkFlags & WITHROTTLE_QUIRK_LNWI)
    this->lnwiMode = true;
  else if (quirkFlags & WITHROTTLE_QUIRK_DCCEX)
    this->dccexMode = true;

  memset(this->rxBuffer, 0, WITHROTTLE_RX_BUFFER_SZ);
  this->rxBufferUsed = 0;
  this->keepaliveTimer.setup(10000);
  for(uint32_t i=0; i<MAX_THROTTLES; i++)
    this->throttleStates[i] = NULL;

  this->keepaliveTimer.reset();
  if (IS_DBGLVL_INFO)
    Serial.printf("[WTHR]: JMRI begin()\n");

  if (0 == strlen(this->additionalIdentStr))
  {
    this->rxtx("NProtoThrottle Bridge\n");
    this->rxtx("HUProtoThrottle Bridge\n");
  } else {
    char bridgeNameStr[128];
    snprintf(bridgeNameStr, sizeof(bridgeNameStr), "NProtoThrottle Bridge %s\n", this->additionalIdentStr);
    this->rxtx(bridgeNameStr);
    snprintf(bridgeNameStr, sizeof(bridgeNameStr), "HUProtoThrottle Bridge %s\n", this->additionalIdentStr);
    this->rxtx(bridgeNameStr);
  }
  // Turn on heartbeat monitoring for the connection
  this->rxtx("*+\n");
  return true;
}

bool WiThrottle::end()
{
  for(uint32_t i=0; i<MAX_THROTTLES; i++)
  {
    if (NULL != this->throttleStates[i] && NULL != this->throttleStates[i]->locCmdStnRef)
    {
      delete this->throttleStates[i]->locCmdStnRef;
      this->throttleStates[i]->locCmdStnRef = NULL;
    }
    this->throttleStates[i] = NULL;
  }

  if (NULL != this->rxBuffer)
    delete[] this->rxBuffer;
  this->rxBuffer = NULL;
  this->rxBufferUsed = 0;

  // Nuke the command station connection pointer
  this->cmdStnConnection = NULL;
  return true;
}

bool WiThrottle::update()
{
  if (NULL == this->cmdStnConnection)
    return false;

  if (this->keepaliveMaxInterval > 0 && this->keepaliveTimer.test(true))
  {
    if (IS_DBGLVL_INFO)
      Serial.printf("[WTHR]: Send keepalive\n");
    this->rxtx("*\n");
    return true;
  }

  this->rxtx();
  return true;
}

bool WiThrottle::locomotiveObjectGet(ThrottleState* tState, uint16_t addr, bool isLongAddr, uint8_t mrbusAddr)
{
  char cmdBuffer[64];

  uint8_t multiThrottleLetter = this->getMultiThrottleLetter(mrbusAddr, tState);
  tState->locCmdStnRef = new WiThrottleLocRef(addr, isLongAddr, mrbusAddr, multiThrottleLetter);

  if (IS_DBGLVL_INFO)
    Serial.printf("[WTHR]: locomotiveObjectGet(%c%04d) mrbus=[0x%02X] letter=%c\n", isLongAddr?'L':'S', addr, mrbusAddr, multiThrottleLetter);

  snprintf(cmdBuffer, sizeof(cmdBuffer), "M%c-*<;>r\n", multiThrottleLetter);
  this->rxtx(cmdBuffer);

  // Invalidate the function states
  tState->locFunctionsGood = false;
  for(uint32_t i=0; i<MAX_FUNCTIONS; i++)
    tState->locFunctions[i] = false;


  char longShort = isLongAddr?'L':'S';
  snprintf(cmdBuffer, sizeof(cmdBuffer), "M%c+%c%d<;>%c%d\n", multiThrottleLetter, longShort, addr, longShort, addr);
  this->rxtx(cmdBuffer);
  tState->isLongAddr = isLongAddr;
  tState->locAddr = addr;

  PeriodicEvent functionWait;
  functionWait.setup(250); // Wait up to 250ms for functions, just for fun - that seems a reasonable stall
  // Keep feeding the socket so the command station can reply while we wait, but avoid a CPU spin loop.
  while (!tState->locFunctionsGood && !functionWait.test())
  {
    this->rxtx();
    yield();
  }

  // If track power is off, turn it back on
  if (false == this->trackPowerOn)
    this->rxtx("PPA1\n");

  return true;
}

bool WiThrottle::locomotiveEmergencyStop(ThrottleState* tState)
{
  WiThrottleLocRef* locCmdStnRef = (WiThrottleLocRef*)(tState->locCmdStnRef);
  char cmdBuffer[32];

  if (IS_DBGLVL_INFO)
    Serial.printf("[WTHR]: locomotiveEmergencyStop(%c%d/%c)\n", locCmdStnRef->isLongAddr?'L':'S', locCmdStnRef->locAddr, locCmdStnRef->multiThrottleLetter);

  snprintf(cmdBuffer, sizeof(cmdBuffer), "M%cA*<;>X\n", locCmdStnRef->multiThrottleLetter);
  this->rxtx(cmdBuffer);

  tState->locEStop = true;
  tState->locSpeed = 0;

  return true;
}

bool WiThrottle::locomotiveSpeedSet(ThrottleState* tState, uint8_t speed, bool isReverse)
{
  WiThrottleLocRef* locCmdStnRef = (WiThrottleLocRef*)(tState->locCmdStnRef);

  // Sets the speed and direction of a locomotive via a handle that has been previously acquired with locomotiveObjectGet().  
  // Speed is 0-127, Direction is 0=forward, 1=reverse.
  if (IS_DBGLVL_INFO)
    Serial.printf("[WTHR]: locomotiveSpeedSet(%c%d/%c): speed[%c:%d]\n", locCmdStnRef->isLongAddr?'L':'S', locCmdStnRef->locAddr, locCmdStnRef->multiThrottleLetter, isReverse?'R':'F', speed);

  // Bound speed to allowed values
  speed = MIN(speed, 127);

  char cmdBuffer[32];
  snprintf(cmdBuffer, sizeof(cmdBuffer), "M%cA*<;>V%d\n", locCmdStnRef->multiThrottleLetter, speed);
  this->rxtx(cmdBuffer);
  // Direction is 0=REV, 1=FWD on WiThrottle
  snprintf(cmdBuffer, sizeof(cmdBuffer), "M%cA*<;>R%d\n", locCmdStnRef->multiThrottleLetter, isReverse?0:1);
  this->rxtx(cmdBuffer);

  tState->locSpeed = speed;
  tState->locRevDirection = isReverse;
  tState->locEStop = false; // We're asked to send a speed, so can't be in estop any more

  return true;
}

bool WiThrottle::locomotiveFunctionsGet(ThrottleState* tState, bool functionStates[])
{
  if (NULL == tState || NULL == functionStates)
    return false;

  for (uint32_t i = 0; i < MAX_FUNCTIONS; i++)
    functionStates[i] = tState->locFunctions[i];

  return true;
}

bool WiThrottle::locomotiveFunctionSetJMRI(ThrottleState* tState, uint8_t funcNum, bool funcActive)
{
  WiThrottleLocRef* locCmdStnRef = (WiThrottleLocRef*)(tState->locCmdStnRef);
  // JMRI (and MRC, and DCC-EX, and probably most) thankfully support the "force function (f)" command as
  // described in the specification.  That makes this way easier and I can avoid having toggle things and
  // keep up with the command station state
  char cmdBuffer[32];
  snprintf(cmdBuffer, sizeof(cmdBuffer), "M%cA*<;>f%d%d\n", locCmdStnRef->multiThrottleLetter, funcActive?1:0, funcNum);
  this->rxtx(cmdBuffer);

  // Because we issued a "force" command, go ahead and just set it proactively
  tState->locFunctions[funcNum] = funcActive;

  return true;
}

bool WiThrottle::locomotiveFunctionSetLNWI(ThrottleState* tState, uint8_t funcNum, bool funcActive)
{
  if (NULL == tState || NULL == tState->locCmdStnRef)
    return false;

  if (funcNum >= MAX_FUNCTIONS)
    return false;

  WiThrottleLocRef* locCmdStnRef = (WiThrottleLocRef*)(tState->locCmdStnRef);
  char cmdBuffer[32];

  if (2 == funcNum)
  {
    // Function 2 on LNWI is non-latching and does not follow the usual toggle pattern.
    snprintf(cmdBuffer, sizeof(cmdBuffer), "M%cA*<;>F%d%d\n", locCmdStnRef->multiThrottleLetter, funcActive ? 1 : 0, funcNum);
    this->rxtx(cmdBuffer);
    tState->locFunctions[funcNum] = funcActive;
    return true;
  }

  if (tState->locFunctions[funcNum] == funcActive)
    return true;

  // For the remaining latching functions the LNWI workaround is a toggle pulse,
  // so issue the on/off sequence only when the logical output actually changes.
  snprintf(cmdBuffer, sizeof(cmdBuffer), "M%cA*<;>F1%d\n", locCmdStnRef->multiThrottleLetter, funcNum);
  this->rxtx(cmdBuffer);
  snprintf(cmdBuffer, sizeof(cmdBuffer), "M%cA*<;>F0%d\n", locCmdStnRef->multiThrottleLetter, funcNum);
  this->rxtx(cmdBuffer);
  tState->locFunctions[funcNum] = funcActive;

  return true;
}

ThrottleState *WiThrottle::getThrottleStateForMultiThrottleLetter(uint8_t letter)
{
  const char* ptr = strchr(this->validThrottleLetters, letter);

  if (NULL == ptr || (ptr - this->validThrottleLetters) >= MAX_THROTTLES)
    return NULL;
  
  return this->throttleStates[ptr - this->validThrottleLetters];
}

bool WiThrottle::locomotiveFunctionSet(ThrottleState* tState, uint8_t funcNum, bool funcActive)
{
  WiThrottleLocRef* locCmdStnRef = (WiThrottleLocRef*)(tState->locCmdStnRef);
  if (IS_DBGLVL_INFO)
    Serial.printf("[WTHR]: locomotiveFunctionSet(%c%d/%c) F%0d = %d\n", locCmdStnRef->isLongAddr?'L':'S', locCmdStnRef->locAddr, locCmdStnRef->multiThrottleLetter, funcNum, funcActive);
  if (this->lnwiMode || this->dccexMode)
    return this->locomotiveFunctionSetLNWI(tState, funcNum, funcActive);
  else
    return this->locomotiveFunctionSetJMRI(tState, funcNum, funcActive);
}

bool WiThrottle::locomotiveDisconnect(ThrottleState* tState)
{
  char cmdBuffer[32];
  WiThrottleLocRef* locCmdStnRef = (WiThrottleLocRef*)tState->locCmdStnRef;

  if (NULL != locCmdStnRef)
  {
    if (IS_DBGLVL_INFO)
      Serial.printf("[WTHR]: locomotiveDisconnect(%c%d/%c)\n", locCmdStnRef->isLongAddr?'L':'S', locCmdStnRef->locAddr, locCmdStnRef->multiThrottleLetter);

    snprintf(cmdBuffer, sizeof(cmdBuffer), "M%c-*<;>r\n", locCmdStnRef->multiThrottleLetter);
    this->rxtx(cmdBuffer);
    snprintf(cmdBuffer, sizeof(cmdBuffer), "M%c-*<;>d\n", locCmdStnRef->multiThrottleLetter);
    this->rxtx(cmdBuffer);

    this->releaseMultiThrottleLetter(locCmdStnRef->mrbusAddr);
    if (NULL != locCmdStnRef)
      delete locCmdStnRef;
    tState->locCmdStnRef = NULL;
    tState->locAddr = 0;
    tState->isLongAddr = false;
    tState->locSpeed = 0;
    tState->locRevDirection = false;
  }
 
  return true;
}