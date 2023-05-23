#include "Clock.h"

Clock::Clock()
{
  this->epochMilliseconds = 0;  // Epoch is in milliseconds
  this->stopped = true;
  this->enabled = false;
}

Clock::~Clock()
{
}

bool Clock::isEnabled()
{
  return this->enabled;
}

void Clock::enable()
{
  this->enabled = true;
}

void Clock::disable()
{
  this->enabled = false;
}

bool Clock::setTime(uint8_t hour, uint8_t minute, uint8_t second, uint16_t ratio)
{
  hour = MIN(23, hour);
  minute = MIN(59, minute);
  second = MIN(59, second);

  this->epochMilliseconds = 1000 * (((((uint32_t)hour * (60)) + minute) * 60) + second);

  if (0 != ratio)
    this->ratio = ratio;

  this->updated = esp_timer_get_time();  // Elapsed time in microseconds from system startup
  return true;
}

bool Clock::setTime(uint32_t epochSeconds, uint16_t ratio)
{
  epochSeconds %= 86400;
  this->epochMilliseconds = 1000 * epochSeconds;
  
  if (0 != ratio)
    this->ratio = ratio;
  this->updated = esp_timer_get_time();  // Elapsed time in microseconds from system startup
  return true;
}


void Clock::updateEpoch()
{
  uint64_t updateRequested = esp_timer_get_time();  // Elapsed time in microseconds from system startup
  if (this->stopped)
    return;

  uint64_t elapsedMicroseconds = updateRequested - this->updated;

  //Serial.printf("elapsedMicroseconds = %llu addedMS = %llu\n", elapsedMicroseconds, (elapsedMicroseconds * this->ratio) / 10000ULL);

  this->epochMilliseconds += (elapsedMicroseconds * this->ratio) / 1000000ULL;
  this->epochMilliseconds %= 86400000UL;
  this->updated = updateRequested;
}

bool Clock::stop()
{
  if (!this->stopped)
  {
    this->updateEpoch();
    this->stopped = true;
  }
  return true;
}

bool Clock::start()
{
  if (this->stopped)
  {
    this->updated = esp_timer_get_time();
    this->stopped = false;
  }
  return true;
}

bool Clock::isStopped()
{
  return this->stopped;
}

bool Clock::getTime(uint8_t* hour, uint8_t* minute, uint8_t* second)
{
  uint32_t epochSeconds;
  this->updateEpoch();
  epochSeconds = (this->epochMilliseconds / 1000) % 86400;

  if (NULL != hour)
    *hour = (epochSeconds / (60*60)) % 24;
  
  if (NULL != minute)
    *minute = (epochSeconds / 60) % 60;
  
  if (NULL != second)
    *second = epochSeconds % 60;
 
  return true;
}

uint16_t Clock::getRatio()
{
  return this->ratio;
}
