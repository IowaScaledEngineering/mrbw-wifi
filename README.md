# The Iowa Scaled Engineering MRBW-WIFI ProtoThrottle Bridge

Wireless MRBus to WiFi (802.11) Bridge for ProtoThrottles

# Configuration Options

## Automagic Configuration

The mrbw-wifi, by default, should connect to a good number of things automagically.  ESU CabControl systems using the onboard wifi, Digitrax LNWIs, default DCC-EX builds, and Steve Todd's rpi-jmri images are supported out of the box with no configuration through auto-discovery.

## Manual Configuration

If you do need to configure something different, you'll need to edit the configuration file.  When plugged into a computer via the USB cord, the device presents a mass storage class drive, much like a flash/thumb drive.  On this drive is a single file - config.txt.  Open it with a text editor like Notepad or Notepad++ (or whatever the Mac equivalent is - I have no idea).

The file is just a normal key = value kind of configuration file.  Any line starting with a # is considered a comment and will be ignored.

So, for example, if I want to connect to my layout's network, I might put in three lines:

mode = withrottle
ssid = CRNW Wireless Net
password = TotallyNotMyPassword1

Here's a list of valid parameters in the config.txt file:

| Parameter | Description                                                                                     |
|-----------|-------------------------------------------------------------------------------------------------|
| ssid      | If you need to explicitly connect to a wifi network, put its name here.  Do not use quotes or anything else, just the network name as a parameter, even if it has spaces. |
| password  | If you need to connect to a secured wifi network that you specified in the ssid parameter, specify the password here.  If your network specified in ssid does not have a password, just leave an empty value in the password field.  |
| mode      | If an ssid is specified, the command station mode must be set.  If the ssid field is not set, the mode parameter can be used to restrict the sort of command station networks that the wifi will automatically discover.  Mode can be `withrottle`, `lnwi`, `esu` currently.  For JMRI, DCC-EX, MRC, and most other WiThrottle-compatible servers, use `withrottle`.  For LNWIs, use `lnwi`.  For ESU CabControl or ECoS systems, use `esu`.   |
| serverIP  | Normally the wifi will use mDNS to discover where the server is located.  If for some reason mDNS service discovery does not work, you can explicitly specify the server IP here.  It should be in the form 1.2.3.4 |
| serverPort  | Normally the wifi will use mDNS to discover where the server is located.  If your server does not run on the standard port for the protocol and you have explicitly set the IP address with serverIP, then you should also set the port to connect on with this option. |
| hostname  | If you want the wifi to explicitly use a different hostname when it connects to the network, please specify it here.  By default, it will be mrbw-wifi-XXXX, where XXXX is the last two octets of the MAC address expressed as hexadecimal to make it unique. |
| fastClockSource  | For JMRI, fast time is sent over the withrottle connection.  If you want to send fast time from your JMRI server via the wifi bridge to your ProtoThrottles and/or ISE fast clock remote displays, set this to `cmdstn`.  To disable any fast time being broadcast by the wifi bridge, set it to `none` or omit the parameter entirely. |
| logLevel  | *Advanced Users Only* The wifi bridge exposes a virtual serial port on the USB interface that it uses to send diagnostic logging information about what it's doing.  logLevel controls the verbosity at a global level.  Options in order of increasing verbosity are  `error`, `warn`, `info`, `debug`.  By default, it's set to `info` if not specified.  Unless you're watching the logs and trying to help us debug a specific problem, there's no reason to use this parameter.  In addition, you can use `logLevelCommandStation`, `logLevelMRBus`, or `logLevelSystem` as key values to turn up logging on only a specific part of the code that's having an issue. |




# Credits

Credit where credit is due.  This project builds on other open source projects, and without them, this would have taken much, much longer.

First, a credit to Adafruit for developing the [tinyuf2 bootloader](https://github.com/adafruit/tinyuf2) for the ESP32-S2.  Super easy to use, super easy to modify.

Secondly, to Espressif and all the contributors to the Arduino framework for the ESP32 series.  The MRBW-WIFI code is a mix of native ESP-IDF calls and some Arduino stuff, which saved me days if not weeks of figuring out stuff like their WiFi and IP stack.  Eventually we'll get to all native ESP-IDF code, but for now it's an excellent compromise.

Thirdly, to Platform.IO.  Seriously, you guys rock.  A little hard to figure out in spots, particularly where it goes off the rails, but genuinely a huge timesaver for getting up to speed on ESP32 without spending weeks mucking about with trying to figure out setup.  While I wasn't a huge VSCode fan when I started, it's more than earned its keep.

Finally, I'd like to thank ESU, who generously provided us with their protocol documentation, as well as Brett Hoffman and JMRI for the excellent WiThrottle protocol.  

Digitrax, you really need to work on the LNWI.  That thing is a weird glitchy little gremlin. 

# License

The ISE MRBW-WIFI source code is all covered by the GNU General Public License v3.  Full details are in the LICENSE file.
Other stuff, like schematics, PCB layout, and documentation, is Creative Commons CC BY-SA.

# Compiling and Development Thoughts

The MRBW-WIFI is open source software and hardware, and we encourage those interested to tinker with it, fix bugs, add features, or whatever they'd like.  As such, I'm including some thoughts about how to build it.

## Setting up Platform.IO

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


## Building the Bootloader

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

## Building UF2 images

NOTE:  This step shouldn't be necessary any more.  PIO should now be building the UF2 and master bin image in the src/build/release directory.  It's more or less documented so I remember how it works if I have to change things.

Basically the tinyuf2 bootloader confines you to the OTA0 partition.  So it's expecting the base of the UF2 image to be 0x00000000 and it'll add the offset based on the partition table.  That does mean if we ever want to change partitioning, we probably are going to have to do it via esptool.  

python3 ../../util/uf2conv.py -f 0xbfdd4eee -b 0 -c -o mrbwwifi-combined.uf2 firmware.bin

## Building the Master Firmware Image

NOTE:  This step shouldn't be necessary any more.  PIO should now be building the UF2 and master bin image in the src/build/release directory.  It's more or less documented so I remember how it works if I have to change things.

python3 ../../util/join_bins.py mrbwwifi-master.bin 0x1000 ../../../src-platformio/arduino/ise_mrbwwifi_esp32s2/bootloader-tinyuf2.bin 0x8000 partitions.bin 0xE000 ../../../src-platformio/arduino/ise_mrbwwifi_esp32s2/boot_app0.bin 0x10000 firmware.bin 0x2d0000 ../../../src-platformio/arduino/ise_mrbwwifi_esp32s2/tinyuf2.bin