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


ESUCCLocRef::ESUCCLocRef(uint16_t locAddr, bool isLongAddr, uint8_t mrbusAddr, uint32_t objID)
{
  this->locAddr = locAddr;
  this->isLongAddr = isLongAddr;
  this->mrbusAddr = mrbusAddr;
  this->objID = objID;
}

/*
void someFunc()
{
  const char* ptr = strstr(this->rxBuffer, "<END", this->rxBufferUsed);

}*/


ESUCabControl::ESUCabControl()
{
  this->cmdStnConnection = NULL;
  this->rxBuffer = new uint8_t[ESUCC_RX_BUFFER_SZ];
  memset(this->rxBuffer, 0, ESUCC_RX_BUFFER_SZ);
  this->rxBufferUsed = 0;
  this->keepaliveTimer.setup(10000);
  for(uint32_t i; i<MAX_THROTTLES; i++)
    this->throttleStates[i] = NULL;
}

bool ESUCabControl::begin(WiFiClient &cmdStnConnection, uint32_t quirkFlags)
{
  this->cmdStnConnection = &cmdStnConnection;
  return true;
}

bool ESUCabControl::end()
{
  this->cmdStnConnection = NULL;
  for(uint32_t i; i<MAX_THROTTLES; i++)
  {
    if(NULL != this->throttleStates[i])
      free(this->throttleStates[i]);
    this->throttleStates[i] = NULL;
  }
  return true;
}

bool ESUCabControl::update()
{
  return true;
}


void ESUCabControl::rxtx()
{
  this->rxtx(NULL);
}

int32_t ESUCabControl::query(const char* queryStr, char** replyBuffer, int32_t* errCode)
{
  PeriodicEvent queryTimeout;
  
  char* replyBeginStr = NULL;
  const char* beginPtr = NULL;
  const char* endPtr = NULL;
  const char* endEndPtr = NULL;
  int32_t queryLen = asprintf(&replyBeginStr, "<REPLY %s>", queryStr);
  int32_t responseLen = 0;

  if (NULL != errCode)
    *errCode = -1; // Since ESU uses positive integers, we'll make -1 mean "no response"

  // Flush rxbuffer
  memset(this->rxBuffer, 0, ESUCC_RX_BUFFER_SZ);
  this->rxBufferUsed = 0;

  // Send query
  char *queryWithLineEnding = NULL;
  asprintf(&queryWithLineEnding, "%s\n", queryStr);
  this->rxtx(queryWithLineEnding);
  free(queryWithLineEnding);

  // Now, our mission is to wait until either we get a response or timeout
  // A response will be in the form <REPLY queryStr>....<END num (errorstr)>
  // However, we may also see other weird stuff, like <EVENT ...> ... <END num (errorstr)>
  queryTimeout.setup(2000);
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

//  Serial.printf("Timeout?  %c\n", queryTimeout.test(false)?'Y':'N');

  Serial.printf("replyBeginStr = [%s] beginPtr=%p, endPtr=%p endEndPtr=%p\n", replyBeginStr, beginPtr, endPtr, endEndPtr);

  if (NULL != endEndPtr)
  {
    // Increment beginning pointer over the first query
    beginPtr += queryLen;
    while (!isprint(*beginPtr))
      beginPtr++; // And increment past the first CR/LF
    responseLen = endPtr - beginPtr;
    if (responseLen > 0)
      asprintf(replyBuffer, "%*.*s", responseLen, responseLen, beginPtr);
//    Serial.printf("Got response [%*.*s]\n", endPtr - (beginPtr+queryLen), endPtr - (beginPtr+queryLen), beginPtr+queryLen);

    if (NULL != errCode)
      *errCode = atoi(endPtr + 5); // Skip over "<END " and get the error code
  }
  else
    Serial.printf("Timeout\n");
      // We timed out...
  if (NULL != replyBeginStr)
    free(replyBeginStr);
  return responseLen;
}

void ESUCabControl::rxtx(const char* cmdStr)
{
  if (NULL != cmdStr)
  {
    this->cmdStnConnection->write(cmdStr);
    this->keepaliveTimer.reset();
    Serial.printf("ESU TX: [%s]\r\n", cmdStr);
  }

  int32_t available = 0;
  available = this->cmdStnConnection->available();
  
  if (available <= 0)
    return;

  uint32_t bytesRead = this->cmdStnConnection->readBytes(this->rxBuffer + this->rxBufferUsed, MIN(available, ESUCC_RX_BUFFER_SZ - this->rxBufferUsed - 1));
  this->rxBufferUsed += bytesRead;
  if (0 == bytesRead)
    return;

  Serial.printf("ESU RX: [%s]\n", this->rxBuffer);

  return;
}

int32_t ESUCabControl::queryLocomotiveObjectGet(uint16_t locAddr)
{
  char queryStr[256];
  char* responseStr;
  int32_t responseLen = 0;
  int32_t objID = -1;
  int32_t errCd = -1;
  snprintf(queryStr, sizeof(queryStr)-1, "queryObjects(10, addr)");

  responseLen = this->query(queryStr, &responseStr, &errCd);

  if (responseLen > 0 && NULL != responseStr)
  {
    Serial.printf("ESU locomotiveObjectGet: Got response, errCode=%d\n", errCd);
    char* bolPtr = responseStr;
    char* eolPtr;
    uint32_t lineCnt = 0;
    while (*bolPtr != 0 && NULL != (eolPtr = strchr(bolPtr, '\n')) && eolPtr <= responseStr + responseLen)
    {
      char lineBuffer[256];
      uint32_t lineLen = MAX(255, eolPtr - bolPtr);
      lineCnt++;
      memcpy(lineBuffer, bolPtr, lineLen);
      lineBuffer[lineLen] = 0;
      Serial.printf("%04d: [%s]\n", lineCnt, lineBuffer);

      // Parse this line, see if it's the locomotive we're looking for
      // Valid responses will be in the form "n..n addr[n]"
      char* addrPtr = strstr(lineBuffer, "addr[");
      if (NULL != addrPtr)
      {
        
        int32_t locoNum = atoi(addrPtr + 5);
        //Serial.printf("ObjID=%d Loco=%d\n", objID, locoNum);
        if (locoNum == locAddr)
        {
          objID = atoi(lineBuffer);
          Serial.printf("Found Loco %d at object ID %d\n", locAddr, objID);
          break;
        }
      }
      bolPtr = eolPtr+1;
    }
  }

  if (NULL != responseStr)
    free(responseStr);

  return objID;
}

int32_t ESUCabControl::queryAddLocomotiveObject(uint16_t locAddr)
{
  char queryStr[256];
  char* responseStr;
  int32_t responseLen = 0;
  int32_t objID = -1;
  int32_t errCd = -1;

  snprintf(queryStr, sizeof(queryStr)-1, "create(10, addr[%d], append)", locAddr);
  responseLen = this->query(queryStr, &responseStr, &errCd);

  if (responseLen > 0 && NULL != responseStr)
  {
    Serial.printf("ESU queryAddLocomotiveObject: Got response, errCd = %d\n", errCd);
    char* bolPtr = responseStr;
    char* eolPtr;
    uint32_t lineCnt = 0;
    while (*bolPtr != 0 && NULL != (eolPtr = strchr(bolPtr, '\n')) && eolPtr <= responseStr + responseLen)
    {
      char lineBuffer[256];
      uint32_t lineLen = MAX(255, eolPtr - bolPtr);
      lineCnt++;
      memcpy(lineBuffer, bolPtr, lineLen);
      lineBuffer[lineLen] = 0;
      Serial.printf("%04d: [%s]\n", lineCnt, lineBuffer);

      // Parse this line, see if it's the locomotive we're looking for
      // Valid responses will be in the form "n..n addr[n]"
      char* idPtr = strstr(lineBuffer, "id[");
      if (NULL != idPtr)
      {
        objID = atoi(idPtr + 3);
        break;
      }
      bolPtr = eolPtr+1;
    }
  }

  if (NULL != responseStr)
    free(responseStr);

  return objID;
}

bool ESUCabControl::queryAcquireLocomotiveObject(int32_t objID)
{
  char queryStr[256];
  char* responseStr;
  int32_t responseLen = 0;
  int32_t errCd = -1;

  snprintf(queryStr, sizeof(queryStr)-1, "request(%d, control, force)", objID);
  responseLen = this->query(queryStr, &responseStr, &errCd);


  Serial.printf("ESU queryAcquireLocomotiveObject: Got response, errCd = %d\n", errCd);

  if (NULL != responseStr)
    free(responseStr);

  if (0 == errCd)
    return true;

  return false;
}

bool ESUCabControl::queryReleaseLocomotiveObject(int32_t objID)
{
  char queryStr[256];
  char* responseStr;
  int32_t responseLen = 0;
  int32_t errCd = -1;

  snprintf(queryStr, sizeof(queryStr)-1, "request(%d, control, force)", objID);
  responseLen = this->query(queryStr, &responseStr, &errCd);


  Serial.printf("ESU queryReleaseLocomotiveObject: Got response, errCd = %d\n", errCd);

  if (NULL != responseStr)
    free(responseStr);

  if (0 == errCd)
    return true;

  return false;
}

bool ESUCabControl::locomotiveObjectGet(ThrottleState* tState, uint16_t addr, bool isLongAddr, uint8_t mrbusAddr)
{
  int32_t objID = this->queryLocomotiveObjectGet(addr);

  // Didn't find it?
  if (-1 == objID)
    objID = this->queryAddLocomotiveObject(addr);

  if (-1 == objID)
    return false;  // Nothing else we can do here except fail!


  ESUCCLocRef* esuLocRef = new ESUCCLocRef(addr, isLongAddr, mrbusAddr, objID);
  tState->locCmdStnRef = (void*)esuLocRef;

  // Invalidate the function states
  tState->locFunctionsGood = false;
  for(uint32_t i=0; i<MAX_FUNCTIONS; i++)
    tState->locFunctions[i] = false;
  tState->isLongAddr = isLongAddr;
  tState->locAddr = addr;

  // Now, forcably take over the locomotive object
  this->queryAcquireLocomotiveObject(objID);

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

  if (NULL != tState->locCmdStnRef)
  {
    ESUCCLocRef* esuLocRef = (ESUCCLocRef*)tState->locCmdStnRef;
    this->queryReleaseLocomotiveObject(esuLocRef->objID);

    delete (ESUCCLocRef*)tState->locCmdStnRef;
  }
  return true;
}


