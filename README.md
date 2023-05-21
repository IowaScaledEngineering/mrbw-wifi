# mrbw-wifi
Wireless MRBus to WiFi (802.11) Bridge for ProtoThrottles

# Setting up Platform.IO

Install VS Code
https://code.visualstudio.com/download
sudo apt install <name>.deb

Open up VS Code, click on the extensions block on the left side (bottom thing, looks like tetris blocks) and search for "PlatformIO IDE" - click install

Go to "Source Control" (third icon down, left side), clone repository, clone from github (up top), log in, search, yada yada....

It'll pop under a window to pick as to where to clone it.  This is the directory that will hold the directory of the project, not the root level where all the files are going to go.  So for example, if you're cloning mrbw-wifi and you want it in /home/ndholmes/data/projects/mrbw-wifi, you'd pick /home/ndholmes/data/projects here 

Once you pick it, it'll pick up on the fact it needs to install some more underlying components, like the Arduino framework

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

# Building UF2 images

Basically the tinyuf2 bootloader confines you to the OTA0 partition.  So it's expecting the base of the UF2 image to be 0x00000000 and it'll add the offset based on the partition table.  That does mean if we ever want to change partitioning, we probably are going to have to do it via esptool.  

python3 ../../util/uf2conv.py -f 0xbfdd4eee -b 0 -c -o mrbwwifi-combined.uf2 firmware.bin

# Building the Master Firmware Image

python3 ../../util/join_bins.py mrbwwifi-master.bin 0x1000 ../../../src-platformio/arduino/ise_mrbwwifi_esp32s2/bootloader-tinyuf2.bin 0x8000 partitions.bin 0xE000 ../../../src-platformio/arduino/ise_mrbwwifi_esp32s2/boot_app0.bin 0x10000 firmware.bin 0x2d0000 ../../../src-platformio/arduino/ise_mrbwwifi_esp32s2/tinyuf2.bin