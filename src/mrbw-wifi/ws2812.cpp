#include "Arduino.h"

#define RGB_LED_GPIO 45

void ws2812Set(uint32_t rgb) 
{
  rgbLedWrite(RGB_LED_GPIO, 0xFF & (rgb>>16), 0xFF & (rgb>>8), 0xFF & (rgb));
}

void ws2812Update(uint32_t rgb) 
{
  static uint32_t lastColor = 0;
  if (lastColor != rgb)
  {
    ws2812Set(rgb);
    delay(10);    
    lastColor = rgb;
  }
}

void ws2812Init()
{
  pinMode(RGB_LED_GPIO, INPUT_PULLUP);
  digitalWrite(RGB_LED_GPIO, HIGH);
}