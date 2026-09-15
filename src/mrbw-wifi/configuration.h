#ifndef _CONFIGURATION_H_
#define _CONFIGURATION_H_
#include <Arduino.h>
#include "SystemState.h"
#include "switches.h"
#include "mrbus.h"
#include <esp_partition.h>
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "ws2812.h"

void fsSetupAndConfig(SystemState& systemState, Switches& switches);
void updateBaseAddressFromSwitches(SystemState& systemState, Switches& switches, MRBus& mrbus);
#endif