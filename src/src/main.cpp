#include <Arduino.h>
#include <Wire.h>
#include "WiFi.h"
#include "FFat.h"
#include "esp_task_wdt.h"

SET_LOOP_TASK_STACK_SIZE(62 * 1024);

// Include drivers for MRBW-WIFI hardware
#include "commonFuncs.h"
#include "versions.h"
#include "mrbus.h"
#include "systemState.h"
#include "switches.h"
#include "msc.h"
#include "display.h"
#include "ws2812.h"
#include "periodicEvent.h"
#include "MRBusThrottle.h"
#include "WiThrottle.h"
#include "ESUCabControl.h"
#include "Clock.h"
#include "esp32s2/rom/rtc.h"
#include <ESPmDNS.h>

#define PIN_SDA  33
#define PIN_SCL  34

ISE_MSC msc;
I2CDisplay display;
Switches switches;
SystemState systemState;
MRBus mrbus;
MRBusThrottle throttles[MAX_THROTTLES];

typedef enum 
{
  STATE_STARTUP               = 0x00,
  STATE_DRAW_SPLASH_SCREEN    = 0x10,
  STATE_WAIT_SPLASH_SCREEN    = 0x11,
  STATE_MAIN_LOOP             = 0x80
} loopState_t;

loopState_t mainLoopState = STATE_STARTUP;
PeriodicEvent tmrStatusScreenUpdate;
PeriodicEvent tmrSplashScreen;

void setup() 
{
  Serial.begin();
  systemState.resetReason = rtc_get_reset_reason(0);
  switches.setup();
  Wire.setPins(PIN_SDA, PIN_SCL);
  Wire.setClock(400000UL);
  Wire.begin();
  mrbus.begin();

  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch
  esp_task_wdt_reset();
  ws2812Set(0x0f0000);

  // Try to mount the FFat partition, format if it can't find it
  systemState.isFSConnected = FFat.begin(true);

  // See if we have a config file.  If we don't, just blow away the partition
  if (systemState.isFSConnected)
  {
    File f = FFat.open(CONFIG_FILE_PATH);
    if (!f || f.isDirectory())
    {
      // Shut it down, reformat, start over - nuclear approach!
      FFat.end();
      FFat.format(true);
      systemState.isFSConnected = FFat.begin(true);
      if (systemState.isFSConnected)
        systemState.configWriteDefault(FFat);
    }
  }

  // Read configuration from FAT partition before 
  //  we open it up to allow the host computer to write to it
  systemState.configRead(FFat);

  mrbus.debugLevelSet(systemState.debugLvlMRBus);

  // Initialize the throttle array - needs to be after reading configuration
  for(uint32_t thrNum=0; thrNum<MAX_THROTTLES; thrNum++)
  {
    throttles[thrNum].initialize(MRBUS_THROTTLE_BASE_ADDR + thrNum, systemState.debugLvlMRBus);
  }

  // Start mass storage class driver.  From here on out, we should be read-only on
  // the mrbw-wifi side
  msc.start();

  // Initialize wifi radio and disconnect from anything we might be connected to
  //  as well as set some basic stuff - hostname, always sort by signal strength, etc.
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(systemState.hostname);
  WiFi.disconnect();
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);

  esp_task_wdt_reset();
  // Start the display
  display.setup(&Wire, 0x3C);

  esp_task_wdt_reset();
  tmrStatusScreenUpdate.setup(1000); // Status screen updates every 1s
}

void drawSplashScreen(SystemState& state)
{
  char buffer[32];
  display.clrscr();
  display.putstr("Iowa Scaled", 0, 0);
  display.putstr("Engineering", 0, 1);
  display.putstr("MRBW-WIFI  ", 0, 2);
  snprintf(buffer, sizeof(buffer), "%d.%d.%d %6.6s", MAJOR_VERSION, MINOR_VERSION, DELTA_VERSION, GIT_REV);
  display.putstr(buffer, 0, 3);
  
  snprintf(buffer, sizeof(buffer)-1, "%02X%02X", state.macAddr[4], state.macAddr[5]);
  display.putstr(buffer, 15, 4);
  
  display.drawISELogo();  
  display.refresh();
}

#define TIME_FLAGS_DISP_FAST       0x01
#define TIME_FLAGS_DISP_FAST_HOLD  0x02
#define TIME_FLAGS_DISP_REAL_AMPM  0x04
#define TIME_FLAGS_DISP_FAST_AMPM  0x08

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
  return true;
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

void drawStatusScreen(SystemState& state)
{
  char lineBuf[24];
  char lpsText[16];
  const uint8_t spinnerChars[] = { '-', '\\', '|', '/' };
  static uint8_t spinnerNum = 0;
//     000000000011111111112
//     012345678901234567890
//  0:[a:LNWI    s T:nn B:aa] a=A/auto, C/config file (LNWI/ESU /WTHR/NONE) s=spinner nn=throttles connected aa=base addr  
//  1:[SSID-NAME-HERE-HERE-H] (scrolling)
//  2:[192.168.255.255:ppppp]
//  3:[R:-sss cc xxC xxxxlps] s=rssi, cc=channel

  // Update spinner
  spinnerNum = (spinnerNum + 1) % sizeof(spinnerChars);

  display.clrscr(false);

  memset(lineBuf, 0, sizeof(lineBuf));
  const char* cmdStnStr = "UNKN";
  switch(state.cmdStnType)
  {
    case CMDSTN_NONE:
      cmdStnStr = "NONE";
      break;
    case CMDSTN_LNWI:
      cmdStnStr = "LNWI";
      break;
    case CMDSTN_JMRI:
      cmdStnStr = "WTHR";
      break;
    case CMDSTN_ESU:
      cmdStnStr = "ESU ";
      break;
    default:
      cmdStnStr = "UNKN";
      break;
  }

  snprintf(lineBuf, sizeof(lineBuf), "%c:%4.4s    %c T:%02d %c:%02d", state.isAutoNetwork?'A':'C', cmdStnStr, spinnerChars[spinnerNum], state.activeThrottles, state.isConflictingBasePresent()?'*':'B', state.baseAddress);
  display.putstr(lineBuf, 0, 0);

  if (0 == strlen(state.ssid))
  {
    snprintf(lineBuf, sizeof(lineBuf), "%-21.21s", "(Searching...)");
  } else {
    snprintf(lineBuf, sizeof(lineBuf), "%-21.21s", state.ssid);
  }
  display.putstr(lineBuf, 0, 1);

  if (state.isWifiConnected)
  {
    char ipBuffer[64];
    switch(state.ipDisplayLine)
    {
      case DISPLAY_IP_LOCAL:  // Two phases for 
      case DISPLAY_IP_LOCAL2:
        snprintf(lineBuf, sizeof(lineBuf), "L:%-19.19s", state.localIP.toString().c_str());
        break;
      case DISPLAY_IP_CMDSTN:
      case DISPLAY_IP_CMDSTN2:
        snprintf(ipBuffer, sizeof(ipBuffer), "%s:%d", state.cmdStnIP.toString().c_str(), state.cmdStnPort);
        snprintf(lineBuf, sizeof(lineBuf), "C:%-19.19s", ipBuffer);
        break;
      default:
        break;
    }
    state.ipDisplayLine = (IPLineDisplay)((state.ipDisplayLine + 1) % DISPLAY_IP_MAX_FIELDS);

  } else {
    snprintf(lineBuf, sizeof(lineBuf), "%-21.21s", "(No Network)");
  }
  display.putstr(lineBuf, 0, 2);

  uint32_t lps = state.loopCnt;
  state.loopCnt = 0;

  if (lps < 10000)
    snprintf(lpsText, sizeof(lpsText), "%04dl/s", lps);
  else if (lps >= 10000)
    snprintf(lpsText, sizeof(lpsText), "%03dkl/s", lps / 1000);
  else if (lps >= 100000)
    snprintf(lpsText, sizeof(lpsText), "%03dMl/s", lps / 1000000);

  snprintf(lineBuf, sizeof(lineBuf), "R:%02ddB  40C %7.7s", state.rssi, lpsText);
  display.putstr(lineBuf, 0, 3);

  display.refresh();
}

void loop() 
{
  systemState.loopCnt++;

  switch(mainLoopState)
  {
    case STATE_STARTUP:

    case STATE_DRAW_SPLASH_SCREEN:
      tmrSplashScreen.setup(2000);
      drawSplashScreen(systemState);
      tmrSplashScreen.reset();
      mainLoopState = STATE_WAIT_SPLASH_SCREEN;
      break;

    case STATE_WAIT_SPLASH_SCREEN:
      if (tmrSplashScreen.test(false))
      {
        mainLoopState = STATE_MAIN_LOOP;

        // Print a bunch of diagnostic header info to the serial console
        Serial.printf("[SYS]: Iowa Scaled Engineering\n");
        Serial.printf("[SYS]: MRBW-WIFI\n");
        Serial.printf("[SYS]: IDF Ver:  [%s]\n", esp_get_idf_version());
        Serial.printf("[SYS]: MAC Addr: [%02X:%02X:%02X:%02X:%02X:%02X]\n", 
          systemState.macAddr[0], systemState.macAddr[1], systemState.macAddr[2],
          systemState.macAddr[3], systemState.macAddr[4], systemState.macAddr[5]);

        esp_chip_info_t chip_info;
        esp_chip_info(&chip_info);
        Serial.printf("[SYS]: ESP32S2 rev %d \n", chip_info.revision);
        Serial.printf("[SYS]: Reset Reason: [%s]\n", systemState.resetReasonStringGet());
      }
      break;

    case STATE_MAIN_LOOP:
      if (tmrStatusScreenUpdate.test(true))
      {
        uint32_t color = WS2812_RED;
        esp_task_wdt_reset();

        // Will only send time if we have a fast time source
        sendMRBusTimePacket(systemState, mrbus);

        Serial.printf("[SYS]: TICK %05us stack: %d heap:%d Wifi=%c CmdStn=%c\n", (uint32_t)(esp_timer_get_time() / 1000000), uxTaskGetStackHighWaterMark(NULL), xPortGetFreeHeapSize(), systemState.isWifiConnected?'Y':'N', systemState.isCmdStnConnected?'Y':'N');

        systemState.baseAddress = switches.baseAddressGet();
        mrbus.setAddress(systemState.baseAddress + 0xD0); // MRBus address for the base is switches + 0xD0 offset
        systemState.rssi = WiFi.RSSI();

        systemState.activeThrottles = 0;
        for(uint32_t thrNum=0; thrNum < MAX_THROTTLES; thrNum++)
          if (throttles[thrNum].isActive())
            systemState.activeThrottles++;

        drawStatusScreen(systemState);

        if (systemState.isWifiConnected && systemState.isCmdStnConnected)
        {
          // Total abuse of an unsuspecting, innocent variable to get a 1 second phase clock
          if (systemState.isConflictingBasePresent() && systemState.ipDisplayLine & 0x01)
            color = WS2812_CYAN;
          else
            color = WS2812_GREEN;

        }
        else if (systemState.isWifiConnected)
          color = WS2812_YELLOW;
          
        ws2812Set(color);
        // Send our version and status out every second
        sendMRBusVersionPacket(systemState, mrbus);
      }
      mrbus.processSerial();

      if (!systemState.isWifiConnected || !systemState.isCmdStnConnected)
      {
        mrbus.rxPktQueue->flush();
      }

      // Check wifi network - if we don't have it, try to get it
      if (!systemState.isWifiConnected)
      {
        bool networkFound = systemState.wifiScan();
        systemState.localIP.fromString("0.0.0.0");
        systemState.isCmdStnConnected = false;

        if (networkFound) // Can either be because search found one or not autconfig
        {
          // Try to connect to the network we found
          Serial.printf("[SYS]: Starting connection to [%s] [%s]\n", systemState.ssid, systemState.password);
          WiFi.begin(systemState.ssid, systemState.password);

          // FIXME - put timeout here
          while(WiFi.status() != WL_CONNECTED)
          {
            delay(100);
          }
          Serial.printf("[SYS]: Connected to wifi network [%s]\n", systemState.ssid);
          WiFi.setAutoReconnect(true);
        }

        // Did we connect?  If so, change our indication to wifi connected and go back around the loop
        if(WiFi.status() == WL_CONNECTED)
        {
          systemState.localIP = WiFi.localIP();
          systemState.isWifiConnected = true;
          systemState.isCmdStnConnected = false;
          MDNS.begin(systemState.hostname);
          return;
        }
      }
      else if (WiFi.status() != WL_CONNECTED)  // This implies that systemState.isWifiConnected is true
      {
        systemState.localIP.fromString("0.0.0.0");
        systemState.isWifiConnected = false;
        MDNS.end();
        systemState.cmdStnDisconnect();
      }

      if (!systemState.isWifiConnected || tmrStatusScreenUpdate.test(false))
        return; // Out we go - can't do squat without a wifi connection, or maybe we need to do a screen refresh

      if (systemState.isCmdStnConnected)
      {
        // Just make sure we're really still connected
        if (!systemState.cmdStnConnection.connected())
        {
          Serial.printf("[SYS]: Stopping command station socket connection\n\n");
          systemState.cmdStnConnection.stop();

          if (NULL != systemState.cmdStn)
          {
            Serial.printf("[SYS]: Deleting disconnected command station\n\n");
            systemState.cmdStnDisconnect();
          }
        }
      }

      if (!systemState.isCmdStnConnected)
      {
        // Try to build a connection
        if (!systemState.cmdStnIPSetup())  // Do all the logic about merging configuration with auto-discovery
          return;  // No IP found for a command station, on with life - return from loop() and start over
        
        Serial.printf("[SYS]: Trying to connect %s:%d\n", systemState.cmdStnIP.toString().c_str(), systemState.cmdStnPort);
        bool connectSuccessful = systemState.cmdStnConnection.connect(systemState.cmdStnIP, systemState.cmdStnPort, 1000);
        Serial.printf("[SYS]: Connect successful = %c\n", connectSuccessful?'Y':'N');

        if (connectSuccessful)
        {
          systemState.isCmdStnConnected = true;
          // Nuke any throttles that may be left over from last time.

          char macBuffer[16];
          snprintf(macBuffer, sizeof(macBuffer)-1, "%02X%02X", systemState.macAddr[4], systemState.macAddr[5]);

          // Create command station object
          switch(systemState.cmdStnType)
          {
            case CMDSTN_JMRI:
              systemState.cmdStn = new WiThrottle(macBuffer);
              systemState.cmdStn->begin(systemState.cmdStnConnection, 0, systemState.debugLvlCommandStation);
              if (FC_SOURCE_CMDSTN == systemState.fcSource)
              {
                if (systemState.cmdStn->fastClockConnect(&systemState.fastClock))
                {
                  systemState.fastClock.enable();
                } else {
                  systemState.fastClock.disable();
                }
              }

              break;
            case CMDSTN_LNWI:
              systemState.cmdStn = new WiThrottle(macBuffer);
              systemState.cmdStn->begin(systemState.cmdStnConnection, WITHROTTLE_QUIRK_LNWI, systemState.debugLvlCommandStation);
              if (FC_SOURCE_CMDSTN == systemState.fcSource)
              {
                if (systemState.cmdStn->fastClockConnect(&systemState.fastClock))
                {
                  systemState.fastClock.enable();
                } else {
                  systemState.fastClock.disable();
                }
              }
              break;
            case CMDSTN_ESU:
              systemState.cmdStn = new ESUCabControl;
              systemState.cmdStn->begin(systemState.cmdStnConnection, 0, systemState.debugLvlCommandStation);
              break;
            default: // Do nothing, don't know what it is
              break;
          }
        }
      }

      if (!systemState.isCmdStnConnected || tmrStatusScreenUpdate.test(false))
        return;

      systemState.cmdStn->update();

      // If we're here, we should have a command station connected
      while(!mrbus.rxPktQueue->isEmpty())
      {
        MRBusPacket pkt;
        mrbus.rxPktQueue->pop(pkt);
        /*Serial.printf("Pkt [%02x->%02x (%d bytes) ", pkt.src, pkt.dest, pkt.len);
        for(uint32_t k = 0; k<pkt.len; k++)
          Serial.printf("%02x ", pkt.data[k]);
        Serial.printf("]\n");*/

        if (pkt.src == systemState.mrbusSrcAddrGet())
        {
          // Ouch, conflicting base station detected
          systemState.registerConflictingBase();
          continue;
        }

        if (pkt.dest == systemState.mrbusSrcAddrGet() && pkt.data[0] == 'S' && pkt.len == 15
          && (pkt.src >= MRBUS_THROTTLE_BASE_ADDR && pkt.src < MRBUS_THROTTLE_BASE_ADDR + MAX_THROTTLES))
        {
//          Serial.printf("Throttle pkt [%02x->%02x  %02d]\n", pkt.src, pkt.dest, pkt.len);
          uint8_t throttleNum = pkt.src - MRBUS_THROTTLE_BASE_ADDR;
          throttles[throttleNum].update(systemState.cmdStn, pkt);
        }
      }

      // Check for dead throttles and remove them
      for(uint32_t thrNum=0; thrNum < MAX_THROTTLES; thrNum++)
      {
        if (throttles[thrNum].isActive() && throttles[thrNum].isExpired(THROTTLE_TIMEOUT_SECONDS))
        {
          throttles[thrNum].disconnect(systemState.cmdStn);
        }
      }
      break;

    default:
      break;

  }
}