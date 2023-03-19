#include <Arduino.h>
#include <Wire.h>
#include "FFat.h"

// Include drivers for MRBW-WIFI hardware
#include "versions.h"
#include "msc.h"
#include "display.h"
#include "ws2812.h"

#define PIN_SDA  33
#define PIN_SCL  34

ISE_MSC msc;
I2CDisplay display;

typedef enum 
{
  STATE_STARTUP               = 0x00,
  STATE_DRAW_SPLASH_SCREEN    = 0x01,
  STATE_MAIN_LOOP             = 0x02
} loopState_t;

loopState_t mainLoopState = STATE_STARTUP;


void setup() 
{
// put your setup code here, to run once:
  Serial.begin();
  Wire.setPins(PIN_SDA, PIN_SCL);
  Wire.setClock(400000UL);
  Wire.begin();
  ws2812Set(0x0f0000);
  msc.start();

  display.setup(&Wire, 0x3C);

  ws2812Set(0x000f00);

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

void loop() 
{
  char buffer[128];

  //snprintf(buffer, sizeof(buffer), "Testing %04d", loopCnt++);
  //display.putstr(buffer, 0, 0);
  
  // put your main code here, to run repeatedly:
  Serial.printf("Some crap\n");
  delay(1000);

  switch(mainLoopState)
  {
    case STATE_STARTUP:
      drawSplashScreen();
      mainLoopState = STATE_DRAW_SPLASH_SCREEN;
      break;

    case STATE_DRAW_SPLASH_SCREEN:
      // Draw splash screen here;
      break;

    default:
      break;

  }
}