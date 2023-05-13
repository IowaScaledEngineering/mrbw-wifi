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

void ESUCabControl::rxtx(const char* cmdStr)
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

  uint32_t bytesRead = this->cmdStnConnection->readBytes(this->rxBuffer + this->rxBufferUsed, MIN(available, ESUCC_RX_BUFFER_SZ - this->rxBufferUsed - 1));
  this->rxBufferUsed += bytesRead;
  if (0 == bytesRead)
    return;

  Serial.printf("ESU RX: [%s]\n", this->rxBuffer);

  return;
}

bool ESUCabControl::locomotiveObjectGet(ThrottleState* tState, uint16_t addr, bool isLongAddr, uint8_t mrbusAddr)
{
  ESUCCLocRef* esuLocRef = new ESUCCLocRef(addr, isLongAddr, mrbusAddr, 0);
  tState->locCmdStnRef = (void*)esuLocRef;
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
  delete (ESUCCLocRef*)tState->locCmdStnRef;
  return true;
}


