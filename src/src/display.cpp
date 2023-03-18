
#include "display.h"

#define SSD1306_WIDTH 128 
#define SSD1306_HEIGHT 64 
static const uint32_t DISPLAY_BUFFER_SIZE  = (SSD1306_WIDTH * SSD1306_HEIGHT / 8);

#define SSD1306_BLACK 0   ///< Draw 'off' pixels
#define SSD1306_WHITE 1   ///< Draw 'on' pixels
#define SSD1306_INVERSE 2 ///< Invert pixels

#define SSD1306_MEMORYMODE 0x20          ///< See datasheet
#define SSD1306_COLUMNADDR 0x21          ///< See datasheet
#define SSD1306_PAGEADDR 0x22            ///< See datasheet
#define SSD1306_SETCONTRAST 0x81         ///< See datasheet
#define SSD1306_CHARGEPUMP 0x8D          ///< See datasheet
#define SSD1306_SEGREMAP 0xA0            ///< See datasheet
#define SSD1306_DISPLAYALLON_RESUME 0xA4 ///< See datasheet
#define SSD1306_DISPLAYALLON 0xA5        ///< Not currently used
#define SSD1306_NORMALDISPLAY 0xA6       ///< See datasheet
#define SSD1306_INVERTDISPLAY 0xA7       ///< See datasheet
#define SSD1306_SETMULTIPLEX 0xA8        ///< See datasheet
#define SSD1306_DISPLAYOFF 0xAE          ///< See datasheet
#define SSD1306_DISPLAYON 0xAF           ///< See datasheet
#define SSD1306_COMSCANINC 0xC0          ///< Not currently used
#define SSD1306_COMSCANDEC 0xC8          ///< See datasheet
#define SSD1306_SETDISPLAYOFFSET 0xD3    ///< See datasheet
#define SSD1306_SETDISPLAYCLOCKDIV 0xD5  ///< See datasheet
#define SSD1306_SETPRECHARGE 0xD9        ///< See datasheet
#define SSD1306_SETCOMPINS 0xDA          ///< See datasheet
#define SSD1306_SETVCOMDETECT 0xDB       ///< See datasheet

#define SSD1306_SETLOWCOLUMN 0x00  ///< Not currently used
#define SSD1306_SETHIGHCOLUMN 0x10 ///< Not currently used
#define SSD1306_SETSTARTLINE 0x40  ///< See datasheet

#define SSD1306_EXTERNALVCC 0x01  ///< External display voltage source
#define SSD1306_SWITCHCAPVCC 0x02 ///< Gen. display voltage from 3.3V

#define SSD1306_RIGHT_HORIZONTAL_SCROLL 0x26              ///< Init rt scroll
#define SSD1306_LEFT_HORIZONTAL_SCROLL 0x27               ///< Init left scroll
#define SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29 ///< Init diag scroll
#define SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL 0x2A  ///< Init diag scroll
#define SSD1306_DEACTIVATE_SCROLL 0x2E                    ///< Stop scroll
#define SSD1306_ACTIVATE_SCROLL 0x2F                      ///< Start scroll
#define SSD1306_SET_VERTICAL_SCROLL_AREA 0xA3             ///< Set scroll range

I2CDisplay::I2CDisplay()
{
    this->i2c = NULL;
    this->addr = 0x00;
    this->unsentChanges = false;
    this->displayBuffer = NULL;
    this->writeCache = NULL;
}

I2CDisplay::~I2CDisplay()
{
    if (NULL != this->displayBuffer)
        free(this->displayBuffer);
    if (NULL != this->writeCache)
        free(this->writeCache);
}

bool I2CDisplay::sendCmdList(const uint8_t* cmdStr, uint32_t cmdStrSz)
{
    uint32_t bytesWritten = 0;
    this->i2c->beginTransmission(this->addr);
    this->i2c->write(0x00); // Command stream
    while (cmdStrSz--)
    {
        this->i2c->write(*cmdStr++);
    }
    this->i2c->endTransmission();
    return true;
}

bool I2CDisplay::setup(TwoWire& i2cInterface, uint8_t address)
{
    this->i2c = &i2cInterface;
    this->addr = address;

    const uint8_t initCmds[] =
    {
        SSD1306_DISPLAYOFF,
        SSD1306_SETDISPLAYCLOCKDIV, 0x80,
        SSD1306_SETMULTIPLEX, (SSD1306_HEIGHT - 1),
        SSD1306_SETDISPLAYOFFSET,   0x00,
        SSD1306_SETSTARTLINE,
        SSD1306_CHARGEPUMP, 0x14,   // Internal charge pump
        SSD1306_MEMORYMODE, 0x00,
        SSD1306_SEGREMAP | 0x01,
        SSD1306_COMSCANDEC,
        SSD1306_SETCOMPINS, 0x12,   // Appropriate for 128x64
        SSD1306_SETCONTRAST, 0xCF,  // Appropriate for 128x64, chargepump
        SSD1306_SETPRECHARGE, 0xF1,
        SSD1306_SETVCOMDETECT, 0x40,
        SSD1306_DISPLAYALLON_RESUME, 
        SSD1306_NORMALDISPLAY, 
        SSD1306_DEACTIVATE_SCROLL, 
        SSD1306_DISPLAYON
    };


    if (NULL == this->displayBuffer)
        this->displayBuffer = (uint8_t*)malloc(sizeof(uint8_t) * DISPLAY_BUFFER_SIZE);
    if (NULL == this->writeCache)
        this->writeCache = (uint8_t*)malloc(sizeof(uint8_t) * DISPLAY_BUFFER_SIZE);
    memset(this->displayBuffer, 0, DISPLAY_BUFFER_SIZE);
    memset(this->writeCache, 0, DISPLAY_BUFFER_SIZE);
    this->unsentChanges = false;

    // Send initialization bullshit

    this->sendCmdList(initCmds, sizeof(initCmds)/ sizeof(initCmds[0]));
/*
    const uint8_t initCmds2[] =
    {
        0x00,
        0x10,
        0x40
    };

    //this->sendCmdList(initCmds2, sizeof(initCmds2)/ sizeof(initCmds2[0]));

    return true;
*/
    for(uint16_t q=0; q<DISPLAY_BUFFER_SIZE; q++)
    {
        this->i2c->beginTransmission(0x3C); //Start communication with slave
        this->i2c->write(0x40); //Data stream
        for(uint8_t w=0; w<16; w++)
        {
            this->i2c->write(this->displayBuffer[q]); //Transmit data to be displayed
            q++;
        }
        q--;
        this->i2c->endTransmission(); //End communication with slave
    }


    return true;
}