#pragma once

#include <stdint.h>
#include <Arduino.h>
#include <Wire.h>

class I2CDisplay
{
    private:
        TwoWire* i2c;
        uint8_t* displayBuffer;
        uint8_t* writeCache;
        uint8_t addr;
        uint8_t xPos;
        uint8_t yPos;

        bool unsentChanges;
    public:
        I2CDisplay();
        ~I2CDisplay();
        bool setup(TwoWire* i2cInterface = &Wire, uint8_t address=0x3C);
        bool refresh(bool force = false);
        bool clrscr(bool refresh = true);
        bool sendCmdList(const uint8_t *cmdStr, uint32_t cmdStrSz);
        uint32_t putstr(const char* s, int32_t x = -1, int32_t y=-1);
        bool drawISELogo();
        uint32_t putc(const uint8_t c, int32_t x = -1, int32_t y = -1);
        uint8_t getXPos();
        uint8_t getYPos();
        bool setXPos(uint8_t x);
        bool setYPos(uint8_t x);
};



