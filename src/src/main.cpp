#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

#include "FFat.h"

// Include drivers for MRBW-WIFI hardware

#include "msc.h"
#include "display.h"
#include "ws2812.h"

#define PIN_SDA  33
#define PIN_SCL  34

ISE_MSC msc;
I2CDisplay display;

void setup() 
{
  // put your setup code here, to run once:
  Serial.begin();

  Wire.setPins(PIN_SDA, PIN_SCL);
  Wire.setClock(400000UL);

  ws2812Set(0x0f0000);

  //display.setup(Wire, 0x3C);
  Adafruit_SSD1306 display(128, 64, &Wire, -1);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    display.setTextSize(1.5);      // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(0, 0);     // Start at top-left corner
    display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.setCursor(0,0);
  display.println("Some crap here");
  display.display();


  ws2812Set(0x000f00);
  msc.start();
}

void loop() 
{
  // put your main code here, to run repeatedly:
  Serial.printf("Some crap\n");
  delay(1000);
}