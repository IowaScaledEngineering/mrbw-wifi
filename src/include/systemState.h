#pragma once

#include <stdint.h>
#include <Arduino.h>
#include <WiFi.h>
#include <FFat.h>
#include "CommandStation.h"

#define CONFIG_FILE_PATH "/config.txt"
#define STRLN_SSID      32
#define STRLN_PASSwORD  64
#define STRLN_HOSTNAME  32
#define MAC_LEN         6

typedef enum
{
  CMDSTN_NONE        = 0,
  CMDSTN_LNWI        = 1,
  CMDSTN_JMRI        = 2,
  CMDSTN_ESU         = 3
} CommandStationType;

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
    char hostname[STRLN_HOSTNAME+1];
    char ssid[STRLN_SSID+1];
    char password[STRLN_PASSwORD+1];
    uint8_t macAddr[MAC_LEN];
    int resetReason;
    bool debugWifiEnable;
    bool isAutoNetwork;
    uint16_t cmdStnPort;
    uint8_t activeThrottles;
    IPAddress cmdStnIP;
    IPAddress cmdStnSuggestedIP;
    CommandStationType cmdStnType;
    IPAddress localIP;
    WiFiClient cmdStnConnection;
    CommandStation* cmdStn;

    SystemState();
    ~SystemState();

    uint8_t mrbusSrcAddrGet();

    bool configWriteDefault(fs::FS &fs);

    bool configRead(fs::FS &fs);

    const char *resetReasonStringGet();

    const char *wifiSecurityTypeStringGet(wifi_auth_mode_t e);

    bool cmdStnIPScan();

    bool cmdStnIPSetup();

    bool wifiScan();
};
