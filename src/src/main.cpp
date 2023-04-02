#include <Arduino.h>
#include <Wire.h>
#include "WiFi.h"
#include "FFat.h"

SET_LOOP_TASK_STACK_SIZE(62 * 1024);

// Include drivers for MRBW-WIFI hardware
#include "versions.h"
#include "mrbus.h"
#include "systemState.h"
#include "switches.h"
#include "msc.h"
#include "display.h"
#include "ws2812.h"
#include "periodicEvent.h"


//#define CONFIG_ARDUINO_LOOP_STACK_SIZE 16384
#define PIN_SDA  33
#define PIN_SCL  34

ISE_MSC msc;
I2CDisplay display;
Switches switches;
SystemState systemState;
MRBus mrbus;

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
  switches.setup();
  Wire.setPins(PIN_SDA, PIN_SCL);
  Wire.setClock(400000UL);
  Wire.begin();
  mrbus.begin();

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

  // Start mass storage class driver.  From here on out, we should be read-only on
  // the mrbw-wifi side
  msc.start();

  // Initialize wifi radio and disconnect from anything we might be connected to
  //  as well as set some basic stuff - hostname, always sort by signal strength, etc.
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(systemState.hostname);
  WiFi.disconnect();
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);

  // Start the display
  display.setup(&Wire, 0x3C);

  ws2812Set(0x000f00);
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

  snprintf(lineBuf, sizeof(lineBuf), "%c:%4.4s    %c T:%02d B:%02d", state.isAutoNetwork?'A':'C', cmdStnStr, spinnerChars[spinnerNum], 0, state.baseAddress);
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
        systemState.baseAddress = switches.baseAddressGet();
        mrbus.setAddress(systemState.baseAddress + 0xD0); // MRBus address for the base is switches + 0xD0 offset
        systemState.rssi = WiFi.RSSI();
        drawStatusScreen(systemState);
        Serial.printf("memtask: %d heap free:%d\n", uxTaskGetStackHighWaterMark(NULL), xPortGetFreeHeapSize());
      }

      mrbus.processSerial();

      while(!mrbus.rxPktQueue->isEmpty())
      {
        MRBusPacket* pkt = mrbus.rxPktQueue->pop();
        Serial.printf("Pkt [%02x->%02x]\n", pkt->src, pkt->dest);
      }

      // Send version packet

      // Check wifi network - if we don't have it, try to get it
      if (!systemState.isWifiConnected)
      {
        bool networkFound = systemState.wifiScan();

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
          Serial.printf("Connected\n");
        }
      }

      if (!systemState.isWifiConnected && WiFi.status() == WL_CONNECTED)
      {
        systemState.localIP = WiFi.localIP();
        systemState.isWifiConnected = true;
        systemState.isCmdStnConnected = false;
      }
      else if (systemState.isWifiConnected && WiFi.status() != WL_CONNECTED)
      {
        systemState.localIP.fromString("0.0.0.0");
        systemState.isWifiConnected = false;
        systemState.isCmdStnConnected = false;
      }

      if (!systemState.isWifiConnected)
        return; // Out we go - can't do squat without a wifi connection

      if (systemState.isCmdStnConnected)
      {
        // Just make sure we're really still connected
        if (!systemState.cmdStnConnection.connected())
        {
          systemState.cmdStnConnection.stop();
          systemState.isCmdStnConnected = false;
        }
      }


      if (!systemState.isCmdStnConnected)
      {
        // Try to build a connection
        if (!systemState.cmdStnIPSetup())
          return;  // No IP found for a command station, on with life - return from loop() and start over
        
        bool connectSuccessful = systemState.cmdStnConnection.connect(systemState.cmdStnIP, systemState.cmdStnPort);
        
        if (connectSuccessful)
          systemState.isCmdStnConnected = true;

      }

      if (!systemState.isCmdStnConnected)
        return;

      // If we're here, we should have a command station connected




      // Check IP connection - if we don't have it, try to get it
      break;

    default:
      break;

  }
}