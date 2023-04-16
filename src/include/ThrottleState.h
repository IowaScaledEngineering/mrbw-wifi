#pragma once
#include <stdint.h>
#include "commonFuncs.h"

class ThrottleState
{
    public:
      bool active;
      uint16_t locAddr;
      bool isLongAddr;
      // This is the state of things from the throttle's point of view
      uint8_t locSpeed;
      bool locRevDirection;
      bool locEStop;
      bool locFunctions[MAX_FUNCTIONS];
      bool locFunctionsGood;
      uint64_t lastUpdate;
      void *locCmdStnRef;
    ThrottleState();
    void init();
    ~ThrottleState();
};
