ESPTOOL=esptool.py
CURRENT_FW=./releases/mrbwwifi-1_0_6-2c3e76-master.bin

erase:
	$(ESPTOOL) -c esp32s2 -p /dev/ttyACM0 --after no_reset erase_flash

flash:
	$(ESPTOOL) -c esp32s2 -p /dev/ttyACM0 --after no_reset write_flash 0 $(CURRENT_FW)

all: erase flash
