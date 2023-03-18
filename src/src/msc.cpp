#include <stdint.h>
#include "msc.h"

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif


static const uint16_t DISK_BLOCK_SIZE = 4096;    // Should be 512
static const uint16_t FLASH_BLOCK_SIZE = 4096;    // Should be 512
const esp_partition_t* p = NULL;
const bool readOnly = false;
volatile bool fsDirty = false;

static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize)
{
  if (NULL == p || readOnly)
    return -1;

  uint32_t actualOffset = lba *  DISK_BLOCK_SIZE + offset;
  uint8_t* cache = (uint8_t*)malloc(4096);
  if (NULL == cache)
    return -1;


  //HWSerial.printf("MSC WRITE: lba: %u, offset: %u, bufsize: %u\r\n", lba, offset, bufsize);

  // The challenge here is that flash has a 4k erase block, whereas FAT filesystems really should use
  // 512 byte blocks for maximum compatibility.  So we need to do erases and writes aligned to 4k boundaries
  // while accepting all sorts of unaligned writes coming in from the USB MSC layer
  uint32_t bytesWritten = 0;
  
  // currentFlashBase is the base address of the current flash block we're working with
  //   It must be aligned to FLASH_BLOCK_SIZE increments
  // workingOffset is the offset into the write buffer we're currently working

  while(bytesWritten < bufsize)
  {
    uint32_t currentFlashBase = ((lba *  DISK_BLOCK_SIZE + offset + bytesWritten) / FLASH_BLOCK_SIZE) * FLASH_BLOCK_SIZE;
    uint32_t startingOffset = 0;
    if (actualOffset > currentFlashBase)
      startingOffset = actualOffset - currentFlashBase;

    uint32_t bytesToCopy = min(bufsize - bytesWritten, FLASH_BLOCK_SIZE - startingOffset);
    
    //HWSerial.printf("MSC WRITE: currentFlashBase=%u actualOffset=%u bufsize=%u, bytesWritten=%u\r\n", currentFlashBase, actualOffset, bufsize, bytesWritten);

    esp_partition_read(p, currentFlashBase, cache, FLASH_BLOCK_SIZE);
    
    //HWSerial.printf("MSC WRITE: memcpy startingOffset=%u, bytesWritten=%u bytesToCopy=%u\r\n", startingOffset, bytesWritten, bytesToCopy);
    memcpy(cache + startingOffset, buffer + bytesWritten, bytesToCopy);

    esp_err_t err = esp_partition_erase_range(p, currentFlashBase, FLASH_BLOCK_SIZE);
    //HWSerial.printf("MSC WRITE: erase returned %s\r\n", esp_err_to_name(err));

    err = esp_partition_write(p, currentFlashBase, cache, FLASH_BLOCK_SIZE);
    fsDirty = true;
    //HWSerial.printf("MSC WRITE: write returned %s\r\n", esp_err_to_name(err));
    bytesWritten += bytesToCopy;
  }
  
  free(cache);

  return bytesWritten;
}

static int32_t onRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
  if (NULL == p)
    return -1;

  uint32_t actualOffset = lba *  DISK_BLOCK_SIZE + offset;

  //HWSerial.printf("MSC READ: lba: %u, offset: %u, bufsize: %u\r\n", lba, offset, bufsize);
  esp_err_t err = esp_partition_read(p, actualOffset, buffer, bufsize);
  //HWSerial.printf("MSC READ: read returned %s\r\n", esp_err_to_name(err));
  return bufsize;
}

static bool onStartStop(uint8_t power_condition, bool start, bool load_eject){
  //HWSerial.printf("MSC START/STOP: power: %u, start: %u, eject: %u\r\n", power_condition, start, load_eject);
  return true;
}



ISE_MSC::ISE_MSC()
{
}

ISE_MSC::~ISE_MSC()
{
}

bool ISE_MSC::isDirty()
{
    return fsDirty;
}

bool ISE_MSC::start(bool mediaPresent, bool writeable)
{
    if (NULL != p)
        return false;

    p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_FAT,
        "ffat");

    // Failed to find partition
    if (NULL == p)
        return false;

    this->MSC.onStartStop(onStartStop);
    this->MSC.onRead(onRead);
    this->MSC.onWrite(onWrite);
    this->MSC.mediaPresent(true);
    this->MSC.begin(p->size / DISK_BLOCK_SIZE, DISK_BLOCK_SIZE);

    return true;
}

bool ISE_MSC::stop()
{
    this->MSC.end();
    p = NULL;
    fsDirty = false;

    return true;
}
