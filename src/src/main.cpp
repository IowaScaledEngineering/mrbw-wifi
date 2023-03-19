#include <Arduino.h>
#include <Wire.h>
#include "WiFi.h"
#include "FFat.h"

// Include drivers for MRBW-WIFI hardware
#include "versions.h"
#include "switches.h"
#include "msc.h"
#include "display.h"
#include "ws2812.h"
#include "periodicEvent.h"

#define PIN_SDA  33
#define PIN_SCL  34

ISE_MSC msc;
I2CDisplay display;
Switches switches;

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
  ws2812Set(0x0f0000);
  msc.start();

  // Initialize wifi radio and disconnect from anything we might be connected to
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  display.setup(&Wire, 0x3C);

  ws2812Set(0x000f00);
  tmrStatusScreenUpdate.setup(1000); // Status screen updates every 1s
}

uint32_t loopCnt = 0;



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

typedef struct 
{
  uint32_t loopCnt;
  uint8_t baseAddress;

} SystemState;



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
  snprintf(lineBuf, sizeof(lineBuf), "%c:%4.4s    %c T:%02d B:%02d", 'A', "LNWI", spinnerChars[spinnerNum], 0, state.baseAddress);
  display.putstr(lineBuf, 0, 0);

  snprintf(lineBuf, sizeof(lineBuf), "%-21.21s", "(Searching...)");
  display.putstr(lineBuf, 0, 1);

  snprintf(lineBuf, sizeof(lineBuf), "%-21.21s", "(Not Connected)");
  display.putstr(lineBuf, 0, 2);

  uint32_t lps = state.loopCnt;
  state.loopCnt = 0;

  if (lps < 10000)
    snprintf(lpsText, sizeof(lpsText), "%04dl/s", lps);
  else if (lps >= 10000)
    snprintf(lpsText, sizeof(lpsText), "%03dkl/s", lps / 1000);
  else if (lps >= 100000)
    snprintf(lpsText, sizeof(lpsText), "%03dMl/s", lps / 1000000);

  snprintf(lineBuf, sizeof(lineBuf), "R:%02ddB  40C %7.7s", -99, lpsText);
  display.putstr(lineBuf, 0, 3);

  display.refresh();
}

SystemState systemState;

void loop() 
{
/*  Serial.printf("Some crap\n");
  delay(1000);
  Serial.printf("Begin scan\n");
  int n = WiFi.scanNetworks();
  Serial.printf("Scan done, %d networks found\n", n);

  for (int i=0; i<n; i++)
  {
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
  }
*/
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
        drawStatusScreen(systemState);
      }
      break;

    default:
      break;

  }
}