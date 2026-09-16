#pragma once

#include <stdint.h>
#include <Arduino.h>
#include <WiFi.h>
#include <FFat.h>
#include <vector>
#include <string>
#include <cctype>
#include <cstdio>
#include <cstring>
#include "CommandStation.h"
#include "periodicEvent.h"
#include "commonFuncs.h"
#include "Clock.h"
#include "esp_mac.h"
#include "MRBusThrottle.h"

#define CONFIG_FILE_PATH "/config.txt"
#define STRLN_SSID      32
#define STRLN_PASSwORD  64
#define STRLN_HOSTNAME  32
#define MAC_LEN         6

#define ESU_PORT_DEFAULT          15471
#define WITHROTTLE_PORT_DEFAULT   12090
#define DCCEX_PORT_DEFAULT        2560

#define WITHROTTLE_MDNS_NAME  "_withrottle"
#define ESU_MDNS_NAME         "_esu-mrtp"

typedef enum
{
  CMDSTN_NONE        = 0,
  CMDSTN_LNWI        = 1,
  CMDSTN_JMRI        = 2,
  CMDSTN_DCCEX       = 3,
  CMDSTN_WFD30       = 4,
  CMDSTN_ESU         = 100
} CommandStationType;

typedef enum
{
  DISPLAY_IP_LOCAL      = 0,
  DISPLAY_IP_LOCAL2     = 1,
  DISPLAY_IP_CMDSTN     = 2,
  DISPLAY_IP_CMDSTN2    = 3,
  DISPLAY_IP_MAX_FIELDS
} IPLineDisplay;

typedef enum
{
  FC_SOURCE_OFF         = 0,
  FC_SOURCE_CMDSTN      = 1
} FastClockSource;

#define MAX_CONFIG_INSTANCES 9

struct ConfigInstance {
  bool isUsed = false;
  int index = 0; // 1-9
  // Instance-specific properties
  std::string ssid = "";
  std::string password = "";
  std::string hostname = "";
  CommandStationType cmdStnType = CMDSTN_NONE;
  FastClockSource fcSource = FC_SOURCE_OFF;
  uint16_t serverPort = 0;
  IPAddress serverIP;
  uint8_t debugLvlCommandStation = DBGLVL_INFO;
  uint8_t debugLvlMRBus = DBGLVL_INFO;
  uint8_t debugLvlSystem = DBGLVL_INFO;
};

class SystemState
{
  private:

  public:
    uint32_t loopCnt;
    uint8_t baseAddress;
    int32_t rssi;
    bool isFSConnected;
    bool isWifiConnected;
    bool isCmdStnConnected;
    char ssid[STRLN_SSID+1];
    char password[STRLN_PASSwORD+1];
    uint8_t macAddr[MAC_LEN];
    int resetReason;
    bool debugWifiEnable;
    bool isAutoNetwork;
    uint16_t cmdStnPort;
    uint8_t activeThrottles;
    int8_t activeConfigNum;
    IPLineDisplay ipDisplayLine;
    IPAddress cmdStnIP;
    CommandStationType cmdStnType;
    bool cmdStnTypeSetByConfig;
    IPAddress localIP;
    WiFiClient cmdStnConnection;
    Clock fastClock;
    FastClockSource fcSource;
    PeriodicEvent conflictingBaseTimer;
    bool conflictingBase;
    CommandStation* cmdStn;

    SystemState();
    ~SystemState();

    uint8_t mrbusSrcAddrGet();
    bool configWriteDefault();
    bool configRead();
    const char *resetReasonStringGet();
    const char *wifiSecurityTypeStringGet(wifi_auth_mode_t e);
    const char *getHostname();
    bool cmdStnIPScan();
    bool cmdStnIPSetup();
    bool isConflictingBasePresent();
    bool wifiScan();
    bool cmdStnDisconnect();
    bool registerConflictingBase();

    bool isWifiScanComplete();
    bool wifiScanStart();
    void updateWifiRSSI(int32_t rssi);
    void updateActiveThrottleCount(MRBusThrottle* throttles);

    uint8_t debugLvlCommandStation;
    uint8_t debugLvlMRBus;
    uint8_t debug;
    std::vector<ConfigInstance> configs;
};
