# The Iowa Scaled Engineering MRBW-WIFI ProtoThrottle Bridge
## Credits - Bootloader

Credit where credit is due.  This project builds on other open source projects, and without them, this would have taken much, much longer.

First, a credit to Adafruit for developing the [tinyuf2 bootloader](https://github.com/adafruit/tinyuf2) for the ESP32-S2.  Super easy to use, super easy to modify.

## Building the Bootloader

The bootloader is based off the Adafruit tinyuf2 bootloader.  Other than the new board files to give it our unique naming, it's 100% stock code from Github.

git clone https://github.com/adafruit/tinyuf2.git
git submodule init
git submodule update

*** Note - This is the old build instructions from 2023.  It probably needs to be updated as on cursory glance, there's probably some stuff wrong with it ***

cd lib/esp-idf
./install
. ./export.sh

Copy the mrbw-wifi/src-tinyuf2-bootloader/ise_mrbw_wifi_esp32s2 directory in this project to ports/espressif/boards in the tinyuf2 project
Copy the mrbw-wifi/src-tinyuf2-bootloader/partitions-4MBLA.csv directory in this project to ports/espressif in the tinyuf2 project

cd ports/expressif
make BOARD=ise_mrbw_wifi_esp32s2 all

When you get done, there should be a ports/espressif/_build/ise_mrbw_wifi_esp32s2 directory.  From there, you need tinyuf2.bin and bootloader/bootloader.bin

Copy those into the src/platformio/arduino/ise_mrbwwifi_esp32s2 directory (or .platformio/packages/framework-arduinoespressif32/variants/ise_mrbwwifi_esp32s2 to pick them up immediately) as bootloader-tinyuf2.bin (for bootloader/bootloader.bin) and tinyuf2.bin.

## License

The tinyuf2 bootloader is under the open source MIT License.  See original LICENSE file included in this directory.  Original source at https://github.com/adafruit/tinyuf2


