#pragma once

#include <stdint.h>
#include <Wire.h>



class I2CDisplay
{
    private:
        TwoWire* i2c;
        uint8_t* displayBuffer;
        uint8_t* writeCache;
        uint8_t addr;
        bool unsentChanges;
    public:
        I2CDisplay();
        ~I2CDisplay();
        bool setup(TwoWire& i2cInterface = Wire, uint8_t address=0x3C);
        bool sendCmdList(const uint8_t* cmdStr, uint32_t cmdStrSz);
};



