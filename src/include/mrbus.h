#pragma once

#include <stdint.h>
#include <Arduino.h>
#include "ringbuffer.h"
#include "commonFuncs.h"

#define MRBUS_PKT_DEST  0
#define MRBUS_PKT_SRC   1
#define MRBUS_PKT_LEN   2
#define MRBUS_PKT_CRC_L 3
#define MRBUS_PKT_CRC_H 4
#define MRBUS_PKT_TYPE  5
#define MRBUS_PKT_SUBTYPE 6

#define MRBUS_PKT_DATA_LEN  15

class MRBusPacket
{
  private:

  public:
    uint8_t src;
    uint8_t dest;
    uint8_t len;
    uint16_t crc;
    uint8_t data[MRBUS_PKT_DATA_LEN];
    

    MRBusPacket();
    bool fromBuffer(uint8_t* buffer, uint8_t bufferSz);
    uint16_t calculateCRC();
    bool verifyCRC();
    static uint16_t crc16Update(uint16_t crc, uint8_t a);
    MRBusPacket& operator=(const MRBusPacket& c);
};


class MRBus
{
    private:
      uint8_t addr;
      uint8_t debug;
      uint8_t* rxBuffer;
      const uint32_t rxBufferSz = 256;
      uint32_t rxBufferUsed;
      QueueHandle_t uartQueue;
    public:
        MRBus();
        ~MRBus();
        bool begin(uint8_t debugLvl = DBGLVL_INFO);
        void transmitPackets();
        bool processSerial();
        bool setAddress(uint8_t address);
        void debugLevelSet(uint8_t debugLvl);
        RingBuffer<MRBusPacket> *rxPktQueue;
        RingBuffer<MRBusPacket>* txPktQueue;
};
