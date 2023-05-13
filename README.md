# mrbw-wifi
Wireless MRBus to WiFi (802.11) Bridge for ProtoThrottles

# Setting up Platform.IO

In your user platformio directory (in Windows, typically C:\Users\(username)\.platformio):

Copy the src-platformio/arduino/ise_mrbwwifi_esp32s2 directory to .platformio/packages/framework-arduinoespressif32/variants
Copy the src-platformio/ise_mrbwwifi_esp32s2.json file to .platformio/platforms/espressif32/boards 


# Building the Bootloader

The bootloader is based off the Adafruit tinyuf2 bootloader.

git clone https://github.com/adafruit/tinyuf2.git
git submodule init
git submodule update

cd lib/esp-idf
./install
. ./export.sh

Copy the src-tinyuf2-bootloader/ise_mrbw_wifi_esp32s2 directory in this project to ports/espressif/boards in the tinyuf2 project
Copy the src-tinyuf2-bootloader/partitions-4MBLA.csv directory in this project to ports/espressif in the tinyuf2 project

cd ports/expressif
make BOARD=ise_mrbw_wifi_esp32s2 all

When you get done, there should be a ports/espressif/_build/ise_mrbw_wifi_esp32s2 directory.  From there, you need tinyuf2.bin and bootloader/bootloader.bin

Copy those into the src/platformio/arduino/ise_mrbwwifi_esp32s2 directory (or .platformio/packages/framework-arduinoespressif32/variants/ise_mrbwwifi_esp32s2 to pick them up immediately) as bootloader-tinyuf2.bin (for bootloader/bootloader.bin) and tinyuf2.bin.

