# ==========================================
# Variables
# ==========================================
# Make inherits environment variables, so $(LOCALAPPDATA) will work on Windows.
ARDUINO_CLI ?= arduino-cli
ESPTOOL ?= $(LOCALAPPDATA)/Arduino15/packages/esp32/tools/esptool_py/5.3.1/esptool.exe
BOOTLOADER_BIN_DIR ?= ./uf2-bootloader
BUILD_OUTPUT_DIR ?= ./release-build
BASENAME ?= mrbw-wifi

# Specify your serial port for the flash target
PORT ?= COM11

# ==========================================
# Phony Targets (Commands that aren't files)
# ==========================================
.PHONY: all firmware merge flash clean

# Default target when you just run `make`
all: merge

# ==========================================
# Build Targets
# ==========================================

firmware:
	@echo ""
	@echo "***********************************"
	@echo "  Building firmware"
	@echo "***********************************"
	@echo ""
	"$(ARDUINO_CLI)" compile -v --profile release \
		--fqbn=esp32:esp32:esp32s2:CDCOnBoot=cdc,PSRAM=enabled,FlashSize=4M \
		--build-property "build.custom_partitions=./partitions.csv" \
		--build-property "upload.maximum_size=2883584" \
		--build-property "compiler.optimization_flags=-O2" \
		--output-dir "$(BUILD_OUTPUT_DIR)" \
		./$(BASENAME).ino

merge: firmware
	@echo ""
	@echo "***********************************"
	@echo "  Merging firmware"
	@echo "***********************************"
	@echo ""
	"$(ESPTOOL)" --chip esp32s2 merge-bin -o "$(BUILD_OUTPUT_DIR)/$(BASENAME)-full.bin" --flash-mode dio --flash-freq 80m --flash-size 4MB \
		0x1000 "$(BOOTLOADER_BIN_DIR)/bootloader-tinyuf2.bin" \
		0x8000 "$(BUILD_OUTPUT_DIR)/$(BASENAME).ino.partitions.bin" \
		0x10000 "$(BUILD_OUTPUT_DIR)/$(BASENAME).ino.bin" \
		0x2d0000 "$(BOOTLOADER_BIN_DIR)/tinyuf2.bin"
	"$(ESPTOOL)" --chip esp32s2 merge-bin -o "$(BUILD_OUTPUT_DIR)/$(BASENAME)-dev.bin" --flash-mode dio --flash-freq 80m --flash-size 4MB \
		0x1000 "$(BUILD_OUTPUT_DIR)/$(BASENAME).ino.bootloader.bin" \
		0x8000 "$(BUILD_OUTPUT_DIR)/$(BASENAME).ino.partitions.bin" \
		0x10000 "$(BUILD_OUTPUT_DIR)/$(BASENAME).ino.bin"


erase:
	@echo ""
	@echo "***********************************"
	@echo "  Erasing Device"
	@echo "***********************************"
	@echo ""
	"$(ESPTOOL)" --chip esp32s2 --port $(PORT) erase-flash

flash:
	@echo ""
	@echo "***********************************"
	@echo "  Flashing Device"
	@echo "***********************************"
	@echo ""
	"$(ESPTOOL)" --chip esp32s2 --port $(PORT) write-flash 0x0 "$(BUILD_OUTPUT_DIR)/$(BASENAME)-dev.bin"


flash-full: merge
	@echo ""
	@echo "***********************************"
	@echo "  Flashing Device"
	@echo "***********************************"
	@echo ""
	"$(ESPTOOL)" --chip esp32s2 --port $(PORT) write-flash 0x0 "$(BUILD_OUTPUT_DIR)/$(BASENAME)-full.bin"

clean:
	@echo "Cleaning build directory and generated binaries..."
	rm -rf "$(BUILD_OUTPUT_DIR)"
	