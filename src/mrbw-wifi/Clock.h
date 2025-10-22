#pragma once
#include <stdint.h>
#include <Arduino.h>
#include "commonFuncs.h"


class Clock
{
  private:
    uint32_t epochMilliseconds;
    uint64_t updated;
    uint16_t ratio;
    bool stopped;
    bool enabled;
    void updateEpoch();

  public:
    Clock();
    ~Clock();
    bool isEnabled();
    void enable();
    void disable();
    bool begin(uint8_t hour = 0, uint8_t minute=0, uint8_t second=0, uint16_t ratio = 100);
    bool setTime(uint8_t hour, uint8_t minute, uint8_t second, uint16_t ratio = 0);
    bool setTime(uint32_t epochSeconds, uint16_t ratio);
    bool stop();
    bool start();
    bool isStopped();
    bool getTime(uint8_t *hours, uint8_t *minutes, uint8_t *seconds);
    uint16_t getRatio();
};
