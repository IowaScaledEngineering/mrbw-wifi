#include "systemState.h"
#include "commonFuncs.h"
#include "defaultConfigFile.h"
#include "ESPmDNS.h"

SystemState::SystemState()
{
  this->loopCnt = 0;
  this->baseAddress = 0;
  this->activeThrottles = 0;
  this->debugWifiEnable = false;
  this->isFSConnected = false;
  this->isAutoNetwork = true;
  this->isWifiConnected = false;
  this->isCmdStnConnected = false;
  memset(this->ssid, 0, sizeof(this->ssid));
  memset(this->password, 0, sizeof(this->password));
  memset(this->hostname, 0, sizeof(this->hostname));

  this->cmdStnPort = 0;
  this->cmdStnIP.fromString("0.0.0.0");
  this->cmdStnType = CMDSTN_NONE;

  memset(this->macAddr, 0, sizeof(this->macAddr));
  // Get default MAC addr for station-class wifi
  esp_read_mac(this->macAddr, ESP_MAC_WIFI_STA);

  snprintf(this->hostname, sizeof(this->hostname), "mrbw-wifi-%02X%02x", this->macAddr[4], this->macAddr[5]);
}

SystemState::~SystemState()
{
}

uint8_t SystemState::mrbusSrcAddrGet()
{
  return 0xD0 + this->baseAddress;
}

bool SystemState::configWriteDefault(fs::FS &fs)
{
  bool writeSuccess = false;
  File f = fs.open(CONFIG_FILE_PATH, FILE_WRITE);
  if (!f)
    return false;

  Serial.printf("Writing configuration file\n");
  writeSuccess = f.print(defaultConfigFileContents);
  f.close();
  return writeSuccess;
}

bool configKeyValueSplit(char* key, uint32_t keySz, char* value, uint32_t valueSz, const char* configLine)
{
  char lineBuffer[256];
  char* separatorPtr = NULL;
  char* lineBufferPtr = NULL;
  uint32_t bytesToCopy;

  separatorPtr = strchr(configLine, '=');
  if (NULL == separatorPtr)
    return false;

  memset(key, 0, keySz);
  memset(value, 0, valueSz);

  // Copy the part that's eligible to be a key into the line buffer
  bytesToCopy = separatorPtr - configLine;
  if (bytesToCopy > sizeof(lineBuffer)-1)
    bytesToCopy = sizeof(lineBuffer);
  memset(lineBuffer, 0, sizeof(lineBuffer));
  strncpy(lineBuffer, configLine, bytesToCopy);

  lineBufferPtr = ltrim(rtrim(lineBuffer));
  if (0 == strlen(lineBufferPtr) || '#' == lineBufferPtr[0])
    return false;

  strncpy(key, lineBufferPtr, keySz);

  bytesToCopy = strlen(separatorPtr+1);
  if (bytesToCopy > sizeof(lineBuffer)-1)
    bytesToCopy = sizeof(lineBuffer);
  memset(lineBuffer, 0, sizeof(lineBuffer));
  strncpy(lineBuffer, separatorPtr+1, bytesToCopy);
  lineBufferPtr = ltrim(rtrim(lineBuffer));
  if (0 == strlen(lineBufferPtr))
  {
    memset(key, 0, keySz);
    return false;
  }
  strncpy(value, lineBufferPtr, valueSz);
  return true;
}

bool SystemState::configRead(fs::FS &fs)
{
  if (!this->isFSConnected)
  {
    // Plug in a bunch of defaults, there's no functional filesystem
    return false;
  }

  File f = fs.open(CONFIG_FILE_PATH);
  if (!f)
    return false;  // At this point, we've tried our best to fix it

  while(f.available())
  {
    char keyStr[128];
    char valueStr[128];
    bool kvFound = configKeyValueSplit(keyStr, sizeof(keyStr), valueStr, sizeof(valueStr), f.readStringUntil('\n').c_str());
    if (!kvFound)
      continue;

    // Okay, looks like we have a valid key/value pair, see if it's something we care about
    if (0 == strcmp(keyStr, "mode"))
    {
      if (0 == strcmp(valueStr, "lnwi"))
      {
        this->cmdStnType = CMDSTN_LNWI;
      }
      else if (0 == strcmp(valueStr, "withrottle"))
      {
        this->cmdStnType = CMDSTN_JMRI;
      }
      else if (0 == strcmp(valueStr, "esu"))
      {
        this->cmdStnType = CMDSTN_ESU;
      }
    }
    else if (0 == strcmp(keyStr, "ssid"))
    {
      strncpy(this->ssid, valueStr, sizeof(this->ssid));
      this->isAutoNetwork = false;
    }
    else if (0 == strcmp(keyStr, "password"))
    {
      strncpy(this->password, valueStr, sizeof(this->password));
    }
    else if (0 == strcmp(keyStr, "serverPort"))
    {
      this->cmdStnPort = atoi(valueStr);
    }
    else if (0 == strcmp(keyStr, "serverIP"))
    {
      this->cmdStnSuggestedIP.fromString((const char*)valueStr);
    }
    else if (0 == strcmp(keyStr, "hostname"))
    {
      strncpy(this->hostname, valueStr, sizeof(this->hostname));
    }
  }

  f.close();

  // Clean up some mess here
  // If there's a password but no network, kill it
  if (strlen(this->password) && 0 == strlen(this->ssid))
    memset(this->password, 0, sizeof(this->password));

  if (0 != strlen(this->ssid))
  {
    // FIXME: Evaluate if this ssid is something we understand and if so, set the cmdstn type
  }


 
  return true;
}

const char* SystemState::resetReasonStringGet()
{
  switch (this->resetReason)
  {
    case 1  : return "Vbat power on reset";
    case 3  : return "Software reset digital core";
    case 4  : return "Legacy watch dog reset digital core";
    case 5  : return "Deep Sleep reset digital core";
    case 6  : return "Reset by SLC module, reset digital core";
    case 7  : return "Timer Group0 Watch dog reset digital core";
    case 8  : return "Timer Group1 Watch dog reset digital core";
    case 9  : return "RTC Watch dog Reset digital core";
    case 10 : return "Instrusion tested to reset CPU";
    case 11 : return "Time Group reset CPU";
    case 12 : return "Software reset CPU";
    case 13 : return "RTC Watch dog Reset CPU";
    case 14 : return "for APP CPU, reseted by PRO CPU";
    case 15 : return "Reset when the vdd voltage is not stable";
    case 16 : return "RTC Watch dog reset digital core and rtc module";
    default : return "No Idea";
  }
}

const char* SystemState::wifiSecurityTypeStringGet(wifi_auth_mode_t e)
{
  const char* encType = "UNKNOWN";
  switch(e)
  {
    case WIFI_AUTH_OPEN:
      encType = "OPEN";
      break;
    case WIFI_AUTH_WAPI_PSK:
      encType = "WAPI_PSK";
      break;
    case WIFI_AUTH_WEP:
      encType = "WEP";
      break;
    case WIFI_AUTH_WPA_PSK:
      encType = "WPA";
      break;
    case WIFI_AUTH_WPA_WPA2_PSK:
      encType = "WPA/WPA2_PSK";
      break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
      encType = "WPA2_ENT";
      break;
    case WIFI_AUTH_WPA2_PSK:
      encType = "WPA2_PSK";
      break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
      encType = "WPA2/WPA3_PSK";
      break;
    case WIFI_AUTH_WPA3_PSK:
      encType = "WPA3_PSK";
      break;
    default: // Unknown - why are we here?
      encType = "UNKNOWN";
      break;
  }
  return encType;
}

bool isHexChar(const char c)
{
  if ( (c >= '0' && c <= '9')
    || (c >= 'A' && c <= 'F')
    || (c >= 'a' && c <= 'f'))
    return true;
  return false;
}

bool isDigitraxSSID(const char* ssid)
{
  const char* ptr = ssid;
  //self.lnwiSSIDMatch = re.compile(r'Dtx\d+-[A-Za-z0-9 ]+_[0-9A-F][0-9A-F][0-9A-F][0-9A-F]-[0-7]')
  if (0 != strncmp(ptr, "Dtx", 3))
    return false;

  // Past the Dtx part - scan until - and make sure it's all digits
  ptr += 3;
  while(*ptr != '-' && *ptr != 0)
  {
    if (!isdigit(*ptr++))
      return false;
  }

  if (*ptr++ != '-')
    return false;

  while(*ptr != '_' && *ptr != 0)
  {
    if (!isalnum(*ptr) && *ptr != ' ')
      return false;
    ptr++;
  }

  if (*ptr++ != '_')
    return false;

  // Make sure we have enough characters to go - 4 of the MAC, a dash, and a channel thing?
  if (strlen(ptr) != 6)
    return false;

  if (isHexChar(ptr[0]) && isHexChar(ptr[1]) && isHexChar(ptr[2]) && isHexChar(ptr[3]) && '-' == ptr[4] && (ptr[5] >= '0' && ptr[5] <= '7'))
    return true;
  return false;
}

bool isDccExSSID(const char* ssid)
{
  //self.dccexSSIDMatch = re.compile(r'DCCEX_[A-Fa-f0-9][A-Fa-f0-9][A-Fa-f0-9][A-Fa-f0-9][A-Fa-f0-9][A-Fa-f0-9]')
  if (12 != strlen(ssid) || 0 != strncmp(ssid, "DCCEX_", 6))
    return false;

  for(uint8_t c=6; c<12; c++)
    if (!isHexChar(ssid[c]))
      return false;
  return true;
}

bool SystemState::cmdStnIPSetup()
{
  if (0 != this->cmdStnSuggestedIP)
  {
    this->cmdStnIP = this->cmdStnSuggestedIP;
    return true;
  }

  const char* svcNameStr = "";

  if (CMDSTN_LNWI == this->cmdStnType || CMDSTN_JMRI == this->cmdStnType)
  {
    svcNameStr = "withrottle";
  } else if (CMDSTN_ESU == this->cmdStnType) { 
    svcNameStr = "esu-mrtp";
  }

  // Do we have a protocol to look for?  Then go searching
  if (strlen(svcNameStr))
  {
    Serial.printf("MDNS starting for service [%s]\n", svcNameStr);
    int32_t numHosts = MDNS.queryService(svcNameStr, "tcp");

    if (numHosts > 0)
    {
      // Just pick the first one for now.  Maybe there's some smarterish later?
      Serial.printf("MDNS found %d hosts\n", numHosts);
      Serial.print("Hostname: ");
      Serial.println(MDNS.hostname(0));
      Serial.print("IP address: ");
      Serial.println(MDNS.IP(0));
      Serial.print("Port: ");
      Serial.println(MDNS.port(0));

      // In the case of auto-discovery, set both the IP and port
      this->cmdStnIP = MDNS.IP(0);
      this->cmdStnPort = MDNS.port(0);
      return true;
    }
  }

  // As a last resort, try hard-coded rules about where certain devices (LNWIs, ESUs)
  //  live based on just how they're built.
  if (CMDSTN_LNWI == this->cmdStnType
    || (CMDSTN_JMRI == this->cmdStnType && isDccExSSID(this->ssid))
    || (CMDSTN_ESU == this->cmdStnType && 0 == strcmp(this->ssid, "ESUWIFI")))
  {
    // LNWIs are always on .1
    // Same for ESU CabControls if we're talking to its wifi network
    // DCC-EXs currently don't seem to do mDNS at all, but again .1
    // Take our IP, make it a.b.c.1 and return it
    this->cmdStnIP = this->localIP;
    this->cmdStnIP[3] = 1;
    return true;
  }


  return false;
}

bool SystemState::wifiScan()
{
  if (!this->isAutoNetwork)
    return true; // Don't scan if we're not in automatic mode

  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);

  Serial.printf("Beginning Wifi Scan\n");
  int totalNetworks = WiFi.scanNetworks();
  Serial.printf("Scan done, %d networks found\n", totalNetworks);


  // Initialize wifi fields
  memset(this->ssid, 0, sizeof(this->ssid));
  memset(this->password, 0, sizeof(this->password));
  this->cmdStnPort = 0;

  for (int n=0; n<totalNetworks; n++)
  {
    String ssid;
    uint8_t auth;
    int32_t rssi;
    uint8_t* bssid;
    int32_t channel;

    WiFi.getNetworkInfo(n, ssid, auth, rssi, bssid, channel);
    // Basic rule here is we should have the networks sorted by signal strength
    // First one that matches our command station type (if set) wins
    Serial.printf("[%-32.32s] Ch: %2d RSSI: %d Auth: %s\n", ssid.c_str(), channel, rssi, wifiSecurityTypeStringGet((wifi_auth_mode_t)auth));
  }

  for (int n=0; n<totalNetworks; n++)
  {
    String ssid;
    uint8_t auth;
    int32_t rssi;
    uint8_t* bssid;
    int32_t channel;

    WiFi.getNetworkInfo(n, ssid, auth, rssi, bssid, channel);

    // Let's look through these networks and see if we find anything meeting our needs
    if ((this->cmdStnType == CMDSTN_NONE || this->cmdStnType == CMDSTN_ESU) && ssid == "ESUWIFI")
    {
      // Auto-configuration for ESU CabControl systems
      this->cmdStnType = CMDSTN_ESU;
      strncpy(this->ssid, "ESUWIFI", sizeof(this->ssid));
      if (auth != WIFI_AUTH_OPEN)
        strncpy(this->password, "123456789", sizeof(this->password));
      this->cmdStnPort = 15471;
      break;
    }
    else if ((this->cmdStnType == CMDSTN_NONE || this->cmdStnType == CMDSTN_LNWI) && (auth == WIFI_AUTH_OPEN) && isDigitraxSSID(ssid.c_str()))
    {
      // Auto-configuration for Digitrax LNWIs
      this->cmdStnType = CMDSTN_LNWI;
      strncpy(this->ssid, ssid.c_str(), sizeof(this->ssid));
      this->cmdStnPort = 12090;
      break;
    }
    else if ((this->cmdStnType == CMDSTN_NONE || this->cmdStnType == CMDSTN_JMRI) && (auth == WIFI_AUTH_OPEN) && ssid == "MRCWi-Fi")
    {
      // Auto-configuration for MRC WiFi Modules
      this->cmdStnType = CMDSTN_JMRI;
      strncpy(this->ssid, ssid.c_str(), sizeof(this->ssid));
      this->cmdStnPort = 12090;
      break;
    }
    else if ((this->cmdStnType == CMDSTN_NONE || this->cmdStnType == CMDSTN_JMRI) && ssid == "RPi-JMRI")
    {
      // Auto-configuration for Steve Todd's JMRI RasPi Image
      this->cmdStnType = CMDSTN_JMRI;
      strncpy(this->ssid, ssid.c_str(), sizeof(this->ssid));
      if (auth != WIFI_AUTH_OPEN)
        strncpy(this->password, "rpI-jmri", sizeof(this->password));

      if (0 == this->cmdStnPort)
        this->cmdStnPort = 12090;

      break;
    }
    else if ((this->cmdStnType == CMDSTN_NONE || this->cmdStnType == CMDSTN_JMRI) && isDccExSSID(ssid.c_str()))
    {
      // Auto-configuration for DCC-EX Command Stations
      this->cmdStnType = CMDSTN_JMRI;
      strncpy(this->ssid, ssid.c_str(), sizeof(this->ssid));
      if (auth != WIFI_AUTH_OPEN)
        snprintf(this->password, sizeof(this->password), "PASS_%s", ssid.c_str()+6);

      if (0 == this->cmdStnPort)
        this->cmdStnPort = 2560;
      break;
    }
  }

  if (strlen(this->ssid))
  {
    Serial.printf("Found network [%s] pass [%s] port [%d] type [%d]\n", this->ssid, this->password, this->cmdStnPort, this->cmdStnType);
    return true;
  }

  return false;
}