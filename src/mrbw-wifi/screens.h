#ifndef _SCREENS_H_
#define _SCREENS_H_
#include "SystemState.h"
#include "display.h"

void drawSplashScreen(SystemState& state, I2CDisplay& display);
void drawStatusScreen(SystemState& state, I2CDisplay& display);

#endif
