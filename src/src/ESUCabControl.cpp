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
  this->debug = DBGLVL_INFO;
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

bool ESUCabControl::begin(WiFiClient &cmdStnConnection, uint32_t quirkFlags, uint8_t debugLvl)
{
  this->debug = debugLvl;
  this->cmdStnConnection = &cmdStnConnection;
  this->keepaliveTimer.reset();
  if (IS_DBGLVL_INFO)
    Serial.printf("[ESU]: ESU CabControl begin()\n");

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
  if (IS_DBGLVL_INFO)
    Serial.printf("[ESU]: ESU CabControl end()\n");

  for(uint32_t i=0; i<MAX_THROTTLES; i++)
  {
    if(NULL != this->throttleStates[i] && NULL != this->throttleStates[i]->locCmdStnRef)
    {
      free(this->throttleStates[i]->locCmdStnRef);
      this->throttleStates[i]->locCmdStnRef = NULL;
    }
    this->throttleStates[i] = NULL;
  }

  this->cmdStnConnection = NULL;

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
  {
    this->rxtx();
  }
  this->handleEvents();
  return true;
}

// Since we're subscribed to any changes in state to both our locomotives
//  being controlled and to 

ThrottleState* ESUCabControl::findThrottleByObject(int32_t rObjID)
{
  for (uint32_t i=0; i<MAX_THROTTLES; i++)
  {
    if (NULL != this->throttleStates[i])
    {
      ESUCCLocRef* esuLocRef = (ESUCCLocRef*)(this->throttleStates[i]->locCmdStnRef);
      if (esuLocRef->objID == rObjID)
        return this->throttleStates[i];
    }
  }
  return NULL;
}

void ESUCabControl::handleEvents()
{
  // When we connect to the ESU system or when we connect a locomotive, we subscribe to "view" it
  //  so that any change to those objects will send EVENT notifications we can watch.  If we see an
  //  event that pertains to a locomotive we're in control of, we need to update that locomotive's throttle
  //  state such that if it conflicts when the next throttle packet comes in, we force an update.

  // Here's what we're looking for locomotives:
  // <EVENT objID>
  // objID func[x,x]  (function change)
  // objID speed[x]   (speed change)
  // objID dir[x]     (direction change)
  // <END 0 (OK)>

  // Here's what we're looking for the system level:
  // <EVENT 1>
  // 1 status[STOP]   (power turned off)
  // 1 status[GO]     (power turned on)
  // <END 0 (OK)>

  // No data?  Express lane out
  if (0 == this->rxBufferUsed)
    return;

  const char* beginPtr = strstr((char*)this->rxBuffer, "<EVENT ");

  // Did we find the start of an event?  If it's not at the beginning of the rxBuffer, we have junk
  // ahead of it and we should clean that out
  if (NULL != beginPtr)
  {
    memmove(this->rxBuffer, beginPtr, this->rxBufferUsed - (beginPtr - (const char*)this->rxBuffer));
    this->rxBufferUsed -= beginPtr - (const char*)this->rxBuffer;
    this->rxBuffer[this->rxBufferUsed] = 0;
  }

  while (NULL != (beginPtr = strstr((char*)this->rxBuffer, "<EVENT ")))
  {
    const char* endPtr = NULL;
    const char* endEndPtr = NULL;

    // Okay, we found a beginning, do we have an end?
    endPtr = strstr(beginPtr, "<END ");
    if (NULL == endPtr)
      break;

    endEndPtr = strchr(endPtr, '>');
    if (NULL == endEndPtr)
      break;

    // If we didn't find and end for this event, just break out
    //Serial.printf("Found event!  len=%d\n", endEndPtr - beginPtr);
    // At this point, we have a fully formed event, from <EVENT all the way to the closing END > tag
    uint32_t rObjID = -1;
    const char* responseStr = beginPtr;
    uint32_t errCd = atoi(endPtr + 5); // Skip over "<END " and get the numeric error code

    // If the errCd isn't 0, why did the command station send this?  Regardless, probably should handle it

    if (0 == errCd && 1 == sscanf(beginPtr, "<EVENT %u>", &rObjID))
    {
      // Okay, now we have an object ID.  See if it's one we care about
      // Go to the end of the EVENT tag and start at the next character
      if (IS_DBGLVL_DEBUG)
        Serial.printf("[ESU]: Event for objID=%d\n", rObjID);

      ThrottleState* tUpdate = this->findThrottleByObject(rObjID);
      if (NULL != tUpdate || 1 == rObjID)  // It's either a throttle or system state (like power)
      {
        responseStr = strchr(beginPtr, '>') + 1;
        while (!isprint(*responseStr) || isspace(*responseStr))
          responseStr++; // And increment past the first CR/LF
        
        uint32_t responseLen = endPtr - responseStr;

        // responseStr now should be the start of things to parse
        const char* bolPtr = responseStr;
        const char* eolPtr = bolPtr;
        while (*bolPtr != 0 && NULL != (eolPtr = strchr(bolPtr, '\n')) && eolPtr < responseStr + responseLen)
        {
          uint32_t rDir = 2;
          uint32_t rSpeed = 0;
          uint32_t rFuncNum = 0;
          uint32_t rFuncVal = 0;
          if (eolPtr > bolPtr) // More than a blank line...
          {
            if (NULL != tUpdate && 2 == sscanf(bolPtr, "%u dir[%u]", &rObjID, &rDir))
            {
              tUpdate->locRevDirection = rDir?true:false;
              if (IS_DBGLVL_INFO)
                Serial.printf("[ESU]: EVENT: loc=%d objID=%d dir=%d\n", tUpdate->locAddr, rObjID, rDir);
            }
            else if (NULL != tUpdate && 2 == sscanf(bolPtr, "%u speed[%u]", &rObjID, &rSpeed))
            {
              tUpdate->locSpeed = rSpeed;
              if (IS_DBGLVL_INFO)
                Serial.printf("[ESU]: EVENT: loc=%d objID=%d speed=%d\n", tUpdate->locAddr, rObjID, rSpeed);
            }
            else if (NULL != tUpdate && 3 == sscanf(bolPtr, "%u func[%u,%u]", &rObjID, &rFuncNum, &rFuncVal))
            {
              if (IS_DBGLVL_INFO)
                Serial.printf("[ESU]: EVENT: loc=%d objID = %d, f[%u]=%u\n", tUpdate->locAddr, rObjID, rFuncNum, rFuncVal);
              if (rFuncNum < MAX_FUNCTIONS)
                tUpdate->locFunctions[rFuncNum] = rFuncVal?true:false;
            }
            else if (1 == rObjID && 0 == strcmp("1 status[GO]", bolPtr))
            {
              if (IS_DBGLVL_INFO)
                Serial.printf("[ESU]: EVENT: track power turned on\n");
              this->trackPowerOn = true;
            }
            else if (1 == rObjID && 0 == strcmp("1 status[STOP]", bolPtr))
            {
              if (IS_DBGLVL_INFO)
                Serial.printf("[ESU]: EVENT: track power turned off\n");
              this->trackPowerOn = false;
            }
          }
          bolPtr = eolPtr+1;
        }
      }

    }

    // Now, remove the whole event from rxbuffer
    memmove((char*)beginPtr, endEndPtr+1, this->rxBufferUsed - (endEndPtr - beginPtr));
    this->rxBufferUsed -= endEndPtr - beginPtr;
    this->rxBuffer[this->rxBufferUsed] = 0;
  }

  beginPtr = strstr((char*)this->rxBuffer, "<EVENT ");
  if (NULL == beginPtr) // No start of an event hanging around?  Clean out the whole thing
  {
    memset(this->rxBuffer, 0, ESUCC_RX_BUFFER_SZ);
    this->rxBufferUsed = 0;
  } else {
    // Clean out any remaining junk in the unused part of the buffer
    memset(this->rxBuffer + this->rxBufferUsed, 0, ESUCC_RX_BUFFER_SZ-this->rxBufferUsed);
  }

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
    if (IS_DBGLVL_DEBUG)
      Serial.printf("[ESU]: TX [%s]\n", cmdStr);
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
  if (IS_DBGLVL_DEBUG && 0 != bytesRead)
    Serial.printf("[ESU]: RX [%s]\n", this->rxBuffer);

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

  this->handleEvents();

  // Send query
  char *queryWithLineEnding = NULL;
  asprintf(&queryWithLineEnding, "%s\n", queryStr);
  this->rxtx(queryWithLineEnding);
  free(queryWithLineEnding);

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
    if (IS_DBGLVL_ERR)
      Serial.printf("[ESU]: RXTX Timeout - query [%s]\n", queryStr);

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

  if (IS_DBGLVL_DEBUG)
    Serial.printf("[ESU]: queryLocomotiveObjectGet: Search for loco [%d]\n", locAddr);

  snprintf(queryStr, sizeof(queryStr)-1, "queryObjects(10, addr)");

  responseLen = this->query(queryStr, &responseStr, &errCd, 2000);

  if (responseLen > 0 && NULL != responseStr && 0 == errCd)
  {
    if (IS_DBGLVL_INFO)
      Serial.printf("[ESU]: locomotiveObjectGet: Got response, errCode=%d\n", errCd);
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
    if (-2 == objID && IS_DBGLVL_ERR)
      Serial.printf("[ESU]: queryLocomotiveObjectGet: TIMED OUT\n");
    else if (-1 == objID && IS_DBGLVL_INFO)
      Serial.printf("[ESU]: queryLocomotiveObjectGet: Loco [%d] not found\n", locAddr);
    else if (IS_DBGLVL_INFO)
      Serial.printf("[ESU]: queryLocomotiveObjectGet: Loco [%d] is objID [%d]\n", locAddr, objID);
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

  if (IS_DBGLVL_DEBUG)
    Serial.printf("[ESU]: queryAddLocomotiveObject: Add loco [%d]\n", locAddr);

  snprintf(queryStr, sizeof(queryStr)-1, "create(10, addr[%d], append)", locAddr);
  responseLen = this->query(queryStr, &responseStr, &errCd);

  if (responseLen > 0 && NULL != responseStr && 0 == errCd)
  {
    if (IS_DBGLVL_DEBUG)
      Serial.printf("[ESU]: queryAddLocomotiveObject: Got response, errCd = %d\n", errCd);
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
    if (-1 == errCd && IS_DBGLVL_ERR)
      Serial.printf("[ESU]: queryAddLocomotiveObject: TIMED OUT\n");
    else if (0 != errCd && IS_DBGLVL_ERR)
      Serial.printf("[ESU]: queryAddLocomotiveObject: QUERY ERROR  [%d]\n", errCd);
    else if (IS_DBGLVL_INFO)
      Serial.printf("[ESU]: queryAddLocomotiveObject: Loco [%d] is objID [%d]\n", locAddr, objID);
  }

  return objID;
}

bool ESUCabControl::queryLocomotiveObjectSpeedDirGet(int32_t objID, uint8_t* speed, bool* isReverse)
{
  char queryStr[256];
  char* responseStr = NULL;
  int32_t responseLen = 0;
  int32_t errCd = -1;
  bool retval = false;

  if (IS_DBGLVL_DEBUG)
    Serial.printf("[ESU]: queryLocomotiveObjectSpeedDirGet: objID[%d]\n", objID);

  snprintf(queryStr, sizeof(queryStr)-1, "get(%d, speed, dir)", objID);
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
        uint32_t rSpeed= 0;
        uint32_t rDir= 0;

        *eolPtr = 0; // Null terminate in place, don't waste a copy
        if (2 == sscanf(bolPtr, "%u speed[%u]", &rObjID, &rSpeed) && rObjID == objID)
        {
          *speed = rSpeed;
        }
        else if (2 == sscanf(bolPtr, "%u dir[%u]", &rObjID, &rDir) && rObjID == objID)
        {
          *isReverse = rDir?true:false;
        }
      }
      bolPtr = eolPtr+1;
    }
  }

  if (NULL != responseStr)
    free(responseStr);

  if (this->debug)
  {
    if (-1 == errCd && IS_DBGLVL_ERR)
      Serial.printf("[ESU]: queryLocomotiveObjectSpeedDirGet: TIMED OUT\n");
    else if (0 != errCd && IS_DBGLVL_ERR)
      Serial.printf("[ESU]: queryLocomotiveObjectSpeedDirGet: QUERY ERROR  [%d]\n", errCd);
    else if (IS_DBGLVL_INFO)
      Serial.printf("[ESU]: queryLocomotiveObjectSpeedDirGet: objID[%d] speed=%d, dir=%c\n", objID, *speed, *isReverse?'R':'F');
  }
  return retval;
}



bool ESUCabControl::queryLocomotiveObjectAllFunctionsGet(int32_t objID, bool *locFunctions)
{
  // This is the fast func getter to minimize the number of queries we need to make
  char queryStr[256];
  char* responseStr = NULL;
  int32_t responseLen = 0;
  int32_t errCd = -1;
  bool retval = false;

  if (IS_DBGLVL_DEBUG)
    Serial.printf("[ESU]: queryLocomotiveObjectAllFunctionsGet: objID[%d]\n", objID);

  for(int32_t queryPhase=0; queryPhase < 3; queryPhase++)
  {
    switch(queryPhase)
    {
      case 0:
        snprintf(queryStr, sizeof(queryStr)-1, "get(%d, func[0], func[1], func[2], func[3], func[4], func[5], func[6], func[7], func[8], func[9])", objID);
        break;
      case 1:
        snprintf(queryStr, sizeof(queryStr)-1, "get(%d, func[10], func[11], func[12], func[13], func[14], func[15], func[16], func[17], func[18], func[19])", objID);
        break;

      case 2:
        snprintf(queryStr, sizeof(queryStr)-1, "get(%d, func[20], func[21], func[22], func[23], func[24], func[25], func[26], func[27], func[28])", objID);
        break;

      default:
        // Eh, what?
        break;

    }
    responseLen = this->query(queryStr, &responseStr, &errCd);

    if (responseLen > 0 && NULL != responseStr && 0 == errCd)
    {
      if (IS_DBGLVL_DEBUG)
        Serial.printf("[ESU]: queryLocomotiveObjectAllFunctionsGet: Got response, errCd = %d\n", errCd);
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
            if (rObjID == objID && rFuncNum < MAX_FUNCTIONS)
            {
              locFunctions[rFuncNum] = rFuncVal?true:false;
            }
          }
        }
        bolPtr = eolPtr+1;
      }
    }
    // Maybe retry?
    if (NULL != responseStr)
      free(responseStr);

    if (this->debug)
    {
      if (-1 == errCd && IS_DBGLVL_ERR)
        Serial.printf("[ESU]: queryLocomotiveObjectAllFunctionsGet: TIMED OUT\n");
      else if (0 != errCd && IS_DBGLVL_ERR)
        Serial.printf("[ESU]: queryLocomotiveObjectAllFunctionsGet: QUERY ERROR  [%d]\n", errCd);
      else if (IS_DBGLVL_INFO)
        Serial.printf("[ESU]: queryLocomotiveObjectAllFunctionsGet: SUCCESS\n");
    }

  }
  return retval;
}

bool ESUCabControl::queryLocomotiveObjectFunctionGet(int32_t objID, uint8_t funcNum)
{
  char queryStr[256];
  char* responseStr = NULL;
  int32_t responseLen = 0;
  int32_t errCd = -1;
  bool retval = false;

  if (IS_DBGLVL_DEBUG)
    Serial.printf("[ESU]: queryLocomotiveObjectFunctionGet: objID[%d] f[%d]\n", objID, funcNum);

  snprintf(queryStr, sizeof(queryStr)-1, "get(%d, func[%d])", objID, funcNum);
  responseLen = this->query(queryStr, &responseStr, &errCd);

  if (responseLen > 0 && NULL != responseStr && 0 == errCd)
  {
    if (IS_DBGLVL_DEBUG)
      Serial.printf("[ESU]: queryLocomotiveObjectFunctionGet: Got response, errCd = %d\n", errCd);
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
    if (-1 == errCd && IS_DBGLVL_ERR)
      Serial.printf("[ESU]: queryLocomotiveObjectFunctionGet: TIMED OUT\n");
    else if (0 != errCd && IS_DBGLVL_ERR)
      Serial.printf("[ESU]: queryLocomotiveObjectFunctionGet: QUERY ERROR  [%d]\n", errCd);
    else if (IS_DBGLVL_INFO)
      Serial.printf("[ESU]: queryLocomotiveObjectFunctionGet: objID[%d] f[%d]=%d\n", objID, funcNum, retval?1:0);
  }
  return retval;
}

int32_t ESUCabControl::queryTrackPowerState()
{
  char* responseStr = NULL;
  int32_t responseLen = 0;
  int32_t errCd = -1;
  int32_t retval = -1;

  if (IS_DBGLVL_DEBUG)
    Serial.printf("[ESU]: queryTrackPowerState\n");

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
    if (-1 == errCd && IS_DBGLVL_ERR)
      Serial.printf("[ESU]: queryTrackPowerState: TIMED OUT\n");
    else if (0 != errCd && IS_DBGLVL_ERR)
      Serial.printf("[ESU]: queryTrackPowerState: QUERY ERROR  [%d]\n", errCd);
    else if (IS_DBGLVL_INFO)
      Serial.printf("[ESU]: queryTrackPowerState: %s\n", retval?"ON":"OFF");
  }
  return retval;
}



bool ESUCabControl::queryAcquireLocomotiveObject(int32_t objID)
{
  char queryStr[256];
  int32_t errCd = -1;

  if (IS_DBGLVL_DEBUG)
    Serial.printf("[ESU]: queryAcquireLocomotiveObject: objID[%d]\n", objID);

  snprintf(queryStr, sizeof(queryStr)-1, "request(%d, view, control, force)", objID);
  this->query(queryStr, NULL, &errCd);

  if (this->debug)
  {
    if (-1 == errCd && IS_DBGLVL_ERR)
      Serial.printf("[ESU]: queryAcquireLocomotiveObject: TIMED OUT\n");
    else if (0 != errCd && IS_DBGLVL_ERR)
      Serial.printf("[ESU]: queryAcquireLocomotiveObject: QUERY ERROR  [%d]\n", errCd);
    else if (IS_DBGLVL_INFO)
      Serial.printf("[ESU]: queryAcquireLocomotiveObject: objID[%d] acquired control\n", objID);
  }

  if (0 == errCd)
    return true;

  return false;
}


bool ESUCabControl::queryLocomotiveObjectSpeedSet(int32_t objID, uint8_t speed, bool isReverse)
{
  char queryStr[256];
  int32_t errCd = -1;

  if (IS_DBGLVL_DEBUG)
    Serial.printf("[ESU]: queryLocomotiveObjectSpeedSet: objID[%d] speed[%c:%d]\n", objID, isReverse?'R':'F', speed);

  speed = MIN(126, speed);

  snprintf(queryStr, sizeof(queryStr)-1, "set(%d, speed[%d], dir[%d])", objID, speed, isReverse?1:0);
  this->query(queryStr, NULL, &errCd);

  if (this->debug)
  {
    if (-1 == errCd && IS_DBGLVL_ERR)
      Serial.printf("[ESU]: queryLocomotiveObjectSpeedSet: TIMED OUT\n");
    else if (0 != errCd && IS_DBGLVL_ERR)
      Serial.printf("[ESU]: queryLocomotiveObjectSpeedSet: QUERY ERROR  [%d]\n", errCd);
    else if (IS_DBGLVL_INFO)
      Serial.printf("[ESU]: queryLocomotiveObjectSpeedSet: objID[%d] speed set\n", objID);
  }
  if (0 == errCd)
    return true;

  return false;
}

bool ESUCabControl::queryLocomotiveObjectFunctionSet(int32_t objID, uint8_t funcNum, bool funcVal)
{
  char queryStr[256];
  int32_t errCd = -1;

  if (IS_DBGLVL_DEBUG)
    Serial.printf("[ESU]: queryLocomotiveObjectFunctionSet: objID[%d] f[%d]=%d\n", objID, funcNum, funcVal?1:0);

  funcNum = MIN(MAX_FUNCTIONS, funcNum);

  snprintf(queryStr, sizeof(queryStr)-1, "set(%d, func[%d,%d])", objID, funcNum, funcVal?1:0);
  this->query(queryStr, NULL, &errCd);

  if (this->debug)
  {
    if (-1 == errCd && IS_DBGLVL_ERR)
      Serial.printf("[ESU]: queryLocomotiveObjectFunctionSet: TIMED OUT\n");
    else if (0 != errCd && IS_DBGLVL_ERR)
      Serial.printf("[ESU]: queryLocomotiveObjectFunctionSet: QUERY ERROR  [%d]\n", errCd);
    else if (IS_DBGLVL_INFO)
      Serial.printf("[ESU]: queryLocomotiveObjectFunctionSet: objID[%d] f[%d] set success\n", objID, funcNum);
  }

  if (0 == errCd)
    return true;

  return false;
}


bool ESUCabControl::queryLocomotiveObjectEmergencyStop(int32_t objID)
{
  char queryStr[256];
  int32_t errCd = -1;

  if (IS_DBGLVL_DEBUG)
    Serial.printf("[ESU]: queryLocomotiveObjectEmergencyStop: objID[%d] ESTOP\n", objID);

  snprintf(queryStr, sizeof(queryStr)-1, "set(%d, stop)", objID);
  this->query(queryStr, NULL, &errCd);

  if (this->debug)
  {
    if (-1 == errCd && IS_DBGLVL_ERR)
      Serial.printf("[ESU]: queryLocomotiveObjectEmergencyStop: TIMED OUT\n");
    else if (0 != errCd && IS_DBGLVL_ERR)
      Serial.printf("[ESU]: queryLocomotiveObjectEmergencyStop: QUERY ERROR  [%d]\n", errCd);
    else if (IS_DBGLVL_INFO)
      Serial.printf("[ESU]: queryLocomotiveObjectEmergencyStop: objID[%d] ESTOP success\n", objID);
  }

  if (0 == errCd)
    return true;

  return false;
}


bool ESUCabControl::queryReleaseLocomotiveObject(int32_t objID)
{
  char queryStr[256];
  int32_t errCd = -1;

  if (IS_DBGLVL_DEBUG)
    Serial.printf("[ESU]: queryReleaseLocomotiveObject: objID[%d] release\n", objID);

  snprintf(queryStr, sizeof(queryStr)-1, "release(%d, view, control)", objID);
  this->query(queryStr, NULL, &errCd);

  if (this->debug)
  {
    if (-1 == errCd && IS_DBGLVL_ERR)
      Serial.printf("[ESU]: queryReleaseLocomotiveObject: TIMED OUT\n");
    else if (0 != errCd && IS_DBGLVL_ERR)
      Serial.printf("[ESU]: queryReleaseLocomotiveObject: QUERY ERROR  [%d]\n", errCd);
    else if (IS_DBGLVL_INFO)
      Serial.printf("[ESU]: queryReleaseLocomotiveObject: objID[%d] release success\n", objID);
  }

  if (0 == errCd)
    return true;

  return false;
}

bool ESUCabControl::locomotiveObjectGet(ThrottleState* tState, uint16_t addr, bool isLongAddr, uint8_t mrbusAddr)
{
  if (IS_DBGLVL_INFO)
    Serial.printf("[ESU]: locomotiveObjectGet(%c%04d)\n", isLongAddr?'L':'S', addr);

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
    if (IS_DBGLVL_ERR)
      Serial.printf("[ESU]: locomotiveObjectGet(%c%04d) object query timeout\n", isLongAddr?'L':'S', addr);
    return false;  // We didn't acquire it because our transaction timed out.  Don't blindly add unless we have a good transaction
  }
  // Didn't find it?
  else if (-1 == objID)
  {
    if (IS_DBGLVL_WARN)
      Serial.printf("[ESU]: locomotiveObjectGet(%c%04d) loc not found, adding it\n", isLongAddr?'L':'S', addr);
    objID = this->queryAddLocomotiveObject(addr);

  }
  if (-1 == objID)
    return false;  // Nothing else we can do here except fail!*/

  if (IS_DBGLVL_INFO)
    Serial.printf("[ESU]: locomotiveObjectGet(%c:%d) at objID[%d]\n", isLongAddr?'L':'S', addr, objID);

  // Put the throttle state reference in the throttle states array so events can update it
  uint8_t offset = mrbusAddr - MRBUS_THROTTLE_BASE_ADDR;
  if (offset < MAX_THROTTLES)  // Eh, now what?
  {
    this->throttleStates[offset] = tState;
  }

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
  this->queryLocomotiveObjectSpeedDirGet(objID, &tState->locSpeed, &tState->locRevDirection);

  // Get current function states
  this->queryLocomotiveObjectAllFunctionsGet(objID, tState->locFunctions);

  tState->locFunctionsGood = true;

  if (IS_DBGLVL_INFO)
    Serial.printf("[ESU]: locomotiveObjectGet(%c%04d) success\n", isLongAddr?'L':'S', addr);

  return true;
}

bool ESUCabControl::locomotiveEmergencyStop(ThrottleState* tState)
{
  ESUCCLocRef* esuLocRef = (ESUCCLocRef*)tState->locCmdStnRef;
  if (NULL == esuLocRef)
    return false;

  if (IS_DBGLVL_INFO)
    Serial.printf("[ESU]: locomotiveEmergencyStop(%c%04d/%d)\n", esuLocRef->isLongAddr?'L':'S', esuLocRef->locAddr, esuLocRef->objID);


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

  if (IS_DBGLVL_INFO)
    Serial.printf("[ESU]: locomotiveSpeedSet(%c%04d/%d) speed[%c:%d]\n", esuLocRef->isLongAddr?'L':'S', esuLocRef->locAddr, esuLocRef->objID, isReverse?'R':'F', speed);

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

  if (IS_DBGLVL_INFO)
    Serial.printf("[ESU]: locomotiveFunctionsGet(%c%04d/%d)\n", esuLocRef->isLongAddr?'L':'S', esuLocRef->locAddr, esuLocRef->objID);


  this->queryLocomotiveObjectAllFunctionsGet(esuLocRef->objID, functionStates);
  return true;
}

bool ESUCabControl::locomotiveFunctionSet(ThrottleState* tState, uint8_t funcNum, bool funcActive)
{
  ESUCCLocRef* esuLocRef = (ESUCCLocRef*)tState->locCmdStnRef;
  if (NULL == esuLocRef)
    return false;

  if (IS_DBGLVL_INFO)
    Serial.printf("[ESU]: locomotiveFunctionSet(%c%04d/%d) F%02d = %d\n", esuLocRef->isLongAddr?'L':'S', esuLocRef->locAddr, esuLocRef->objID, funcNum, funcActive?1:0);

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

    if (IS_DBGLVL_INFO)
      Serial.printf("[ESU]: locomotiveDisconnect(%c%04d/%d)\n", esuLocRef->isLongAddr?'L':'S', esuLocRef->locAddr, esuLocRef->objID);

    uint8_t offset = esuLocRef->mrbusAddr - MRBUS_THROTTLE_BASE_ADDR;
    if (offset < MAX_THROTTLES)  // Eh, now what?
      this->throttleStates[offset] = NULL;
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


