#include "periodicEvent.h"

PeriodicEvent::PeriodicEvent()
{
  this->interval_us = 0;
  this->nextEvent = 0;
  this->eventTriggered = false;
}

PeriodicEvent::~PeriodicEvent()
{
}

void PeriodicEvent::setup(uint16_t interval_ms)
{
  
  this->interval_us = interval_ms * 1000;
  this->nextEvent = esp_timer_get_time() + this->interval_us;
  this->eventTriggered = false;
}

void PeriodicEvent::reset()
{
  this->nextEvent = esp_timer_get_time() + this->interval_us;
  this->eventTriggered = false;
}

void PeriodicEvent::debug()
{
  Serial.printf("nextEvent=%lld now=%lld interval_us=%lu\n", this->nextEvent, esp_timer_get_time(), this->interval_us);
}

bool PeriodicEvent::test(bool autoReset)
{
  bool triggered = false;
  // If we're not setup, just return false always
  if (0 == this->interval_us)
    return false;

  // This isn't rollover safe, but given an int64_t will be larger than the life of the hardware...
  if (this->eventTriggered || esp_timer_get_time() >= this->nextEvent)
  {
    triggered = true;
    if (autoReset)
    {
      this->nextEvent = esp_timer_get_time() + this->interval_us;
      this->eventTriggered = false;
    } else {
      this->eventTriggered = triggered;
    }
  }

  return triggered;
}


