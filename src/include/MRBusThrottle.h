#pragma once
#include <stddef.h>
#include <stdint.h>
#include "commonFuncs.h"
#include "CommandStation.h"
#include "mrbus.h"

class MRBusThrottle
{
    private:
      bool active;
      uint8_t throttleAddr;
      uint16_t locAddr;
      bool isLongAddr;
      uint8_t locSpeed;
      bool locRevDirection;
      bool locEStop;
      bool locFunctions[MAX_FUNCTIONS];
      bool locFunctionsGood;
      uint64_t lastUpdate;
      bool isCmdStnReferenceValid;
      CmdStnLocRef *locCmdStnReference;

    public:
        MRBusThrottle();
        ~MRBusThrottle();
        void initialize(uint8_t mrbusAddr);
        uint8_t mrbusAddrGet();
        bool isActive();
        bool isExpired(uint32_t idleMilliseconds);
        bool release(CommandStation *cmdStn);
        bool disconnect(CommandStation *cmdStn);
        void update(CommandStation *cmdStn, MRBusPacket &pkt);
};
