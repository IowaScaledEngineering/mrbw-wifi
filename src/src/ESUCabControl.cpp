#include "ESUCabControl.h"
/*

ESU Cab Control Protocol Notes

query(objID, field1, field2, etc...)


Queries the loco manager object for addresses

Possible fields:
 - addr = Locomotive address
 - name = Name as stored in the ESU CC
 - protocol = 

Query:
queryObjects(10, addr)

Result:
1000 addr[5291]

Add a locomotive:
create(10, )

Acquire a locomotive:
request(objID, control, force)

Release a locomotive:
release(objID, control)

Speed:
  Get:   get(objID, speed) <- Normalized to 127 step
  Set:   set(objID, speed[val]) <- Normalized to 127 step
  Estop: set(objID, stop)  <-- Emergency stop

Direction:
  Get:   get(objID, dir) 
  Set:   set(objID, dir[val]) <- val of 1 = reverse

Functions:
  Get:   get(objID, func[n]) 
  Set:   set(objID, dir[val]) <- val of 1 = reverse

Fun fact:  you can chain all these fields.  So get(objID, speed, dir, func[0], func[1]) is perfectly allowed
The result will look like:
<REPLY get(1000,speed,dir,func[0],func[1])>
1000 speed[0]
1000 dir[0]
1000 func[0,0]
1000 func[1,0]
<END 0 (OK)>
*/


ESUCabControl::ESUCabControl()
{
  this->debug = false;
  this->cmdStnConnection = NULL;
  this->trackPowerOn = false;
  this->rxBuffer = new uint8_t[ESUCC_RX_BUFFER_SZ];
  memset(this->rxBuffer, 0, ESUCC_RX_BUFFER_SZ);
  this->rxBufferUsed = 0;
  this->keepaliveTimer.setup(10000);
  for(uint32_t i=0; i<MAX_THROTTLES; i++)
    this->throttleStates[i] = NULL;
}

ESUCabControl::~ESUCabControl()
{
  this->end();
}

ESUCCLocRef::ESUCCLocRef(uint16_t locAddr, bool isLongAddr, uint8_t mrbusAddr, int32_t objID)
{
  this->locAddr = locAddr;
  this->isLongAddr = isLongAddr;
  this->mrbusAddr = mrbusAddr;
  this->objID = objID;
}

bool ESUCabControl::begin(WiFiClient &cmdStnConnection, uint32_t quirkFlags)
{
  this->cmdStnConnection = &cmdStnConnection;
  this->keepaliveTimer.reset();
  Serial.printf("ESU CabControl begin()\n");

  // Set us up to get system-level events
  this->query("request(1, view)", NULL, NULL, 100);
  // Get our current track power state

  int32_t tpState = this->queryTrackPowerState();
  if (0 == tpState)
    this->trackPowerOn = false;
  else if (1 == tpState)
    this->trackPowerOn = true;

  return true;
}

bool ESUCabControl::end()
{
  this->cmdStnConnection = NULL;
  for(uint32_t i=0; i<MAX_THROTTLES; i++)
  {
    if(NULL != this->throttleStates[i])
      free(this->throttleStates[i]);
    this->throttleStates[i] = NULL;
  }

  if (NULL != this->rxBuffer)
    delete this->rxBuffer;
  this->rxBuffer = NULL;
  this->rxBufferUsed = 0;
  return true;
}

bool ESUCabControl::update()
{
  if (this->keepaliveTimer.test(true))
  {
    int32_t tpState = this->queryTrackPowerState();
    if (0 == tpState)
      this->trackPowerOn = false;
    else if (1 == tpState)
      this->trackPowerOn = true;
  }
  else
    this->rxtx();

  return true;
}

// Since we're subscribed to any changes in state to both our locomotives
//  being controlled and to 

void ESUCabControl::flushEvents()
{
  // FIXME:  Flush the rx buffer and handle any EVENTs by updating either throttles or system state
  memset(this->rxBuffer, 0, ESUCC_RX_BUFFER_SZ);
  this->rxBufferUsed = 0;
}


void ESUCabControl::rxtx()
{
  this->rxtx(NULL);
}

void ESUCabControl::rxtx(const char* cmdStr)
{
  if (NULL != cmdStr)
  {
    int32_t bytesToWrite = strlen(cmdStr);
    int32_t written = 0;

    while(written < bytesToWrite)
      written += this->cmdStnConnection->write(cmdStr + written, bytesToWrite-written);
    this->cmdStnConnection->flush();

    this->keepaliveTimer.reset();
    //if (this->debug)
      Serial.printf("ESU TX: [%s]\r\n", cmdStr);
  }

  int32_t available = 0;

  if (!this->cmdStnConnection->connected())
    return; // Command station not connected
  available = this->cmdStnConnection->available();

  if (available <= 0)
    return;

  uint32_t bytesRead = this->cmdStnConnection->readBytes(this->rxBuffer + this->rxBufferUsed, MIN(available, ESUCC_RX_BUFFER_SZ - this->rxBufferUsed - 1));
  this->rxBufferUsed += bytesRead;
  this->rxBuffer[this->rxBufferUsed] = 0; // Null terminate it.  This shouldn't ever be in the protocol
  if (0 != bytesRead)
    Serial.printf("ESU RX: [%s]\n", this->rxBuffer);

  return;
}


int32_t ESUCabControl::query(const char* queryStr, char** replyBuffer, int32_t* errCd, int32_t timeout_ms)
{
  PeriodicEvent queryTimeout;
  
  char* replyBeginStr = NULL;
  const char* beginPtr = NULL;
  const char* endPtr = NULL;
  const char* endEndPtr = NULL;
  int32_t queryLen = asprintf(&replyBeginStr, "<REPLY %s>", queryStr);
  int32_t responseLen = 0;

  if (NULL != errCd)
    *errCd = -1; // Since ESU uses positive integers, we'll make -1 mean "no response"


  // Send query
  char *queryWithLineEnding = NULL;
  asprintf(&queryWithLineEnding, "%s\n", queryStr);
  this->rxtx(queryWithLineEnding);
  free(queryWithLineEnding);

  // Now, our mission is to wait until either we get a response or timeout
  // A response will be in the form <REPLY queryStr>....<END num (errorstr)>
  // However, we may also see other weird stuff, like <EVENT ...> ... <END num (errorstr)>
  // Eventually this->flushEvents() should handle this for us

  // Flush rxbuffer
  this->flushEvents();

  queryTimeout.setup(timeout_ms);
  queryTimeout.reset();

  // While we're not timed out...
  while(!queryTimeout.test(false) && NULL == endEndPtr)
  {
    this->rxtx();

    if (NULL == beginPtr)
      beginPtr = strstr((char*)this->rxBuffer, replyBeginStr);

    if (NULL == beginPtr)
      continue;

    // Okay, we found the start - do we have an end?
    if (NULL == endPtr)
      endPtr = strstr(beginPtr, "<END ");

    if (NULL == endPtr)
      continue;

    // Look for the end of the end tag
    endEndPtr = strchr(endPtr, '>');
    // At this point, we have a fully formed response?

    // If we don't have a response yet, go wait for a while and let the CPU go do other things, like
    // say wifi or USB
//    if (NULL == endEndPtr)
//      delayMicroseconds(100);

  }

  // We're done with the temporary replyBeginStr here, free it up
  if (NULL != replyBeginStr)
    free(replyBeginStr);

  if (NULL != endEndPtr)
  {
    // Increment beginning pointer over the first query
    beginPtr += queryLen;
    while (!isprint(*beginPtr))
      beginPtr++; // And increment past the first CR/LF
    responseLen = endPtr - beginPtr;
    if (NULL != replyBuffer)
    {
      *replyBuffer = (char*)calloc(responseLen+1, 1);
      memcpy(*replyBuffer, beginPtr, responseLen);
    }
//    Serial.printf("Got response [%*.*s]\n", endPtr - (beginPtr+queryLen), endPtr - (beginPtr+queryLen), beginPtr+queryLen);

    if (NULL != errCd)
      *errCd = atoi(endPtr + 5); // Skip over "<END " and get the numeric error code
  }
  else
  {
    Serial.printf("ESU RXTX Timeout\n");
    if (NULL != replyBuffer)
    {
      // Even if we time out, code further down may still expect a reply buffer
      *replyBuffer = (char*)calloc(1, 1);
      responseLen = 0;
    }

    if (NULL != errCd)
      *errCd = -1;
  }

//  Serial.printf("rxtx  replyBuffer=%p beginPtr=%p endPtr=%p endEndPtr=%p queryLen=%d\n", replyBuffer, beginPtr, endPtr, endEndPtr, queryLen);
  return responseLen;
}

int32_t ESUCabControl::queryLocomotiveObjectGet(uint16_t locAddr)
{
  char queryStr[256];
  char* responseStr = NULL;
  int32_t responseLen = 0;
  int32_t objID = -1;
  int32_t errCd = -1;

  if (this->debug)
    Serial.printf("ESU queryLocomotiveObjectGet: Search for loco [%d]\n", locAddr);

  snprintf(queryStr, sizeof(queryStr)-1, "queryObjects(10, addr)");

  responseLen = this->query(queryStr, &responseStr, &errCd, 2000);

  if (responseLen > 0 && NULL != responseStr && 0 == errCd)
  {
    Serial.printf("ESU locomotiveObjectGet: Got response, errCode=%d\n", errCd);
    char* bolPtr = responseStr;
    char* eolPtr = bolPtr;

    while (*bolPtr != 0 && NULL != (eolPtr = strchr(bolPtr, '\n')) && eolPtr < responseStr + responseLen)
    {
      //Serial.printf("responseStr=%p responseLen=%d bolPtrOff=%d eolPtrOff=%d\n", responseStr, responseLen, bolPtr - responseStr, eolPtr - responseStr);
      if (eolPtr > bolPtr) // If there's something here other than a newline
      {
        uint32_t rObjID = 0;
        uint32_t rLocAddr = 0;

        *eolPtr = 0; // Null terminate in place, don't waste a copy
        // Parse this line, see if it's the locomotive we're looking for
        //  Valid responses will be in the form "n addr[n]"

        if (2 == sscanf(bolPtr, "%u addr[%u]", &rObjID, &rLocAddr))
        {
          // Line parsed with right number of arguments
          if (rLocAddr == locAddr)
          {
            objID = rObjID;
            break;
          }
        }
      }
      bolPtr = eolPtr+1;
    }
  }

  if (NULL != responseStr)
    free(responseStr);

  // Handles errors coming back from the query, such as timeouts
  if (errCd < 0)
    objID = -2; // Timeout

  if (this->debug)
  {
    if (-2 == objID)
      Serial.printf("ESU queryLocomotiveObjectGet: TIMED OUT\n");
    else if (-1 == objID)
      Serial.printf("ESU queryLocomotiveObjectGet: Loco [%d] not found\n", locAddr);
    else
      Serial.printf("ESU queryLocomotiveObjectGet: Loco [%d] is objID [%d]\n", locAddr, objID);
  }
  return objID;
}

int32_t ESUCabControl::queryAddLocomotiveObject(uint16_t locAddr)
{
  char queryStr[256];
  char* responseStr = NULL;
  int32_t responseLen = 0;
  int32_t objID = -1;
  int32_t errCd = -1;

  if (this->debug)
    Serial.printf("ESU queryAddLocomotiveObject: Add loco [%d]\n", locAddr);

  snprintf(queryStr, sizeof(queryStr)-1, "create(10, addr[%d], append)", locAddr);
  responseLen = this->query(queryStr, &responseStr, &errCd);

  if (responseLen > 0 && NULL != responseStr && 0 == errCd)
  {
    //Serial.printf("ESU queryAddLocomotiveObject: Got response, errCd = %d\n", errCd);
    char* bolPtr = responseStr;
    char* eolPtr;
    while (*bolPtr != 0 && NULL != (eolPtr = strchr(bolPtr, '\n')) && eolPtr < responseStr + responseLen)
    {
      if (eolPtr > bolPtr) // If there's something here other than a newline
      {
        uint32_t rObjID = 0;

        *eolPtr = 0; // Null terminate in place, don't waste a copy
        // Parse this line, see if it's the locomotive we're looking for
        //  Valid responses will be in the form "10 id[n]"
        if (1 == sscanf(bolPtr, "10 id[%u]", &rObjID))
        {
          // Line parsed with correct number of arguments
          //Serial.printf("ESU queryAddLocomotiveObject: rObjID=%u\n", rObjID);
          objID = rObjID;
          break;
        }
      }
      bolPtr = eolPtr+1;
    }
  }

  if (NULL != responseStr)
    free(responseStr);

  if (this->debug)
  {
    if (-1 == errCd)
      Serial.printf("ESU queryAddLocomotiveObject: TIMED OUT\n");
    else if (0 != errCd)
      Serial.printf("ESU queryAddLocomotiveObject: QUERY ERROR  [%d]\n", errCd);
    else
      Serial.printf("ESU queryAddLocomotiveObject: Loco [%d] is objID [%d]\n", locAddr, objID);
  }

  return objID;
}


bool ESUCabControl::queryLocomotiveObjectFunctionGet(int32_t objID, uint8_t funcNum)
{
  char queryStr[256];
  char* responseStr = NULL;
  int32_t responseLen = 0;
  int32_t errCd = -1;
  bool retval = false;

  if (this->debug)
    Serial.printf("ESU queryLocomotiveObjectFunctionGet: objID[%d] f[%d]\n", objID, funcNum);

  snprintf(queryStr, sizeof(queryStr)-1, "get(%d, func[%d])", objID, funcNum);
  responseLen = this->query(queryStr, &responseStr, &errCd);

  if (responseLen > 0 && NULL != responseStr && 0 == errCd)
  {
    //Serial.printf("ESU queryLocomotiveObjectFunctionGet: Got response, errCd = %d\n", errCd);
    char* bolPtr = responseStr;
    char* eolPtr;
    while (*bolPtr != 0 && NULL != (eolPtr = strchr(bolPtr, '\n')) && eolPtr < responseStr + responseLen)
    {
      if (eolPtr > bolPtr) // If there's something here other than a newline
      {
        uint32_t rObjID = 0;
        uint32_t rFuncNum= 0;
        uint32_t rFuncVal= 0;

        *eolPtr = 0; // Null terminate in place, don't waste a copy
        if (3 == sscanf(bolPtr, "%u func[%u,%u]", &rObjID, &rFuncNum, &rFuncVal))
        {
          //Serial.printf("ESU queryLocomotiveObjectFunctionGet: rObjID=%u, rFuncNum=%u, rFuncVal=%u\n", rObjID, rFuncNum, rFuncVal);
          
          if (rObjID == objID && rFuncNum == funcNum)
          {
            retval = rFuncVal?true:false;
            break;
          }
        }
      }
      bolPtr = eolPtr+1;
    }
  }

  if (NULL != responseStr)
    free(responseStr);

  if (this->debug)
  {
    if (-1 == errCd)
      Serial.printf("ESU queryLocomotiveObjectFunctionGet: TIMED OUT\n");
    else if (0 != errCd)
      Serial.printf("ESU queryLocomotiveObjectFunctionGet: QUERY ERROR  [%d]\n", errCd);
    else
      Serial.printf("ESU queryLocomotiveObjectFunctionGet: objID[%d] f[%d]=%d\n", objID, funcNum, retval?1:0);
  }
  return retval;
}

int32_t ESUCabControl::queryTrackPowerState()
{
  char* responseStr = NULL;
  int32_t responseLen = 0;
  int32_t errCd = -1;
  int32_t retval = -1;

  if (this->debug)
    Serial.printf("ESU queryTrackPowerState\n");

  responseLen = this->query("get(1, status)", &responseStr, &errCd);

  if (responseLen > 0 && NULL != responseStr && 0 == errCd)
  {
    char* bolPtr = responseStr;
    char* eolPtr;
    while (*bolPtr != 0 && NULL != (eolPtr = strchr(bolPtr, '\n')) && eolPtr < responseStr + responseLen)
    {
      if (eolPtr > bolPtr) // If there's something here other than a newline
      {
        *eolPtr = 0; // Null terminate in place, don't waste a copy

        if (0 == strcmp("1 status[GO]", bolPtr))
        {
          retval = 1;
          break;
        }
        else if (0 == strcmp("1 status[STOP]", bolPtr))
        {
          retval = 0;
          break;
        }
      }
      bolPtr = eolPtr+1;
    }
  }

  if (NULL != responseStr)
    free(responseStr);

  if (this->debug)
  {
    if (-1 == errCd)
      Serial.printf("ESU queryTrackPowerState: TIMED OUT\n");
    else if (0 != errCd)
      Serial.printf("ESU queryTrackPowerState: QUERY ERROR  [%d]\n", errCd);
    else
      Serial.printf("ESU queryTrackPowerState: %s\n", retval?"ON":"OFF");
  }
  return retval;
}



bool ESUCabControl::queryAcquireLocomotiveObject(int32_t objID)
{
  char queryStr[256];
  int32_t errCd = -1;

  if (this->debug)
    Serial.printf("ESU queryAcquireLocomotiveObject: objID[%d]\n", objID);

  snprintf(queryStr, sizeof(queryStr)-1, "request(%d, control, force)", objID);
  this->query(queryStr, NULL, &errCd);

  if (this->debug)
  {
    if (-1 == errCd)
      Serial.printf("ESU queryAcquireLocomotiveObject: TIMED OUT\n");
    else if (0 != errCd)
      Serial.printf("ESU queryAcquireLocomotiveObject: QUERY ERROR  [%d]\n", errCd);
    else
      Serial.printf("ESU queryAcquireLocomotiveObject: objID[%d] acquired control\n", objID);
  }

  if (0 == errCd)
    return true;

  return false;
}


bool ESUCabControl::queryLocomotiveObjectSpeedSet(int32_t objID, uint8_t speed, bool isReverse)
{
  char queryStr[256];
  int32_t errCd = -1;

  if (this->debug)
    Serial.printf("ESU queryLocomotiveObjectSpeedSet: objID[%d] speed[%c:%d]\n", objID, isReverse?'R':'F', speed);

  speed = MIN(126, speed);

  snprintf(queryStr, sizeof(queryStr)-1, "set(%d, speed[%d], dir[%d])", objID, speed, isReverse?1:0);
  this->query(queryStr, NULL, &errCd);

  if (this->debug)
  {
    if (-1 == errCd)
      Serial.printf("ESU queryLocomotiveObjectSpeedSet: TIMED OUT\n");
    else if (0 != errCd)
      Serial.printf("ESU queryLocomotiveObjectSpeedSet: QUERY ERROR  [%d]\n", errCd);
    else
      Serial.printf("ESU queryLocomotiveObjectSpeedSet: objID[%d] speed set\n", objID);
  }
  if (0 == errCd)
    return true;

  return false;
}

bool ESUCabControl::queryLocomotiveObjectFunctionSet(int32_t objID, uint8_t funcNum, bool funcVal)
{
  char queryStr[256];
  int32_t errCd = -1;

  if (this->debug)
    Serial.printf("ESU queryLocomotiveObjectFunctionSet: objID[%d] f[%d]=%d\n", objID, funcNum, funcVal?1:0);

  funcNum = MIN(MAX_FUNCTIONS, funcNum);

  snprintf(queryStr, sizeof(queryStr)-1, "set(%d, func[%d,%d])", objID, funcNum, funcVal?1:0);
  this->query(queryStr, NULL, &errCd);

  if (this->debug)
  {
    if (-1 == errCd)
      Serial.printf("ESU queryLocomotiveObjectFunctionSet: TIMED OUT\n");
    else if (0 != errCd)
      Serial.printf("ESU queryLocomotiveObjectFunctionSet: QUERY ERROR  [%d]\n", errCd);
    else
      Serial.printf("ESU queryLocomotiveObjectFunctionSet: objID[%d] f[%d] set success\n", objID, funcNum);
  }

  if (0 == errCd)
    return true;

  return false;
}


bool ESUCabControl::queryLocomotiveObjectEmergencyStop(int32_t objID)
{
  char queryStr[256];
  int32_t errCd = -1;

  if (this->debug)
    Serial.printf("ESU queryLocomotiveObjectEmergencyStop: objID[%d] ESTOP\n", objID);

  snprintf(queryStr, sizeof(queryStr)-1, "set(%d, stop)", objID);
  this->query(queryStr, NULL, &errCd);

  if (this->debug)
  {
    if (-1 == errCd)
      Serial.printf("ESU queryLocomotiveObjectEmergencyStop: TIMED OUT\n");
    else if (0 != errCd)
      Serial.printf("ESU queryLocomotiveObjectEmergencyStop: QUERY ERROR  [%d]\n", errCd);
    else
      Serial.printf("ESU queryLocomotiveObjectEmergencyStop: objID[%d] ESTOP success\n", objID);
  }

  if (0 == errCd)
    return true;

  return false;
}


bool ESUCabControl::queryReleaseLocomotiveObject(int32_t objID)
{
  char queryStr[256];
  int32_t errCd = -1;

  if (this->debug)
    Serial.printf("ESU queryReleaseLocomotiveObject: objID[%d] release\n", objID);

  snprintf(queryStr, sizeof(queryStr)-1, "release(%d, control)", objID);
  this->query(queryStr, NULL, &errCd);

  if (this->debug)
  {
    if (-1 == errCd)
      Serial.printf("ESU queryReleaseLocomotiveObject: TIMED OUT\n");
    else if (0 != errCd)
      Serial.printf("ESU queryReleaseLocomotiveObject: QUERY ERROR  [%d]\n", errCd);
    else
      Serial.printf("ESU queryReleaseLocomotiveObject: objID[%d] release success\n", objID);
  }

  if (0 == errCd)
    return true;

  return false;
}

bool ESUCabControl::locomotiveObjectGet(ThrottleState* tState, uint16_t addr, bool isLongAddr, uint8_t mrbusAddr)
{
  Serial.printf("ESU:locomotiveObjectGet start [%c:%d]\n", isLongAddr?'L':'S', addr);

  if (NULL != tState->locCmdStnRef)
  {
    free(tState->locCmdStnRef);
    tState->locCmdStnRef = NULL;
  }

  // Let's start with if track power is on or not.  If it's off, turn it ON!
  if (false == this->trackPowerOn)
    this->query("set(1, go)", NULL, NULL, 100);

  int32_t objID = this->queryLocomotiveObjectGet(addr);

  if (-2 == objID)
  {
    Serial.printf("ESU:locomotiveObjectGet object query timeout\n");
    return false;  // We didn't acquire it because our transaction timed out.  Don't blindly add unless we have a good transaction
  }
  // Didn't find it?
  if (-1 == objID)
  {
    Serial.printf("ESU:locomotiveObjectGet loc not found, added\n");
    objID = this->queryAddLocomotiveObject(addr);

  }
  if (-1 == objID)
    return false;  // Nothing else we can do here except fail!*/

  Serial.printf("ESU:locomotiveObjectGet [%c:%d]=objID[%d]\n", isLongAddr?'L':'S', addr, objID);

  tState->locCmdStnRef = new ESUCCLocRef(addr, isLongAddr, mrbusAddr, objID);

  // Invalidate the function states
  tState->locFunctionsGood = false;
  for(uint32_t i=0; i<MAX_FUNCTIONS; i++)
    tState->locFunctions[i] = false;
  tState->isLongAddr = isLongAddr;
  tState->locAddr = addr;

  // Now, forcably take over the locomotive object
  this->queryAcquireLocomotiveObject(objID);

  // Get current speed/direction
  // FIXME

  // Get current function states
  for(uint32_t funcNum=0; funcNum < MAX_FUNCTIONS; funcNum++)
  {
    tState->locFunctions[funcNum] = this->queryLocomotiveObjectFunctionGet(objID, funcNum);
  }
  tState->locFunctionsGood = true;

  Serial.printf("ESU:locomotiveObjectGet success\n");

  return true;
}

bool ESUCabControl::locomotiveEmergencyStop(ThrottleState* tState)
{
  ESUCCLocRef* esuLocRef = (ESUCCLocRef*)tState->locCmdStnRef;
  if (NULL == esuLocRef)
    return false;

  bool success = queryLocomotiveObjectEmergencyStop(esuLocRef->objID);
  if (success)
  {
    tState->locSpeed = 0;
    tState->locEStop = true;
  }
  return true;
}

bool ESUCabControl::locomotiveSpeedSet(ThrottleState* tState, uint8_t speed, bool isReverse)
{
  ESUCCLocRef* esuLocRef = (ESUCCLocRef*)tState->locCmdStnRef;
  if (NULL == esuLocRef)
    return false;

  bool success = this->queryLocomotiveObjectSpeedSet(esuLocRef->objID, speed, isReverse);
  if (success)
  {
    tState->locEStop = false;
    tState->locSpeed = speed;
    tState->locRevDirection = isReverse;
  }

  return true;
}

bool ESUCabControl::locomotiveFunctionsGet(ThrottleState* tState, bool functionStates[])
{
  ESUCCLocRef* esuLocRef = (ESUCCLocRef*)tState->locCmdStnRef;
  if (NULL == esuLocRef)
    return false;
 
  for(uint32_t funcNum=0; funcNum < MAX_FUNCTIONS; funcNum++)
  {
    functionStates[funcNum] = this->queryLocomotiveObjectFunctionGet(esuLocRef->objID, funcNum);
  }

  return true;
}

bool ESUCabControl::locomotiveFunctionSet(ThrottleState* tState, uint8_t funcNum, bool funcActive)
{
  ESUCCLocRef* esuLocRef = (ESUCCLocRef*)tState->locCmdStnRef;
  if (NULL == esuLocRef)
    return false;

  bool success = this->queryLocomotiveObjectFunctionSet(esuLocRef->objID, funcNum, funcActive);
  if (success)
    tState->locFunctions[funcNum] = funcActive;
  return success;
}

bool ESUCabControl::locomotiveDisconnect(ThrottleState* tState)
{
  if (NULL != tState->locCmdStnRef)
  {
    ESUCCLocRef* esuLocRef = (ESUCCLocRef*)tState->locCmdStnRef;
    this->queryReleaseLocomotiveObject(esuLocRef->objID);
    delete esuLocRef;
  }

  tState->locCmdStnRef = NULL;
  tState->locAddr = 0;
  tState->isLongAddr = false;
  tState->locSpeed = 0;
  tState->locRevDirection = false;
  tState->locEStop = false;

  return true;
}


