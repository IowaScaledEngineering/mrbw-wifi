#include "Arduino.h"
#include "MRBusThrottle.h"


MRBusThrottle::MRBusThrottle()
{
  this->tState.init();
  this->throttleAddr = 0x00;
}

MRBusThrottle::~MRBusThrottle()
{
}

void MRBusThrottle::initialize(uint8_t mrbusAddr)
{
  this->tState.init();
  this->throttleAddr = mrbusAddr;
}

uint8_t MRBusThrottle::mrbusAddrGet()
{
  return this->throttleAddr;
}

bool MRBusThrottle::isActive()
{
  return this->tState.active;
}

bool MRBusThrottle::isExpired(uint32_t idleSeconds)
{
  // Make this easy - lastupdate is stored in milliseconds 
  if (esp_timer_get_time() - this->tState.lastUpdate > idleSeconds * 1000000)
    return true;
  return false;
}

// Release just releases the current engine number
bool MRBusThrottle::release(CommandStation* cmdStn)
{
  Serial.printf("Locomotive %c:%d released\n", this->tState.isLongAddr?'L':'S', this->tState.locAddr);
  return true;
}


// Disconnect says that the throttle went away, go ahead and shut things down and
//  mark the throttle as inactive
bool MRBusThrottle::disconnect(CommandStation* cmdStn)
{
  cmdStn->locomotiveDisconnect(&this->tState);
  this->release(cmdStn);
  this->initialize(this->throttleAddr);
  return true;
}

void MRBusThrottle::update(CommandStation* cmdStn, MRBusPacket &pkt)
{
  bool updateFunctions[MAX_FUNCTIONS];

// MRBus Throttle Packet Definition
// Data 0: 'S' (Status)
// Data 1: High 8 bits of locomotive address
// Data 2: Low 8 bits of locomotive address
// Data 3:
//    Bit 7:    Forward (0x80) / Reverse (0x00)
//    Bit 0-6:  Speed (0=off, 1=Estop, 2-127 speed)
// Data 4:
//    Bit 7:   Reserved
//    Bit 6:   Reserved
//    Bit 5:   Reserved
//    Bit 4:   Reserved
//    Bit 0-3: F24 (bit 0) - F28 (bit 3)
// Data 5: F16 (bit 0) - F23 (bit 7)
// Data 6: F08 (bit 0) - F15 (bit 7)
// Data 7: F00 (bit 0) - F07 (bit 7)
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

  if (addr != this->tState.locAddr || longAddr != this->tState.isLongAddr || NULL == this->tState.locCmdStnRef)
  {
    // FIXME: If we already have a locomotive, we may need to release it

    // Need to acquire locomotive from command station
    bool successfullyAcquired = cmdStn->locomotiveObjectGet(&this->tState, addr, longAddr, this->throttleAddr);
    if (!successfullyAcquired)
    {
      //FIXME:  We didn't successfully acquire, clean house
      this->tState.init();
      return;
    }
    this->tState.active = true;
    this->tState.locFunctionsGood = false;
    this->tState.isLongAddr = longAddr;
    this->tState.locAddr = addr;
    Serial.printf("Locomotive %c:%d acquired\n", this->tState.isLongAddr?'L':'S', this->tState.locAddr);
  }

  // Only send estop if we just moved into that state
  if (estop && !this->tState.locEStop)
  {
    cmdStn->locomotiveEmergencyStop(&this->tState);
    this->tState.locEStop = estop;
    this->tState.locSpeed = 0;
  }

  if (!this->tState.locEStop && (speed != this->tState.locSpeed || reverse != this->tState.locRevDirection))
  {
    cmdStn->locomotiveSpeedSet(&this->tState, speed, reverse);
    this->tState.locSpeed = speed;
    this->tState.locRevDirection = reverse;
  }

  if (!this->tState.locFunctionsGood)
  {
    // FIXME: Fetch current function state from command station
    for(uint32_t i=0; i<MAX_FUNCTIONS; i++)
      this->tState.locFunctions[i] = false;

    this->tState.locFunctionsGood = true;
  }

  for(uint32_t i=0; i<MAX_FUNCTIONS; i++)
    updateFunctions[i] = pkt.data[7 - (i/8)] & (1<<(i%8))?true:false;

  for(uint32_t i=0; i<MAX_FUNCTIONS; i++)
  {
    if(updateFunctions[i] != this->tState.locFunctions[i])
    {
      // FIXME:  Send command station update
      cmdStn->locomotiveFunctionSet(&this->tState, i, updateFunctions[i]);
      this->tState.locFunctions[i] = updateFunctions[i];
    }
  }
  //Serial.printf("Locomotive %d updated\n", this->locAddr);
  this->tState.lastUpdate = esp_timer_get_time();
}