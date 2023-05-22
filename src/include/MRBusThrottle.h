#pragma once

#include <stddef.h>
#include <stdint.h>
#include "commonFuncs.h"
#include "CommandStation.h"
#include "ThrottleState.h"
#include "mrbus.h"

class MRBusThrottle
{
    private:
      ThrottleState tState;
      uint8_t throttleAddr;
      uint8_t debug;
    public:
        MRBusThrottle();
        ~MRBusThrottle();
        CmdStnLocRef *getCmdStnRef();
        void initialize(uint8_t mrbusAddr, uint8_t debugLvl = DBGLVL_INFO);
        uint8_t mrbusAddrGet();
        bool isActive();
        bool isExpired(uint32_t idleMilliseconds);
        bool release(CommandStation *cmdStn);
        bool disconnect(CommandStation *cmdStn);
        void update(CommandStation *cmdStn, MRBusPacket &pkt);
};
