#ifndef _MRBUS_PACKETS_H_
#define _MRBUS_PACKETS_H_
#include "SystemState.h"
#include "mrbus.h"

bool sendMRBusTimePacket(SystemState& systemState, MRBus& mrbus);
bool sendMRBusVersionPacket(SystemState& systemState, MRBus& mrbus);
bool isMRBusThrottlePacket(uint8_t srcAddr, MRBusPacket& pkt);
#endif