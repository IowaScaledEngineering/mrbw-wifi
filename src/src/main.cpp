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
#include "esp32s2/rom/rtc.h"

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
int resetReason = 0;
void verbose_print_reset_reason(int reason)
{
  switch ( reason)
  {
    case 1  : Serial.println ("Vbat power on reset");break;
    case 3  : Serial.println ("Software reset digital core");break;
    case 4  : Serial.println ("Legacy watch dog reset digital core");break;
    case 5  : Serial.println ("Deep Sleep reset digital core");break;
    case 6  : Serial.println ("Reset by SLC module, reset digital core");break;
    case 7  : Serial.println ("Timer Group0 Watch dog reset digital core");break;
    case 8  : Serial.println ("Timer Group1 Watch dog reset digital core");break;
    case 9  : Serial.println ("RTC Watch dog Reset digital core");break;
    case 10 : Serial.println ("Instrusion tested to reset CPU");break;
    case 11 : Serial.println ("Time Group reset CPU");break;
    case 12 : Serial.println ("Software reset CPU");break;
    case 13 : Serial.println ("RTC Watch dog Reset CPU");break;
    case 14 : Serial.println ("for APP CPU, reseted by PRO CPU");break;
    case 15 : Serial.println ("Reset when the vdd voltage is not stable");break;
    case 16 : Serial.println ("RTC Watch dog reset digital core and rtc module");break;
    default : Serial.println ("NO_MEAN");
  }
}

void setup() 
{
  Serial.begin();
  resetReason = rtc_get_reset_reason(0);
  switches.setup();
  Wire.setPins(PIN_SDA, PIN_SCL);
  Wire.setClock(400000UL);
  Wire.begin();
  mrbus.begin();

  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch
  esp_task_wdt_reset();
  ws2812Set(0x0f0000);

  // Initialize the throttle array
  for(uint32_t thrNum=0; thrNum<MAX_THROTTLES; thrNum++)
  {
    throttles[thrNum].initialize(MRBUS_THROTTLE_BASE_ADDR + thrNum);
  }

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

void drawSplashScreen()
{
  char buffer[32];
  display.clrscr();
  display.putstr("Iowa Scaled", 0, 0);
  display.putstr("Engineering", 0, 1);
  display.putstr("MRBW-WIFI  ", 0, 2);
  snprintf(buffer, sizeof(buffer), "%d.%d.%d %6.6s", MAJOR_VERSION, MINOR_VERSION, DELTA_VERSION, GIT_VERSION);
  display.putstr(buffer, 0, 3);
  display.drawISELogo();  
  display.refresh();
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

  snprintf(lineBuf, sizeof(lineBuf), "%c:%4.4s    %c T:%02d B:%02d", state.isAutoNetwork?'A':'C', cmdStnStr, spinnerChars[spinnerNum], state.activeThrottles, state.baseAddress);
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
    snprintf(lineBuf, sizeof(lineBuf), "%-21.21s", state.localIP.toString().c_str());
  } else {
    snprintf(lineBuf, sizeof(lineBuf), "%-21.21s", "(Not Connected)");
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
      drawSplashScreen();
      tmrSplashScreen.reset();
      mainLoopState = STATE_WAIT_SPLASH_SCREEN;
      break;

    case STATE_WAIT_SPLASH_SCREEN:
      if (tmrSplashScreen.test(false))
      {
        mainLoopState = STATE_MAIN_LOOP;
      }

    case STATE_MAIN_LOOP:
      if (tmrStatusScreenUpdate.test(true))
      {
        uint32_t color = 0x0f0000;
        esp_task_wdt_reset();

        Serial.printf("%us memtask: %d heap free:%d Wifi=%c CmdStn=%c\n", (uint32_t)(esp_timer_get_time() / 1000000), uxTaskGetStackHighWaterMark(NULL), xPortGetFreeHeapSize(), systemState.isWifiConnected?'Y':'N', systemState.isCmdStnConnected?'Y':'N');
        //verbose_print_reset_reason(resetReason);
        systemState.baseAddress = switches.baseAddressGet();
        mrbus.setAddress(systemState.baseAddress + 0xD0); // MRBus address for the base is switches + 0xD0 offset
        systemState.rssi = WiFi.RSSI();

        systemState.activeThrottles = 0;
        for(uint32_t thrNum=0; thrNum < MAX_THROTTLES; thrNum++)
          if (throttles[thrNum].isActive())
            systemState.activeThrottles++;

        drawStatusScreen(systemState);

        if (systemState.isWifiConnected && systemState.isCmdStnConnected)
          color = 0x000f00;
        else if (systemState.isWifiConnected)
          color = 0x0f0f00;
          
        ws2812Set(color);

        // Send version packet
        MRBusPacket versionPkt;

        versionPkt.src = systemState.baseAddress + 0xD0;
        versionPkt.dest = 0xFF;
        versionPkt.len = 14;
        versionPkt.data[0]  = 'v';
        versionPkt.data[1]  = 0x80;
        versionPkt.data[2]  = 0xAB;
        versionPkt.data[3]  = 0xCD;
        versionPkt.data[4]  = 0xEF;
        versionPkt.data[5]  = 1;
        versionPkt.data[6]  = 0;
        versionPkt.data[7]  = 'N';
        versionPkt.data[8]  = 'O';
        versionPkt.data[9]  = ' ';
        versionPkt.data[10] = 'W';
        versionPkt.data[11] = 'I';
        versionPkt.data[12] = 'F';
        versionPkt.data[13] = 'I';

        mrbus.txPktQueue->push(versionPkt);
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
          Serial.printf("Starting connection to [%s] [%s]\n", systemState.ssid, systemState.password);
          WiFi.begin(systemState.ssid, systemState.password);

          // FIXME - put timeout here
          while(WiFi.status() != WL_CONNECTED)
          {
            delay(100);
          }
          Serial.printf("Connected to wifi network\n");
          WiFi.setAutoReconnect(true);
        }

        // Did we connect?  If so, change our indication to wifi connected and go back around the loop
        if(WiFi.status() == WL_CONNECTED)
        {
          systemState.localIP = WiFi.localIP();
          systemState.isWifiConnected = true;
          systemState.isCmdStnConnected = false;
          return;
        }
      }
      else if (WiFi.status() != WL_CONNECTED)  // This implies that systemState.isWifiConnected is true
      {
        systemState.localIP.fromString("0.0.0.0");
        systemState.isWifiConnected = false;
        if (systemState.isCmdStnConnected)
        {
            systemState.cmdStn->end();
            delete systemState.cmdStn;
        }
        systemState.isCmdStnConnected = false;
      }

      if (!systemState.isWifiConnected || tmrStatusScreenUpdate.test(false))
        return; // Out we go - can't do squat without a wifi connection, or maybe we need to do a screen refresh

      if (systemState.isCmdStnConnected)
      {
        // Just make sure we're really still connected
        if (!systemState.cmdStnConnection.connected())
        {
          if (NULL != systemState.cmdStn)
          {
            Serial.printf("Deleting disconnected command station\n\n");
            //systemState.cmdStn->end();
            //delete systemState.cmdStn;
          }
          Serial.printf("Stopping socket connection\n\n");
          systemState.cmdStnConnection.stop();
          systemState.isCmdStnConnected = false;
        }
      }

      if (!systemState.isCmdStnConnected)
      {
        // Try to build a connection
        if (!systemState.cmdStnIPSetup())
          return;  // No IP found for a command station, on with life - return from loop() and start over
        
        Serial.printf("Trying to connect %s:%d\n", systemState.cmdStnIP.toString().c_str(), systemState.cmdStnPort);
        bool connectSuccessful = systemState.cmdStnConnection.connect(systemState.cmdStnIP, systemState.cmdStnPort, 1000);
        Serial.printf("Connect successful = %c\n", connectSuccessful?'Y':'N');

        if (connectSuccessful)
        {
          systemState.isCmdStnConnected = true;
          // Nuke any throttles that may be left over from last time.

          // Create command station object
          switch(systemState.cmdStnType)
          {
            case CMDSTN_JMRI:
              systemState.cmdStn = new WiThrottle;
              systemState.cmdStn->begin(systemState.cmdStnConnection, 0);
              break;
            case CMDSTN_LNWI:
              systemState.cmdStn = new WiThrottle;
              systemState.cmdStn->begin(systemState.cmdStnConnection, WITHROTTLE_QUIRK_LNWI);
              break;
            case CMDSTN_ESU:
              systemState.cmdStn = new ESUCabControl;
              systemState.cmdStn->begin(systemState.cmdStnConnection, 0);
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