#pragma once

#include <stdint.h>
#include <Arduino.h>

class PeriodicEvent
{
    private:
        uint64_t nextEvent;
        uint32_t interval_us;
        bool eventTriggered;
    public:
        PeriodicEvent();
        ~PeriodicEvent();
        void setup(uint16_t interval_ms);
        void reset();
        bool test(bool autoReset = false);
};
