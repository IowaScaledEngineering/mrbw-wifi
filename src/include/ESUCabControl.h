#pragma once
#include "CommandStation.h"
#include "ThrottleState.h"
#include "periodicEvent.h"

#define ESUCC_RX_BUFFER_SZ  4096


class ESUCCLocRef : public CmdStnLocRef
{
  public:
    int32_t objID;
    using CmdStnLocRef::CmdStnLocRef;
    ESUCCLocRef(uint16_t locAddr, bool isLongAddr, uint8_t mrbusAddr, int32_t objID);
};

class ESUCabControl : public CommandStation
{
  private:
    bool debug;
    WiFiClient *cmdStnConnection;
    uint8_t* rxBuffer;
    uint32_t rxBufferUsed;
    PeriodicEvent keepaliveTimer;
    bool trackPowerOn;
    uint32_t keepaliveMaxInterval;
    ThrottleState* throttleStates[MAX_THROTTLES];
    int32_t queryLocomotiveObjectGet(uint16_t locAddr);
    int32_t queryAddLocomotiveObject(uint16_t locAddr);
    bool queryLocomotiveObjectFunctionGet(int32_t objID, uint8_t funcNum);
    bool queryAcquireLocomotiveObject(int32_t objID);
    bool queryLocomotiveObjectSpeedSet(int32_t objID, uint8_t speed, bool isReverse);
    bool queryLocomotiveObjectFunctionSet(int32_t objID, uint8_t funcNum, bool funcVal);
    bool queryLocomotiveObjectEmergencyStop(int32_t objID);
    bool queryReleaseLocomotiveObject(int32_t objID);
    int32_t query(const char *queryStr, char **replyBuffer, int32_t *errCode, int32_t timeout_ms=1000);

  public:
    ESUCabControl();
    ~ESUCabControl();
    bool begin(WiFiClient &cmdStnConnection, uint32_t quirkFlags);
    bool end();
    bool update();
    void rxtx();
    void rxtx(const char *cmdStr);

    bool locomotiveObjectGet(ThrottleState *locCmdStnRef, uint16_t addr, bool isLongAddr, uint8_t mrbusAddr);
    bool locomotiveEmergencyStop(ThrottleState* locCmdStnRef);
    bool locomotiveSpeedSet(ThrottleState* locCmdStnRef, uint8_t speed, bool isReverse);
    bool locomotiveFunctionsGet(ThrottleState* locCmdStnRef, bool functionStates[]);
    bool locomotiveFunctionSet(ThrottleState* locCmdStnRef, uint8_t funcNum, bool funcActive);
    bool locomotiveDisconnect(ThrottleState* locCmdStnRef);
};