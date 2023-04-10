#include "mrbus.h"
#include "driver/uart.h"

MRBus::MRBus()
{
  this->addr = 0x03;
  this->rxBuffer = new uint8_t[this->rxBufferSz];
  this->rxBufferUsed = 0;

  this->rxPktQueue = new RingBuffer<MRBusPacket>(16);
  this->txPktQueue = new RingBuffer<MRBusPacket>(16);  
}

MRBus::~MRBus()
{
  delete[] this->rxBuffer;
  delete this->rxPktQueue;
  delete this->txPktQueue;
}

#define UART_BUFFER_SZ  (128 * 2)

bool MRBus::setAddress(uint8_t address)
{
  this->addr = address;
  return true;
}

bool MRBus::begin()
{
  //this->uart.begin(57600, SERIAL_8N1, 18, 17, false, 2000, 1020, 1024);
  
  const uart_port_t uart_num = UART_NUM_1;
  uart_config_t uart_config = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, //UART_HW_FLOWCTRL_CTS_RTS,
    .rx_flow_ctrl_thresh = 122,
  };
  // Configure UART parameters
  ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));

  // Set UART pins(TX: IO4, RX: IO5, RTS: IO18, CTS: IO19)
  ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, 17, 18, 16, 15));

  //ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, UART_BUFFER_SZ, UART_BUFFER_SZ, 10, &this->uartQueue, 0));
  ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, UART_BUFFER_SZ, UART_BUFFER_SZ, 0, NULL, 0));
  uart_flush(UART_NUM_1);

  return true;
}

void MRBus::transmitPackets()
{
  size_t bytesAvailable;
  uart_get_tx_buffer_free_size(UART_NUM_1, &bytesAvailable);
  if (this->txPktQueue->isEmpty() || bytesAvailable < 64)
    return;

  MRBusPacket pkt;
  if (!this->txPktQueue->pop(pkt))
    return;
  
  uint8_t* xBeeTxBuffer = new uint8_t[64];
  uint8_t* xBeeTxBuffer_escaped = new uint8_t[64];

/*
#     txBuffer.append(0x7E)       # 0 - Start 
#     txBuffer.append(0x00)       # 1 - Len MSB
#     txBuffer.append(txPktLen)   # 2 - Len LSB
#     txBuffer.append(0x01)       # 3 - API being called - transmit by 16 bit address
#     txBuffer.append(0x00)       # 4 - Frame identifier
#     txBuffer.append(0xFF)       # 5 - MSB of dest address - broadcast 0xFFFF
#     txBuffer.append(0xFF)       # 6 - LSB of dest address - broadcast 0xFFFF
#     txBuffer.append(0x00)       # 7 - Transmit Options
     
#     txBuffer.append(dest)           # 8 / 0 - Destination
#     txBuffer.append(src)            # 9 / 1 - Source
#     txBuffer.append(len(data) + 5)  # 10/ 2 - Length
#     txBuffer.append(0)              # 11/ 3 - CRC High
#     txBuffer.append(0)              # 12/ 4 - CRC Low

TX: [7e 00 18 01 00 ff ff 00 ff d0 7d 33 1b cf 76 80 ab cd ef 01 00 4e 4f 20 57 49 46 49 ]
*/
  pkt.src = this->addr;
  pkt.calculateCRC(); // Update the CRC just to make sure we've got it
  //Serial.printf("TX src=%02x dst=%02x len=%d crc=%04x\n", pkt->src,pkt->dest, pkt->len + 5, pkt->crc);

  xBeeTxBuffer[0] = 0x7E;
  xBeeTxBuffer[1] = 0x00;
  xBeeTxBuffer[2] = 10 + pkt.len;
  xBeeTxBuffer[3] = 0x01;
  xBeeTxBuffer[4] = 0x00;
  xBeeTxBuffer[5] = 0xFF;
  xBeeTxBuffer[6] = 0xFF;
  xBeeTxBuffer[7] = 0x00;

  xBeeTxBuffer[8] = pkt.dest;
  xBeeTxBuffer[9] = pkt.src; 
  xBeeTxBuffer[10] = pkt.len + 5;
  xBeeTxBuffer[11] = 0xFF & (pkt.crc);
  xBeeTxBuffer[12] = 0xFF & (pkt.crc>>8);
  for (int i=0; i<pkt.len; i++)
    xBeeTxBuffer[13 + i] = pkt.data[i];
  
  xBeeTxBuffer[13 + pkt.len] = 0;
  for (int i=3; i<13+pkt.len; i++)
    xBeeTxBuffer[13 + pkt.len] += xBeeTxBuffer[i];

  xBeeTxBuffer[13 + pkt.len] = 0xFF - xBeeTxBuffer[13 + pkt.len];

  uint8_t xbtx_idx = 0; // Number of bytes in escaped buffer
  xBeeTxBuffer_escaped[xbtx_idx++] = xBeeTxBuffer[0];
  for(int i=1; i<13 + 1 + pkt.len; i++)
  {
    switch(xBeeTxBuffer[i])
    {
      case 0x7E:
      case 0x7D:
      case 0x11:
      case 0x13:
        xBeeTxBuffer_escaped[xbtx_idx++] = 0x7D;
        xBeeTxBuffer_escaped[xbtx_idx++] = xBeeTxBuffer[i] ^ 0x20;
        break;
      default:
        xBeeTxBuffer_escaped[xbtx_idx++] = xBeeTxBuffer[i];
        break;
    }
  }
  uart_write_bytes(UART_NUM_1, xBeeTxBuffer_escaped, xbtx_idx);

  delete xBeeTxBuffer;
  delete xBeeTxBuffer_escaped;
}

bool MRBus::processSerial()
{
  this->transmitPackets();

  int bytesRead = uart_read_bytes(UART_NUM_1, this->rxBuffer + this->rxBufferUsed, this->rxBufferSz - this->rxBufferUsed - 1, 0);
  
  // Error case
  if (bytesRead < 0)
    return false;

  this->rxBufferUsed += bytesRead;

  // No bytes?  Get out!
  if (0 == this->rxBufferUsed)
    return false;

  //Serial.printf("Read %d bytes\n", this->rxBufferUsed);

  // Read until we find a start of frame marker
  uint8_t* startPtr = this->rxBuffer;
  uint8_t* endPtr = this->rxBuffer + this->rxBufferUsed;

  uint8_t* startOfUnprocessedData = startPtr;

  bool rxInPacket = false;
  bool rxEscapeNext = false;
  uint32_t rxExpectedPktLen = 255;
  uint8_t xBeePkt[64];
  uint32_t xBeePktLen = 0;

  //Serial.printf("Processing: [");

  while(startPtr < endPtr)
  {
    uint8_t b = *startPtr++;
    //Serial.printf("0x%02X ", b);
    switch(b)
    {
      case 0x7E:  // XBee Start of Frame character
        xBeePktLen = 0;
        xBeePkt[xBeePktLen++] = 0x7E;
        rxEscapeNext = false;
        rxInPacket = true;

        break;

      case 0x7D: // XBee escape character
        rxEscapeNext= true;
        break;

      default:
        if (!rxInPacket)
        {
          // This is just garbage, we're not in a packet
          startOfUnprocessedData = startPtr;
          break;
        } else {
          if (rxEscapeNext)
          {
            b ^= 0x20;
            rxEscapeNext = false;
          }
          xBeePkt[xBeePktLen++] = b;
          if (3 == xBeePktLen)
            rxExpectedPktLen = (((uint32_t)xBeePkt[1])<<8) + xBeePkt[2] + 4;

          // See if we've completed packet processing
          if (xBeePktLen == rxExpectedPktLen)
          {
            uint8_t checksum = 0;
            for(int z=3; z<rxExpectedPktLen; z++)
              checksum = (checksum + xBeePkt[z]) & 0xFF;

            if (0xFF == checksum)
            {
              // Successful checksum
              uint8_t pktDataOffset = 0;
              if (0x80 == xBeePkt[3])
                pktDataOffset = 14; // 64 bit addressing
              else if (0x81 == xBeePkt[3])
                pktDataOffset = 8; // 16 bit addressing

              if(pktDataOffset)
              {
                MRBusPacket pkt;
                bool goodPacket = pkt.fromBuffer(xBeePkt + pktDataOffset, xBeePktLen - pktDataOffset);
                if (goodPacket && (pkt.dest == this->addr || pkt.dest == 0xFF) && !this->rxPktQueue->isFull())
                  this->rxPktQueue->push(pkt);
              }
            }

            xBeePktLen = 0;
            memset(xBeePkt, 0, sizeof(xBeePkt));
            rxEscapeNext = false;
            rxInPacket = false;
            startOfUnprocessedData = startPtr;
          }
        }
        break;
    }
  }
  //Serial.printf("]\n");
  this->rxBufferUsed = endPtr - startOfUnprocessedData;
  if (this->rxBufferUsed) // If there's any bytes left over, move them up in the buffer
  {
    //Serial.printf("Saving off %d bytes\n", this->rxBufferUsed);
    memmove(this->rxBuffer, startOfUnprocessedData, this->rxBufferUsed);
  }

  return true;
}

MRBusPacket::MRBusPacket()
{
  this->src = 0x03;
  this->dest = 0xFF;
  this->len = 0;
  this->crc = 0;
  memset(this->data, 0, sizeof(this->data));
}

MRBusPacket& MRBusPacket::operator=(const MRBusPacket& c)
{
  this->src = c.src;
  this->dest = c.dest;
  this->len = c.len;
  this->crc = c.crc;
  memcpy(this->data, c.data, sizeof(this->data));
  return *this;
}

bool MRBusPacket::fromBuffer(uint8_t* buffer, uint8_t bufferSz)
{
  if (bufferSz < 6)
    return false;

  this->src = buffer[MRBUS_PKT_SRC];
  this->dest = buffer[MRBUS_PKT_DEST];
  this->len = (5+sizeof(this->data))<buffer[MRBUS_PKT_LEN] ? (sizeof(this->data)):buffer[MRBUS_PKT_LEN]-5;
  this->crc = (((uint16_t)buffer[MRBUS_PKT_CRC_H])<<8) | buffer[MRBUS_PKT_CRC_L];

  memset(this->data, 0, sizeof(this->data));
  for (int i=0; i<this->len-5; i++)
    this->data[i] = buffer[i+5];

  return true;
}

uint16_t MRBusPacket::calculateCRC()
{
  uint16_t crc = 0;

  for (int i=0; i<5+this->len; i++)
  {
    switch(i)
    {
      case 0:
        crc = this->crc16Update(crc, this->dest);
        break;
      case 1:
        crc = this->crc16Update(crc, this->src);
        break;
      case 2:
        crc = this->crc16Update(crc, this->len + 5); // Remember to add header length
        break;
      case 3:
      case 4:
        // These are the CRC bytes, do nothing
        break;
      default:
        crc = this->crc16Update(crc, this->data[i-5]);
        break;
    }
  }

  this->crc = crc;
  return crc;
}


uint16_t MRBusPacket::crc16Update(uint16_t crc, uint8_t a)
{
  const uint8_t MRBus_CRC16_HighTable[16] =
  {
    0x00, 0xA0, 0xE0, 0x40, 0x60, 0xC0, 0x80, 0x20,
    0xC0, 0x60, 0x20, 0x80, 0xA0, 0x00, 0x40, 0xE0
  };
  const uint8_t MRBus_CRC16_LowTable[16] =
  {
    0x00, 0x01, 0x03, 0x02, 0x07, 0x06, 0x04, 0x05,
    0x0E, 0x0F, 0x0D, 0x0C, 0x09, 0x08, 0x0A, 0x0B
  };

  uint8_t t = 0;
  uint8_t i = 0;
  uint8_t w = 0;
  uint8_t crc16_high = (crc >> 8) & 0xFF;
  uint8_t crc16_low = crc & 0xFF;

  for(uint8_t i=0; i<2; i++)
  {
    if (0 == i)
    {
      t = 0xF0 & (crc16_high ^ a);
      t = ((t << 4) & 0xF0) | ((t >> 4) & 0x0F);
    } else {
      w = (((crc16_high << 4) & 0xF0) | ((crc16_high >> 4) & 0x0F));
      t = (w ^ a) & 0x0F;
    }

    crc16_high = (crc16_high << 4) & 0xFF; 
    crc16_high |= (crc16_low >> 4);
    crc16_low = crc16_low << 4;

    crc16_high = crc16_high ^ MRBus_CRC16_HighTable[t];
    crc16_low = crc16_low ^ MRBus_CRC16_LowTable[t];
  }

  return ( (((uint16_t)crc16_high << 8) & 0xFF00) | (uint16_t)crc16_low );
}



