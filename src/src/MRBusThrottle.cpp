#include "MRBusThrottle.h"
#include "Arduino.h"
MRBusThrottle::MRBusThrottle()
{
  this->active = false;
  this->throttleAddr = 0x00;
  this->locAddr = 0;
  this->isLongAddr = false;
  this->locSpeed = 0;
  this->locRevDirection = false;
  this->locEStop = false;
  for(uint32_t i=0; i<MAX_FUNCTIONS; i++)
    this->locFunctions[i] = false;

  this->locFunctionsGood = false;
  this->lastUpdate = 0;
  this->isCmdStnReferenceValid = false;
}

MRBusThrottle::~MRBusThrottle()
{
}

void MRBusThrottle::initialize(uint8_t mrbusAddr)
{
  this->active = false;
  this->throttleAddr = mrbusAddr;
  this->locAddr = 0;
  this->isLongAddr = false;
  this->locSpeed = 0;
  this->locRevDirection = false;
  this->locEStop = false;
  for(uint32_t i=0; i<MAX_FUNCTIONS; i++)
    this->locFunctions[i] = false;

  this->locFunctionsGood = false;
  this->lastUpdate = 0;
  this->isCmdStnReferenceValid = false;
}

uint8_t MRBusThrottle::mrbusAddrGet()
{
  return this->throttleAddr;
}

bool MRBusThrottle::isActive()
{
  return this->active;
}

bool MRBusThrottle::isExpired(uint32_t idleSeconds)
{
  // Make this easy - lastupdate is stored in milliseconds 
  if (esp_timer_get_time() - this->lastUpdate > idleSeconds * 1000000)
    return true;
  return false;
}

// Release just releases the current engine number
bool MRBusThrottle::release(CommandStation* cmdStn)
{
  Serial.printf("Locomotive %c:%d released\n", this->isLongAddr?'L':'S', this->locAddr);
  return true;
}


// Disconnect says that the throttle went away, go ahead and shut things down and
//  mark the throttle as inactive
bool MRBusThrottle::disconnect(CommandStation* cmdStn)
{
  cmdStn->locomotiveSpeedSet(this->locCmdStnReference, 0, false);
  this->release(cmdStn);
  this->initialize(this->throttleAddr);
  return true;
}

void MRBusThrottle::update(CommandStation* cmdStn, MRBusPacket &pkt)
{
  bool updateFunctions[MAX_FUNCTIONS];

  if (pkt.len < 10 || 'S' != pkt.data[0])
    return; // Not a status update, ignore

  uint16_t addr = pkt.data[1] * 256 + pkt.data[2];
  bool longAddr = (addr & 0x8000)?false:true;
  if (!longAddr)
    addr = addr & 0x007F;

  uint8_t speed = pkt.data[3] & 0x7F;
  bool estop = false;
  if (1 == speed)
  {
    speed = 0;
    estop = true;
  } else if (speed > 1) {
    speed = speed - 1;
  }

  bool reverse = (pkt.data[3] & 0x80)?false:true;

  if (addr != this->locAddr || longAddr != this->isLongAddr || false == this->isCmdStnReferenceValid)
  {
    // FIXME: If we already have a locomotive, we may need to release it

    // Need to acquire locomotive from command station
    bool successfullyAcquired = cmdStn->locomotiveObjectGet(&this->locCmdStnReference, addr, longAddr, this->throttleAddr);
    if (!successfullyAcquired)
    {
      //FIXME:  We didn't successfully acquire, clean house
      this->isCmdStnReferenceValid = false;
      this->locFunctionsGood = false;
      this->locAddr = 0;
      this->isLongAddr = false;
      this->locSpeed = 0;
      this->locRevDirection = false;
      this->locEStop = false;
      return;
    }
    this->active = true;
    this->isCmdStnReferenceValid = true;
    this->locFunctionsGood = false;
    this->isLongAddr = longAddr;
    this->locAddr = addr;
    Serial.printf("Locomotive %c:%d acquired\n", this->isLongAddr?'L':'S', this->locAddr);
  }

  // Only send estop if we just moved into that state
  if (estop && !this->locEStop)
  {
    cmdStn->locomotiveEmergencyStop(this->locCmdStnReference);
    this->locEStop = estop;
  }

  if (this->locEStop && (speed != this->locSpeed || reverse != this->locRevDirection))
  {
    cmdStn->locomotiveSpeedSet(this->locCmdStnReference, speed, reverse);
    this->locSpeed = speed;
    this->locRevDirection = reverse;
  }


  if (!this->locFunctionsGood)
  {
    // FIXME: Fetch current function state from command station
    for(uint32_t i=0; i<MAX_FUNCTIONS; i++)
      this->locFunctions[i] = false;

    this->locFunctionsGood = true;
  }

  for(uint32_t i=0; i<MAX_FUNCTIONS; i++)
    updateFunctions[i] = pkt.data[7 - (i/8)] & (1<<(i%8))?true:false;

  for(uint32_t i=0; i<MAX_FUNCTIONS; i++)
  {
    if(updateFunctions[i] != this->locFunctions[i])
    {
      // FIXME:  Send command station update
      cmdStn->locomotiveFunctionSet(this->locCmdStnReference, i, updateFunctions[i]);
      this->locFunctions[i] = updateFunctions[i];
    }
  }
  //Serial.printf("Locomotive %d updated\n", this->locAddr);
  this->lastUpdate = esp_timer_get_time();
}