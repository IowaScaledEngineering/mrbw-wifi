#include "SystemState.h"
#include "commonFuncs.h"
#include "defaultConfigFile.h"
#include "ESPmDNS.h"
#include "esp_mac.h"

SystemState::SystemState()
{
  this->activeConfigNum = -1;
  this->loopCnt = 0;
  this->baseAddress = 0;
  this->activeThrottles = 0;
  this->debugWifiEnable = false;
  this->isFSConnected = false;
  this->isAutoNetwork = true;
  this->isWifiConnected = false;
  this->isCmdStnConnected = false;
  this->cmdStnTypeSetByConfig = false;
  this->conflictingBaseTimer.setup(10000);  // Set base conflicts to last 10 seconds
  this->conflictingBaseTimer.reset();
  this->conflictingBase = false;
  this->ipDisplayLine = DISPLAY_IP_LOCAL;
  this->fcSource = FC_SOURCE_OFF;
  this->debug = DBGLVL_INFO;
  this->debugLvlMRBus = DBGLVL_INFO;
  this->debugLvlCommandStation = DBGLVL_INFO;

  memset(this->ssid, 0, sizeof(this->ssid));
  memset(this->password, 0, sizeof(this->password));

  this->cmdStnPort = 0;
  this->cmdStnIP.fromString("0.0.0.0");
  this->cmdStnType = CMDSTN_NONE;

  memset(this->macAddr, 0, sizeof(this->macAddr));
  // Get default MAC addr for station-class wifi
  esp_read_mac(this->macAddr, ESP_MAC_WIFI_STA);
}

SystemState::~SystemState()
{
}

void SystemState::updateWifiRSSI(int32_t rssi)
{
  this->rssi = rssi;
}

void SystemState::updateActiveThrottleCount(MRBusThrottle* throttles)
{
  uint32_t newActiveThrottles = 0;
  for(uint32_t thrNum=0; thrNum < MAX_THROTTLES; thrNum++)
    if (throttles[thrNum].isActive())
      newActiveThrottles++;

  this->activeThrottles = newActiveThrottles;
}

uint8_t SystemState::mrbusSrcAddrGet()
{
  return 0xD0 + this->baseAddress;
}

bool SystemState::configWriteDefault()//fs::FS &fs)
{
  FILE* f = fopen("/config/config.txt", "w+");

  if (!f)
    return false;

  Serial.printf("Writing configuration file\n");
  
  size_t bytesWritten = fwrite(defaultConfigFileContents, 1, strlen(defaultConfigFileContents), f);
  fclose(f);

  if (bytesWritten ==  strlen(defaultConfigFileContents))
    return true;

  return false;
}

bool SystemState::cmdStnDisconnect()
{
  if (DBGLVL_INFO)
    Serial.printf("[SYS]: cmdStnDisconnect()\n");

  if (this->isCmdStnConnected && NULL != this->cmdStn)
  {
      this->cmdStn->end();
      delete this->cmdStn;
  }
  this->isCmdStnConnected = false;
  this->cmdStn = NULL;
  // Handles the case where the cmd station might be still set from a previous instance
  // If we didn't set it through the configuration file, then just clear it out
  if (!this->cmdStnTypeSetByConfig)
    this->cmdStnType = CMDSTN_NONE;
  return true;
}

uint8_t debugWordToLevel(const char* debugStr)
{
  if (0 == strcmp(debugStr, "error"))
    return DBGLVL_ERR;
  else if (0 == strcmp(debugStr, "warn"))
    return DBGLVL_WARN;
  else if (0 == strcmp(debugStr, "info"))
    return DBGLVL_INFO;
  else if (0 == strcmp(debugStr, "debug"))
    return DBGLVL_DEBUG;

  // No valid string?  Turn it all on!
  return DBGLVL_DEBUG;
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

static void assignField(ConfigInstance &cfg, const std::string &key, const std::string &val)
{
  if (key == "ssid")
  {
    cfg.ssid = val;
    cfg.isUsed = true;
  }
  else if (key == "password")
  {
    cfg.password = val;
    cfg.isUsed = true;
  }
  else if (key == "mode")
  {
    cfg.cmdStnType = CMDSTN_NONE;
    if ("lnwi" == val)
      cfg.cmdStnType = CMDSTN_LNWI;
    else if ("dccex" == val)
      cfg.cmdStnType = CMDSTN_DCCEX;
    else if ("withrottle" == val)
      cfg.cmdStnType = CMDSTN_JMRI;
    else if ("esu" == val)
      cfg.cmdStnType = CMDSTN_ESU;

    cfg.isUsed = true;
  }
  else if (key == "serverPort")
  {
    cfg.serverPort = val.empty() ? 0 : MAX(0, MIN(65535, atoi(val.c_str())));
    cfg.isUsed = true;
  }
  else if (key == "serverIP")
  {
    cfg.serverIP.fromString((const char*)val.c_str());
    cfg.isUsed = true;
  }
  else if (key == "fastClockSource")
  {
    cfg.fcSource = (val == "cmdstn") ? FC_SOURCE_CMDSTN : FC_SOURCE_OFF;
    cfg.isUsed = true;
  }
  else if (key == "logLevel")
  {
    cfg.debugLvlMRBus = cfg.debugLvlCommandStation = cfg.debugLvlSystem = debugWordToLevel(val.c_str());    
    cfg.isUsed = true;
  }
  else if (key == "logLevelCommandStation")
  {
    cfg.debugLvlCommandStation = debugWordToLevel(val.c_str());    
    cfg.isUsed = true;
  }
  else if (key == "logLevelMRBus")
  {
    cfg.debugLvlMRBus = debugWordToLevel(val.c_str());    
    cfg.isUsed = true;
  }
  else if (key == "logLevelSystem")
  {
    cfg.debugLvlSystem = debugWordToLevel(val.c_str());    
    cfg.isUsed = true;
  }
  else if (key == "hostname")
  {
    cfg.hostname = val;
  }
}

// In-place string trimmer
static void trim(std::string &s)
{
  while (!s.empty() && isspace((unsigned char)s.front())) s.erase(s.begin());
  while (!s.empty() && isspace((unsigned char)s.back()))  s.pop_back();
}

struct RawConfigLine {
  std::string key;
  int index; // 1-9, or 0 for global/all
  std::string value;
};

// Parses a single line into key, index, and value.
// Returns false if the line is empty, a comment, or malformed.
static bool parseLine(const std::string &lineIn, RawConfigLine &out)
{
  std::string line = lineIn;
  trim(line);

  // Skip comments and blank lines
  if (line.empty() || line[0] == '#')
    return false;

  size_t eqPos = line.find('=');
  if (eqPos == std::string::npos)
    return false; // Malformed: missing '='

  std::string rawKey = line.substr(0, eqPos);
  std::string rawVal = line.substr(eqPos + 1);

  trim(rawKey);
  trim(rawVal); // Preserves empty value if nothing follows '='

  out.value = rawVal;
  out.index = 0; // Default: global

  // Check for bracketed index: key[1]
  size_t openBracket = rawKey.find('[');
  if (openBracket != std::string::npos)
  {
    size_t closeBracket = rawKey.find(']', openBracket);
    if (closeBracket != std::string::npos && closeBracket > openBracket + 1)
    {
      std::string idxStr = rawKey.substr(openBracket + 1, closeBracket - openBracket - 1);
      int idx = atoi(idxStr.c_str());

      if (idx >= 0 && idx <= MAX_CONFIG_INSTANCES)
      {
        out.index = idx;
        out.key = rawKey.substr(0, openBracket);
        trim(out.key);
        return true;
      }
      else
      {
        // Out-of-bounds index (must be 1-9)
        return false;
      }
    }
    return false; // Malformed bracket syntax
  }

  out.key = rawKey;
  return true;
}


bool SystemState::configRead()
{
  if (!this->isFSConnected)
    return false;

  FILE *f = fopen("/config/config.txt", "r");
  if (!f)
  {
    Serial.printf("[SYS]: config.txt failed to open\n");
    return false;
  }

  std::vector<RawConfigLine> parsedLines;
  int maxIndexSeen = 0;

  char buf[256];
  while (fgets(buf, sizeof(buf), f))
  {
    RawConfigLine entry;
    if (parseLine(buf, entry))
    {
      if (entry.index > maxIndexSeen)
        maxIndexSeen = entry.index;

      parsedLines.push_back(entry);
    }
  }
  fclose(f);

  // If no indexed keys were found, instantiate at least index 1
  int totalConfigs = (maxIndexSeen == 0) ? 1 : maxIndexSeen;

  this->configs.clear();
  this->configs.resize(totalConfigs);
  for (int i = 0; i < totalConfigs; ++i)
  {
    configs[i].index = i + 1;
  }

  // Pass 1: Apply global keys (index == 0) to all instances
  for (const auto &line : parsedLines)
  {
    if (line.index == 0)
    {
      for (auto &cfg : this->configs)
      {
        assignField(cfg, line.key, line.value);
      }
    }
  }

  // Pass 2: Apply specific index overrides (index 1-9 maps to vec[index - 1])
  for (const auto &line : parsedLines)
  {
    if (line.index >= 1 && line.index <= totalConfigs)
    {
      assignField(this->configs[line.index - 1], line.key, line.value);
    }
  }

  Serial.printf("[SYS]: %d configurations loaded\n", totalConfigs);

  for (int i = 0; i < totalConfigs; ++i)
  {
    if (!this->configs[i].isUsed)
      continue;

    Serial.printf("[SYS]:  Config %d - SSID[%s] / [%s]\n", this->configs[i], this->configs[i].ssid.c_str(), this->configs[i].password.c_str());
  }

  // Set the global (non-per-config) configuration values
  this->debugLvlMRBus = this->configs[0].debugLvlMRBus;
  this->debug = this->configs[0].debugLvlSystem;
  this->debugLvlCommandStation = this->configs[0].debugLvlCommandStation;
  return true;
}

const char* SystemState::getHostname()
{
  if (this->configs[0].hostname.empty())
  {
    char hostnameStr[32];
    snprintf(hostnameStr, sizeof(hostnameStr), "mrbw-wifi-%02x%02x", this->macAddr[4], this->macAddr[5]);
    this->configs[0].hostname = std::string(hostnameStr);
  }

  return this->configs[0].hostname.c_str();
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

bool mdnsQuery(const char* svcNameStr, IPAddress& ipAddr, uint16_t& port)
{
  Serial.printf("[SYS]: mDNS query for service [%s]\n", svcNameStr);

  ipAddr = (uint32_t)0;
  port = 0;

  mdns_result_t * results = NULL;
  mdns_result_t * r = NULL;

  esp_err_t err = mdns_query_ptr(svcNameStr, "_tcp", 3000, 20,  &results);
  int32_t numHosts = 0;
  const char* hostname = "";

  r = results;
  while (ESP_OK == err && NULL != r && 0 == numHosts)
  {
    mdns_ip_addr_t * a = NULL;
    a = r->addr;
    while (a)
    {
      if (a->addr.type == IPADDR_TYPE_V4)
      {
        numHosts++;
        hostname = r->hostname;
        ipAddr = (uint32_t)(a->addr.u_addr.ip4.addr);
        port = r->port;
        mdns_query_results_free(results);
        Serial.printf("[SYS]: mDNS found host [%s] address [%s:%d]\n", hostname, ipAddr.toString().c_str(), port);
        return true;
      }
      a = a->next;
    }
    r = r->next;
  }

  mdns_query_results_free(results);
  Serial.printf("[SYS]: mDNS found NO hosts for [%s]\n", svcNameStr);
  return false;
}

bool SystemState::cmdStnIPSetup()
{
  if (0 != (uint32_t)this->cmdStnIP && 0 != this->cmdStnPort)
  {
    return true;
  }

  IPAddress ipAddr = (uint32_t)0;
  uint16_t port = 0;

  // Try WiThrottle first for things we know are WiThrottle and for unknowns
  if (CMDSTN_LNWI == this->cmdStnType || CMDSTN_JMRI == this->cmdStnType || CMDSTN_DCCEX == this->cmdStnType || CMDSTN_NONE == this->cmdStnType)
  {
    if (mdnsQuery(WITHROTTLE_MDNS_NAME, ipAddr, port))
    {
      this->cmdStnIP = ipAddr;
      this->cmdStnPort = port;
      if (CMDSTN_NONE == this->cmdStnType)
        this->cmdStnType = CMDSTN_JMRI;
      return true;
    }
  }
  
  // Try ESU for things we know are ESU, or if we didn't find a configuration match above on an unknown
  if (CMDSTN_ESU == this->cmdStnType || CMDSTN_NONE == this->cmdStnType)
  { 
    if (mdnsQuery(ESU_MDNS_NAME, ipAddr, port))
    {
      this->cmdStnIP = ipAddr;
      this->cmdStnPort = port;
      this->cmdStnType = CMDSTN_ESU;
      return true;
    }
  }

  // As a last resort, try hard-coded rules about where certain devices (LNWIs, ESUs)
  //  live based on just how they're built.
  if (CMDSTN_LNWI == this->cmdStnType
    || (CMDSTN_DCCEX == this->cmdStnType && isDccExSSID(this->ssid))
    || (CMDSTN_ESU == this->cmdStnType && 0 == strcmp(this->ssid, "ESUWIFI")))
  {
    // LNWIs are always (?) on .1
    // Same for ESU CabControls if we're talking to its wifi network
    // DCC-EXs currently don't seem to do mDNS at all, but again .1
    // Take our IP, make it a.b.c.1 and return it
    
    this->cmdStnIP = (((uint32_t)this->localIP) & 0x00FFFFFF) | 0x01000000;
    if (0 == this->cmdStnPort)
    {
      if (CMDSTN_LNWI == this->cmdStnType)
        this->cmdStnPort = WITHROTTLE_PORT_DEFAULT;
      else if (CMDSTN_ESU == this->cmdStnType)
        this->cmdStnPort = ESU_PORT_DEFAULT;
      else if (CMDSTN_DCCEX == this->cmdStnType)
        this->cmdStnPort = DCCEX_PORT_DEFAULT;
    }

    Serial.printf("[SYS]: Trying backup plan of command station [%s:%d]\n", this->cmdStnIP.toString().c_str(), this->cmdStnPort);

    return true;
  }
  return false;
}

bool SystemState::registerConflictingBase()
{
  this->conflictingBase = true;
  this->conflictingBaseTimer.reset();
  return true;
}

bool SystemState::isConflictingBasePresent()
{
  if (this->conflictingBase && this->conflictingBaseTimer.test(false))
  {
    // We've timed out
    this->conflictingBase = false;
  }

  return this->conflictingBase;
}


bool SystemState::wifiScanStart()
{
  Serial.printf("[SYS]: Beginning Wifi Scan\n");
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.setMinSecurity(WIFI_AUTH_OPEN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  int16_t scanResult = WiFi.scanNetworks(true);
  if (scanResult == WIFI_SCAN_FAILED)
  {
    Serial.printf("[SYS]: Could not start Wifi Scan\n");
    return false;
  }

  return true;
}

bool SystemState::isWifiScanComplete()
{
  int16_t scanResult = WiFi.scanComplete();
  if (scanResult >= 0 || scanResult == WIFI_SCAN_FAILED)
    return true;
  return false;
}

bool SystemState::wifiScan()
{
  int totalNetworks = WiFi.scanComplete();
  if (totalNetworks == WIFI_SCAN_FAILED)
  {
    Serial.printf("[SYS]: WiFi scan failed\n");
    WiFi.scanDelete();
    return false;
  }

  if (totalNetworks >= 0)
    Serial.printf("[SYS]: WiFi scan done, %d networks found\n", totalNetworks);
  
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
    Serial.printf("  [%-32.32s] Ch: %2ld RSSI: %ld Auth: %s\n", ssid.c_str(), channel, rssi, wifiSecurityTypeStringGet((wifi_auth_mode_t)auth));
  }

  // Initialize wifi fields
  memset(this->ssid, 0, sizeof(this->ssid));
  memset(this->password, 0, sizeof(this->password));
  this->cmdStnPort = 0;
  this->cmdStnIP.fromString("0.0.0.0");
  this->activeConfigNum = -1;
  uint8_t configNum = 0;

  for (const auto& config : this->configs) 
  {
    bool match = false;
    configNum++;

    if (IS_DBGLVL_INFO)
      Serial.printf("[SYS]: Config [%u] ", configNum);
      
    if (!config.isUsed)
    {
      if (IS_DBGLVL_INFO)
        Serial.printf("unused\n");
      continue;
    }

    if (IS_DBGLVL_INFO)
      Serial.printf("ssid [%s]/[%s] type [%d]\n", config.ssid.c_str(), config.password.c_str(), config.cmdStnType);

    for (int n=0; n<totalNetworks; n++)
    {
      String ssid;
      uint8_t auth;
      int32_t rssi;
      uint8_t* bssid;
      int32_t channel;

      WiFi.getNetworkInfo(n, ssid, auth, rssi, bssid, channel);
      // Let's look through these networks and see if we find anything meeting our needs

      if (IS_DBGLVL_INFO)
        Serial.printf("[SYS]: Considering network [%s]\n", ssid.c_str());

      if (!config.ssid.empty() && 0 == strcmp(config.ssid.c_str(), ssid.c_str())
        && (auth == WIFI_AUTH_OPEN || !config.password.empty()))
      {
        // This is a good candidate
        strncpy(this->ssid, config.ssid.c_str(), sizeof(this->ssid));
        strncpy(this->password, config.password.c_str(), sizeof(this->password));
        this->cmdStnType = config.cmdStnType;
        this->cmdStnPort = config.serverPort;
        this->cmdStnIP = config.serverIP;
        this->fcSource = config.fcSource;
        match = true;
        break;
      }
      else if (config.ssid.empty() && config.password.empty())
      {
        if ((config.cmdStnType == CMDSTN_NONE || config.cmdStnType == CMDSTN_ESU) && ssid == "ESUWIFI")
        {
          // It's an ESU
          this->cmdStnType = CMDSTN_ESU;
          strncpy(this->ssid, ssid.c_str(), sizeof(this->ssid));
          if (auth != WIFI_AUTH_OPEN)
            strncpy(this->password, "123456789", sizeof(this->password));
          this->cmdStnPort = (config.serverPort != 0)?config.serverPort:ESU_PORT_DEFAULT;
          this->fcSource = config.fcSource;
          match = true;
          break;
        }
        else if ((config.cmdStnType == CMDSTN_NONE || config.cmdStnType == CMDSTN_LNWI) && (auth == WIFI_AUTH_OPEN) && isDigitraxSSID(ssid.c_str()))
        {
          this->cmdStnType = CMDSTN_LNWI;
          strncpy(this->ssid, ssid.c_str(), sizeof(this->ssid));
          this->cmdStnPort = WITHROTTLE_PORT_DEFAULT;
          this->fcSource = config.fcSource;
          match = true;
          break;
        }
        else if ((config.cmdStnType == CMDSTN_NONE || config.cmdStnType == CMDSTN_JMRI) && (auth == WIFI_AUTH_OPEN) && ssid == "MRCWi-Fi")
        {
          this->cmdStnType = CMDSTN_JMRI;
          strncpy(this->ssid, ssid.c_str(), sizeof(this->ssid));
          this->cmdStnPort = WITHROTTLE_PORT_DEFAULT;
          this->fcSource = config.fcSource;
          match = true;
          break;

        }
        else if ((config.cmdStnType == CMDSTN_NONE || config.cmdStnType == CMDSTN_JMRI) && ssid == "RPi-JMRI")
        {
          // Auto-configuration for Steve Todd's JMRI RasPi Image
          this->cmdStnType = CMDSTN_JMRI;
          strncpy(this->ssid, ssid.c_str(), sizeof(this->ssid));
          if (auth != WIFI_AUTH_OPEN)
            strncpy(this->password, "rpI-jmri", sizeof(this->password));

          if (config.serverPort != 0)
            this->cmdStnPort = config.serverPort;

          this->fcSource = config.fcSource;
          match = true;
          break;

        }
        else if ((config.cmdStnType == CMDSTN_NONE || config.cmdStnType == CMDSTN_DCCEX) && isDccExSSID(ssid.c_str()))
        {
          // Auto-configuration for DCC-EX Command Stations
          this->cmdStnType = CMDSTN_DCCEX;
          strncpy(this->ssid, ssid.c_str(), sizeof(this->ssid));
          if (auth != WIFI_AUTH_OPEN)
            snprintf(this->password, sizeof(this->password), "PASS_%s", ssid.c_str()+6);

          if (config.serverPort != 0)
            this->cmdStnPort = config.serverPort;

          this->fcSource = config.fcSource;
          match = true;
          break;
        }
      }
    }

    if (match)
    {
      this->activeConfigNum = configNum;
      break;
    }

  }

  WiFi.scanDelete();

  if (strlen(this->ssid))
  {
    if (IS_DBGLVL_INFO)
      Serial.printf("[SYS]: Found network [%s]/[%s] Port [%d] matches config [%u], type[%d]\n", this->ssid, this->password, this->cmdStnPort, this->activeConfigNum, this->cmdStnType);
    return true;
  }

  return false;
}
