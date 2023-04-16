#pragma once
#include "CommandStation.h"
#include "periodicEvent.h"


#define WITHROTTLE_QUIRK_LNWI    0x00000001
#define WITHROTTLE_RX_BUFFER_SZ  2048

class WiThrottleLocRef : public CmdStnLocRef
{
  public:
    uint8_t multiThrottleLetter;
    using CmdStnLocRef::CmdStnLocRef;
    WiThrottleLocRef(uint16_t locAddr, bool isLongAddr, uint8_t mrbusAddr, uint8_t multiThrottleLetter);
};

class WiThrottle : public CommandStation
{
  private:
    const char *validThrottleLetters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    WiFiClient *cmdStnConnection;
    bool lnwiMode;
    uint8_t* rxBuffer;
    uint32_t rxBufferUsed;
    PeriodicEvent keepaliveTimer;
    bool trackPowerOn;
    uint32_t keepaliveMaxInterval;
    ThrottleState* throttleStates[MAX_THROTTLES];
    bool locomotiveFunctionSetJMRI(ThrottleState* tState, uint8_t funcNum, bool funcActive);
    bool locomotiveFunctionSetLNWI(ThrottleState* tState, uint8_t funcNum, bool funcActive);
    ThrottleState* getThrottleStateForMultiThrottleLetter(uint8_t letter);


  public:
    WiThrottle();
    ~WiThrottle();
    void rxtx();
    void rxtx(const char * cmdStr);
    uint8_t getMultiThrottleLetter(uint8_t mrbusAddr, ThrottleState *tState);
    void releaseMultiThrottleLetter(uint8_t mrbusAddr);
    void processResponse(const uint8_t* rxData, uint32_t rxDataLen);
    bool begin(WiFiClient &cmdStnConnection, uint32_t quirkFlags);
    bool end();
    bool update();
    bool locomotiveObjectGet(ThrottleState* tState, uint16_t addr, bool isLongAddr, uint8_t mrbusAddr);
    bool locomotiveEmergencyStop(ThrottleState* tState);
    bool locomotiveSpeedSet(ThrottleState* tState, uint8_t speed, bool isReverse);
    bool locomotiveFunctionsGet(ThrottleState* tState, bool functionStates[]);

    bool locomotiveFunctionSet(ThrottleState* tState, uint8_t funcNum, bool funcActive);
    bool locomotiveDisconnect(ThrottleState* tState);
};