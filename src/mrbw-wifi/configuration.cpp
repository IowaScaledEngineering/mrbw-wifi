#include "configuration.h"

void updateBaseAddressFromSwitches(SystemState& systemState, Switches& switches, MRBus& mrbus)
{
  systemState.baseAddress = switches.baseAddressGet();
  mrbus.setAddress(systemState.baseAddress + 0xD0); // MRBus address for the base is switches + 0xD0 offset
}

static wl_handle_t wl_handle = WL_INVALID_HANDLE;

void fsSetupAndConfig(SystemState& systemState, Switches& switches)
{
  // Try to mount the FFat partition, format if it can't find it
  Serial.printf("Starting filesystem\n");

  esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true, // If true, try to format the partition if mount fails
        .max_files = 3, // Number of files that can be open at a time
        .allocation_unit_size = 4096, // Size of allocation unit, cluster size.
        .disk_status_check_enable = false,
        .use_one_fat = false, // Use only one FAT table (reduce memory usage), but decrease reliability of file system in case of power failure.

  };

  esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl("/config", "ffat", &mount_config, &wl_handle);
  if (err != ESP_OK) {
       Serial.printf("Failed to mount FATFS (%s)\n", esp_err_to_name(err));
  } else {
    systemState.isFSConnected = true;
    Serial.printf("isFSConnected=%d\n", systemState.isFSConnected);
  }
  FILE* f = NULL;

  if (systemState.isFSConnected)
    f = fopen("/config/config.txt", "r");

  if (NULL == f || !systemState.isFSConnected || switches.factoryResetGet())
  {
    FILE* f = fopen("/config/config.txt", "r");
    if (!f || switches.factoryResetGet())
    {
      ws2812Set(WS2812_BLUE);
      delay(2000);
      Serial.printf("F=[%p] reset=%d\n", f, switches.factoryResetGet());
      err = esp_vfs_fat_spiflash_format_cfg_rw_wl("/config", "ffat", &mount_config);
      if (err != ESP_OK) {
        Serial.printf("Failed to format FATFS (%s)\n", esp_err_to_name(err));
      } else {
        Serial.printf("Format Successful\n");
        systemState.isFSConnected = true;
        Serial.printf("isFSConnected=%d\n", systemState.isFSConnected);
      }

      if (systemState.isFSConnected)
      {
        Serial.printf("Write default configuration\n");
        systemState.configWriteDefault();
      }
      ws2812Set(WS2812_RED);
    } else {
      fclose(f);
    }
  }

  // Read configuration from FAT partition before 
  //  we open it up to allow the host computer to write to it
  systemState.configRead();

  // Unmount it so we're out of the way when we start up the mass storage class
  esp_vfs_fat_spiflash_unmount_rw_wl("/config", wl_handle);
}
