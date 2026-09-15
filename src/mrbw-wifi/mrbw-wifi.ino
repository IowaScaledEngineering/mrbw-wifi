#include <Arduino.h>
#include <Wire.h>
#include "WiFi.h"
//#include "FFat.h"
#include "esp_task_wdt.h"
#include "esp_chip_info.h"
SET_LOOP_TASK_STACK_SIZE(62 * 1024);

// Include drivers for MRBW-WIFI hardware
#include "commonFuncs.h"
#include "versions.h"
#include "mrbus.h"
#include "SystemState.h"
#include "switches.h"
#include "msc.h"
#include "display.h"
#include "ws2812.h"
#include "periodicEvent.h"
#include "MRBusThrottle.h"
#include "WiThrottle.h"
#include "ESUCabControl.h"
#include "Clock.h"
#include "screens.h"
#include "mrbusPackets.h"
#include "esp32s2/rom/rtc.h"
#include "configuration.h"
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
  STATE_RUNNING               = 0x20,   
} MainState_t;

typedef enum
{
  NETSTATE_INIT_WIFI,
  NETSTATE_DISCONNECT_WIFI,
  NETSTATE_SEARCH_WIFI_START,
  NETSTATE_SEARCH_WIFI_WAIT,
  NETSTATE_CONNECT_WIFI,
  NETSTATE_WAIT_WIFI,
  NETSTATE_FIND_SERVER,
  NETSTATE_FIND_SERVER_WAIT,
  NETSTATE_CONNECT_SERVER,
  NETSTATE_CONNECT_SERVER_WAIT,
  NETSTATE_CONNECTED,
  NETSTATE_DISCONNECT_SERVER,
  NETSTATE_CONNECTION_HICCUP_WAIT
} NetworkState_t;


MainState_t mainLoopState = STATE_STARTUP;
NetworkState_t netState = NETSTATE_INIT_WIFI;
PeriodicEvent timerScreenUpdate;
PeriodicEvent timerOneSecondTasks;
PeriodicEvent timerNetworkSearch;
uint32_t retryCounter = 0;



void setup() 
{
  Serial.begin(115200);
  systemState.resetReason = rtc_get_reset_reason(0);

  switches.setup();
  ws2812Init();
  Serial.setDebugOutput(true);

  if (psramInit() == false) {
    Serial.println("PSRAM init failed!");
  } else if (psramAddToHeap() == false) {
    Serial.println("PSRAM could not be added to the heap!");
  } else {
    Serial.println("PSRAM added to the heap.");
  }

  // Initialize the I2C bus for the display at 400kHz
  Wire.setPins(PIN_SDA, PIN_SCL);
  Wire.setClock(400000UL);
  Wire.begin();

  ws2812Set(WS2812_RED);
  fsSetupAndConfig(systemState, switches);
  ws2812Set(WS2812_RED);

  // Initialize MRBus
  mrbus.begin();
  mrbus.debugLevelSet(systemState.debugLvlMRBus);

  // Watchdog setup
  esp_task_wdt_config_t twdt_config = 
  {
    .timeout_ms = WDT_TIMEOUT * 1000,
    .idle_core_mask = 1,    // Bitmask of cores
    .trigger_panic = true,
  };

  esp_task_wdt_init(&twdt_config); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch
  esp_task_wdt_reset();

  // Initialize the throttle array - needs to be after reading configuration
  for(uint32_t thrNum=0; thrNum<MAX_THROTTLES; thrNum++)
  {
    throttles[thrNum].initialize(MRBUS_THROTTLE_BASE_ADDR + thrNum, systemState.debugLvlMRBus);
  }

  // Start mass storage class driver.  From here on out, we shouldn't try to do anything
  //  with the fat partition from the ESP side for risk of corruption if the PC writes it
  msc.start();

  updateBaseAddressFromSwitches(systemState, switches, mrbus);

  // Initialize wifi radio and disconnect from anything we might be connected to
  //  as well as set some basic stuff - hostname, always sort by signal strength, etc.
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(systemState.getHostname());
  WiFi.disconnect();
  WiFi.setMinSecurity(WIFI_AUTH_OPEN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);

  esp_task_wdt_reset();

  // Start the display
  display.setup(&Wire, 0x3C);

  timerOneSecondTasks.setup(1000);
  esp_task_wdt_reset();
}

uint32_t color = WS2812_OFF;

void loop() 
{
  systemState.loopCnt++;

  // Slap the watchdog timer to keep it from resetting us
  esp_task_wdt_reset();
  // Update the status LED every time around
  //   It'll only write if something changed
  ws2812Update(color);

  switch(mainLoopState)
  {
    default:
    case STATE_STARTUP:
      timerOneSecondTasks.reset();
    case STATE_DRAW_SPLASH_SCREEN:
      timerScreenUpdate.setup(2000); // Display splash screen for 2 seconds
      drawSplashScreen(systemState, display);
      timerScreenUpdate.reset();
      mainLoopState = STATE_WAIT_SPLASH_SCREEN;
      break;

    case STATE_WAIT_SPLASH_SCREEN:
      if (timerScreenUpdate.test(false))
      {
        timerScreenUpdate.setup(1000); // Display status screen for 1 second
        timerScreenUpdate.reset();
        mainLoopState = STATE_RUNNING;

        // Print a bunch of diagnostic header info to the serial console
        Serial.printf("[SYS]: Iowa Scaled Engineering\n");
        Serial.printf("[SYS]: MRBW-WIFI\n");
        Serial.printf("[SYS]: IDF Ver:  [%s]\n", esp_get_idf_version());
        Serial.printf("[SYS]: ESP Arduino Ver: [%s]", ESP_ARDUINO_VERSION_STR);
        Serial.printf("[SYS]: MAC Addr: [%02X:%02X:%02X:%02X:%02X:%02X]\n", 
          systemState.macAddr[0], systemState.macAddr[1], systemState.macAddr[2],
          systemState.macAddr[3], systemState.macAddr[4], systemState.macAddr[5]);

        esp_chip_info_t chip_info;
        esp_chip_info(&chip_info);
        Serial.printf("[SYS]: ESP32S2 rev %d \n", chip_info.revision);
        Serial.printf("[SYS]: Reset Reason: [%s]\n", systemState.resetReasonStringGet());
      }
      break;

    case STATE_RUNNING:
      if (timerOneSecondTasks.test(true))
      {
        // Do our once-a-second periodic stuff
        updateBaseAddressFromSwitches(systemState, switches, mrbus);
        sendMRBusTimePacket(systemState, mrbus);
        sendMRBusVersionPacket(systemState, mrbus);
        for(uint32_t thrNum=0; netState == NETSTATE_CONNECTED && systemState.cmdStn != NULL && thrNum < MAX_THROTTLES; thrNum++)
        {
          if (throttles[thrNum].isActive() && throttles[thrNum].isExpired(THROTTLE_TIMEOUT_SECONDS))
            throttles[thrNum].disconnect(systemState.cmdStn);
        }
        if (SYS_DBGLVL_DEBUG)
          Serial.printf("[SYS]: TICK %05lus stack: %u heap:%d Wifi=%c CmdStn=%c\n", (uint32_t)(esp_timer_get_time() / 1000000), uxTaskGetStackHighWaterMark(NULL), xPortGetFreeHeapSize(), systemState.isWifiConnected?'Y':'N', systemState.isCmdStnConnected?'Y':'N');
      }

      if (timerScreenUpdate.test(true))
      {
        systemState.updateActiveThrottleCount(throttles);
        systemState.updateWifiRSSI(WiFi.RSSI());
        drawStatusScreen(systemState, display);
      }

      // Process any MRBus packets that may have come in
      mrbus.processSerial();

      switch(netState)
      {
        // These cases are where something has gone wrong and we need to disconnect and reset the wifi layer
        default:
        case NETSTATE_DISCONNECT_WIFI:
          Serial.printf("[SYS]: WiFi Disconnected, Resetting\n");
          // Put up wifi disconnected screen
          color = WS2812_RED;
          MDNS.end();
          systemState.localIP.fromString("0.0.0.0");
          systemState.cmdStnDisconnect();
          systemState.isWifiConnected = false;
          systemState.isCmdStnConnected = false;
          if (NULL != systemState.cmdStn)
          {
            systemState.cmdStn->end();
            delete systemState.cmdStn;
          }
          WiFi.disconnect();
          WiFi.setAutoReconnect(false);
          netState = NETSTATE_INIT_WIFI;
          // Intentional fall-through

        case NETSTATE_INIT_WIFI:
          // In this state, we're setting up our wifi from a completely disconnected state
          color = WS2812_RED;
          systemState.isWifiConnected = false;
          systemState.isCmdStnConnected = false;
          WiFi.disconnect();
          WiFi.mode(WIFI_STA);
          WiFi.setHostname(systemState.getHostname());
          netState = NETSTATE_SEARCH_WIFI_START;
          break;

        case NETSTATE_SEARCH_WIFI_START:
          color = WS2812_RED;
          systemState.isWifiConnected = false;
          systemState.isCmdStnConnected = false;
          if (!systemState.wifiScanStart())
            netState = NETSTATE_INIT_WIFI;
          netState = NETSTATE_SEARCH_WIFI_WAIT;
          break;

        case NETSTATE_SEARCH_WIFI_WAIT:
          color = WS2812_RED;
          systemState.isWifiConnected = false;
          systemState.isCmdStnConnected = false;
          if (systemState.isWifiScanComplete())
          {
            Serial.printf("[SYS]: WiFi Scan Complete\n");
            netState = NETSTATE_CONNECT_WIFI;
          }
          break;
        
        case NETSTATE_CONNECT_WIFI:
          color = WS2812_RED;
          systemState.isWifiConnected = false;
          systemState.isCmdStnConnected = false;
          // See if any of our wifi networks match up with any of our configurations
          if (!systemState.wifiScan())
          {
            Serial.printf("[SYS]: No viable WiFi network found - rescanning\n");
            netState = NETSTATE_SEARCH_WIFI_START;
            break;
          }

          Serial.printf("[SYS]: Starting connection to [%s] [%s]\n", systemState.ssid, systemState.password);
          WiFi.disconnect();
          if (0 == strlen(systemState.password))
            WiFi.setMinSecurity(WIFI_AUTH_OPEN);
          else
            WiFi.setMinSecurity(WIFI_AUTH_WPA2_PSK);
          WiFi.begin(systemState.ssid, systemState.password);
          timerNetworkSearch.setup(20000);
          netState = NETSTATE_WAIT_WIFI;
          break;

        case NETSTATE_WAIT_WIFI:
          color = WS2812_RED;
          systemState.isWifiConnected = false;
          systemState.isCmdStnConnected = false;
          if (WiFi.status() == WL_CONNECTED)
          {
            Serial.printf("[SYS]: Successful connection to [%s]\n", systemState.ssid);
            systemState.isWifiConnected = true;
            systemState.isCmdStnConnected = false;
            WiFi.setAutoReconnect(true);
            systemState.localIP = WiFi.localIP();
            Serial.printf("[SYS]: Receiver IP is [%s]\n", systemState.localIP.toString());
            Serial.printf("[SYS]: Starting MDNS process with hostname [%s]\n", systemState.getHostname());
            MDNS.begin(systemState.getHostname());
            retryCounter = 0;
            netState = NETSTATE_FIND_SERVER;
          }
          else if (timerNetworkSearch.test(false))
          {
            // If our timer elapses, go back to searching
            systemState.isWifiConnected = false;
            systemState.isCmdStnConnected = false;
            Serial.printf("[SYS]: Connection failed to WiFi network [%s]\n", systemState.ssid);
            netState = NETSTATE_DISCONNECT_WIFI;
          }
          break;

        case NETSTATE_FIND_SERVER:
          color = WS2812_YELLOW;
          systemState.isWifiConnected = true;
          systemState.isCmdStnConnected = false;
          if (WiFi.status() != WL_CONNECTED)
          {
            Serial.printf("[SYS]: WiFi network disconnected, restarting\n");
            netState = NETSTATE_DISCONNECT_WIFI;
            break;
          }
          if (!systemState.cmdStnIPSetup())  // Do all the logic about merging configuration with auto-discovery
          {
            Serial.printf("[SYS]: Could not find server, waiting\n");
            timerNetworkSearch.setup(2000);
            netState = NETSTATE_FIND_SERVER_WAIT;
            break;
          }
          retryCounter = 0;
          netState = NETSTATE_CONNECT_SERVER;
          break;

        case NETSTATE_FIND_SERVER_WAIT:
          color = WS2812_YELLOW;
          systemState.isWifiConnected = true;
          systemState.isCmdStnConnected = false;
          if (timerNetworkSearch.test(true))
          {
            if (retryCounter++ >= 5)
              netState = NETSTATE_DISCONNECT_WIFI;
            else
              netState = NETSTATE_FIND_SERVER;
          }
          break;

        case NETSTATE_CONNECT_SERVER_WAIT:
          color = WS2812_YELLOW;
          systemState.isWifiConnected = true;
          systemState.isCmdStnConnected = false;
          if (timerNetworkSearch.test(true))
          {
            if (retryCounter++ >= 5)
              netState = NETSTATE_DISCONNECT_WIFI;
            else
              netState = NETSTATE_CONNECT_SERVER;
          }
          break;

        case NETSTATE_CONNECT_SERVER:
          {
            color = WS2812_YELLOW;
            systemState.cmdStnConnection.stop();
            systemState.isWifiConnected = true;
            systemState.isCmdStnConnected = false;
            Serial.printf("[SYS]: Trying to connect %s:%d, retry=%d\n", systemState.cmdStnIP.toString().c_str(), systemState.cmdStnPort, retryCounter);
            
            bool connectSuccessful = systemState.cmdStnConnection.connect(systemState.cmdStnIP, systemState.cmdStnPort, 1000);
            uint32_t quirkFlags = 0;
            
            if (!connectSuccessful)
            {
              systemState.cmdStnConnection.stop();
              Serial.printf("[SYS]: Connect FAILED!\n");
              timerNetworkSearch.setup(2000);
              netState = NETSTATE_CONNECT_SERVER_WAIT;
              break;
            }
            color = WS2812_GREEN;
            Serial.printf("[SYS]: Connect successful!\n");
            // Create command station object
            
            switch(systemState.cmdStnType)
            {
              case CMDSTN_JMRI:
              case CMDSTN_DCCEX:
              case CMDSTN_LNWI:
                // These are all variants of the WiThrottle protocol, but all implement it just a little differently
                // The main difference right now is that DCC-EX and LNWI do not support the "force function" ('f') command
                //  which is bloody annoying
                if (CMDSTN_LNWI == systemState.cmdStnType)
                  quirkFlags |= WITHROTTLE_QUIRK_LNWI;
                else if (CMDSTN_DCCEX == systemState.cmdStnType)
                  quirkFlags |= WITHROTTLE_QUIRK_DCCEX;

                char macBuffer[16];
                snprintf(macBuffer, sizeof(macBuffer)-1, "%02X%02X", systemState.macAddr[4], systemState.macAddr[5]);
                systemState.cmdStn = new WiThrottle(macBuffer);
                systemState.cmdStn->begin(systemState.cmdStnConnection, quirkFlags, systemState.debugLvlCommandStation);
                if (FC_SOURCE_CMDSTN == systemState.fcSource)
                {
                  if (systemState.cmdStn->fastClockConnect(&systemState.fastClock))
                  {
                    systemState.fastClock.enable();
                  } else {
                    systemState.fastClock.disable();
                  }
                }
                netState = NETSTATE_CONNECTED;
                break;

              case CMDSTN_ESU:
                systemState.cmdStn = new ESUCabControl;
                systemState.cmdStn->begin(systemState.cmdStnConnection, 0, systemState.debugLvlCommandStation);
                netState = NETSTATE_CONNECTED;
                break;

              default: // Do nothing, don't know what it is
                break;
            }
          }
          break;

        case NETSTATE_CONNECTED:
          color = WS2812_GREEN;
          systemState.isWifiConnected = true;
          systemState.isCmdStnConnected = true;
          if (WiFi.status() != WL_CONNECTED)
          {
            Serial.printf("[SYS]: WiFi network disconnected, waiting for reconnect\n");
            timerNetworkSearch.setup(10000);
            netState = NETSTATE_CONNECTION_HICCUP_WAIT;
            break;
          }
          else if (!systemState.cmdStnConnection.connected())
          {
            Serial.printf("[SYS]: Command station disconnected\n");
            netState = NETSTATE_DISCONNECT_SERVER;
          }
          break;

        case NETSTATE_DISCONNECT_SERVER:
          color = WS2812_YELLOW;
          Serial.printf("[SYS]: Deleting command station object\n");
          systemState.isCmdStnConnected = false;
          systemState.cmdStn->end();
          delete systemState.cmdStn;
          netState = NETSTATE_FIND_SERVER;
          break;

        case NETSTATE_CONNECTION_HICCUP_WAIT:
          // If we're here, the connection we had died.  It might be transient, so give it a second to see
          //  if it decides to come back before we kill everything and start over
          if (WiFi.status() == WL_CONNECTED)
          {
            Serial.printf("[SYS]: WiFi network restored\n");
            netState = NETSTATE_CONNECTION_HICCUP_WAIT;
          }
          else if (timerNetworkSearch.test(false))
          {
            Serial.printf("[SYS]: WiFi network DID NOT recover, resetting\n");
            netState = NETSTATE_DISCONNECT_WIFI;
          }
      }

      // If we don't have good wifi and command station connections, we can't act as a receiver
      if (netState != NETSTATE_CONNECTED)
      {
        // Just throw away any MRBus traffic until we get both connections
        mrbus.rxPktQueue->flush();
        return;
      }

      // ****************************************************************************
      // CONNECTED AND RUNNING
      //  This is normally where we handle and bridge packets if everything's connected
      // ****************************************************************************

      systemState.cmdStn->update();

      while(!mrbus.rxPktQueue->isEmpty())
      {
        MRBusPacket pkt;
        mrbus.rxPktQueue->pop(pkt);
        if (pkt.src == systemState.mrbusSrcAddrGet())
        {
          // Ouch, conflicting base station detected
          systemState.registerConflictingBase();
          continue;
        }

        if (isMRBusThrottlePacket(systemState.mrbusSrcAddrGet(), pkt))
        {
          uint8_t throttleNum = pkt.src - MRBUS_THROTTLE_BASE_ADDR;
          throttles[throttleNum].update(systemState.cmdStn, pkt);
        }

        break;
      }
    
  }
}