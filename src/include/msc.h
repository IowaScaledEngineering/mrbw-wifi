#pragma once

#include <stdint.h>
#include <USB.h>
#include <USBMSC.h>
#include <esp_partition.h>

class ISE_MSC
{
    private:
        USBMSC MSC;
    public:
        ISE_MSC();
        ~ISE_MSC();
        bool start(bool mediaPresent = true, bool writeable = true);
        bool stop();
        bool isDirty();
};
