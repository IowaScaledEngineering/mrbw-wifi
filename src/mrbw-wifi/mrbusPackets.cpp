#include "mrbusPackets.h"
#include "versions.h"

#define TIME_FLAGS_DISP_FAST       0x01
#define TIME_FLAGS_DISP_FAST_HOLD  0x02
#define TIME_FLAGS_DISP_REAL_AMPM  0x04
#define TIME_FLAGS_DISP_FAST_AMPM  0x08

bool isMRBusThrottlePacket(uint8_t srcAddr, MRBusPacket& pkt)
{
  if (pkt.dest == srcAddr && pkt.data[0] == 'S' && pkt.len == 15
    && (pkt.src >= MRBUS_THROTTLE_BASE_ADDR && pkt.src < MRBUS_THROTTLE_BASE_ADDR + MAX_THROTTLES))
    return true;

  return false;
}

bool sendMRBusTimePacket(SystemState& systemState, MRBus& mrbus)
{
  MRBusPacket timePkt;
  // If the fast clock isn't enabled, don't send a time packet
  if (!systemState.fastClock.isEnabled())
    return false;

  timePkt.src = systemState.mrbusSrcAddrGet();
  timePkt.dest = 0xFF;
  timePkt.len = 14;
  timePkt.data[0]  = 'T';
  timePkt.data[1]  = 0; // Real hours
  timePkt.data[2]  = 0; // Real minutes
  timePkt.data[3]  = 0; // Real seconds
  timePkt.data[4]  = 0; // Flags

  if (systemState.fastClock.isStopped())
  {
    timePkt.data[4] |= TIME_FLAGS_DISP_FAST_HOLD;
  }

  timePkt.data[4] |= TIME_FLAGS_DISP_FAST;

  systemState.fastClock.getTime(&timePkt.data[5], &timePkt.data[6], &timePkt.data[7]);

  uint16_t mrbusFTRatio = systemState.fastClock.getRatio() / 100;
  timePkt.data[8] = 0xFF & (mrbusFTRatio>>8);
  timePkt.data[9] = mrbusFTRatio & 0xFF;
  return mrbus.txPktQueue->push(timePkt);
}


bool sendMRBusVersionPacket(SystemState& systemState, MRBus& mrbus)
{
  // Send version packet
  MRBusPacket versionPkt;
  const char* const gitRev = GIT_REV;
  versionPkt.src = systemState.mrbusSrcAddrGet();
  versionPkt.dest = 0xFF;
  versionPkt.len = 14;
  versionPkt.data[0]  = 'v';
  versionPkt.data[1]  = 0x80;
  for(uint32_t i=0; i<6; i+=2)
  {
    char hexPair[3];
    memcpy(hexPair, gitRev + i, 2);
    hexPair[2] = 0;
    versionPkt.data[i/2 + 2] = strtol(hexPair, NULL, 16);
  }
  versionPkt.data[5]  = 1;
  versionPkt.data[6]  = 0;

  if (!systemState.isWifiConnected)
  {
    memcpy(versionPkt.data + 7, "NO WIFI", 7);
  } else if (!systemState.isCmdStnConnected) {
    memcpy(versionPkt.data + 7, "NO CMST", 7);
  } else if (systemState.isWifiConnected && systemState.isCmdStnConnected) {
    switch(systemState.cmdStnType)
    {
        case CMDSTN_DCCEX:
          memcpy(versionPkt.data + 7, "WF-DCCX", 7);
          break;
        case CMDSTN_JMRI:
          memcpy(versionPkt.data + 7, "WF-WTHR", 7);
          break;
        case CMDSTN_LNWI:
          memcpy(versionPkt.data + 7, "WF-LNWI", 7);
          break;
        case CMDSTN_ESU:
          memcpy(versionPkt.data + 7, "WF-ESU ", 7);
          break;
        default:
          memcpy(versionPkt.data + 7, "WF-UNKN", 7);
          break;
    }
  } else {
    memcpy(versionPkt.data + 7, "WF-XXX", 7);
  }

  return mrbus.txPktQueue->push(versionPkt);
}
