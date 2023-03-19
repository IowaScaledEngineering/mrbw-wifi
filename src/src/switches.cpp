#include "Arduino.h"
#include "switches.h"

Switches::Switches()
{
}

Switches::~Switches()
{
}


void Switches::setup()
{
  pinMode(GPIO_NUM_9, INPUT_PULLUP);   // 16
  pinMode(GPIO_NUM_10, INPUT_PULLUP);  // 8
  pinMode(GPIO_NUM_11, INPUT_PULLUP);  // 4
  pinMode(GPIO_NUM_12, INPUT_PULLUP);  // 2
  pinMode(GPIO_NUM_13, INPUT_PULLUP);  // 1
  pinMode(GPIO_NUM_14, INPUT_PULLUP);  // A
  pinMode(GPIO_NUM_21, INPUT_PULLUP);  // FR

  this->refresh();

}

void Switches::refresh()
{
  this->baseAddress = (digitalRead(GPIO_NUM_9)?0:0x10) | (digitalRead(GPIO_NUM_10)?0:0x08) | (digitalRead(GPIO_NUM_11)?0:0x04) | (digitalRead(GPIO_NUM_12)?0:0x02) | (digitalRead(GPIO_NUM_13)?0:0x01);
  this->switchA = digitalRead(GPIO_NUM_14)?false:true;
  this->switchFR = digitalRead(GPIO_NUM_21)?false:true;
}

uint8_t Switches::baseAddressGet()
{
  this->refresh();
  return this->baseAddress;
}
