#pragma once

#include <stdint.h>
#include <Arduino.h>

class Switches
{
    private:
        uint8_t baseAddress;
        bool switchA;
        bool switchFR;
    public:
        Switches();
        ~Switches();
        void setup();
        void refresh();
        uint8_t baseAddressGet();
        uint8_t factoryResetGet();
};
