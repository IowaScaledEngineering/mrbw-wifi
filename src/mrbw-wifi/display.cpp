
#include "display.h"
#include "font.h"

#define SSD1306_WIDTH 128 
#define SSD1306_HEIGHT 64 
#define MAX_X_CHARS    22
#define MAX_Y_CHARS     4

#define MAX(a,b)  ((a>b)?(a):(b))
#define MIN(a,b)  ((a>b)?(b):(a))

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
    this->i2c->beginTransmission(this->addr);
    this->i2c->write(0x00); // Command stream
    while (cmdStrSz--)
    {
        this->i2c->write(*cmdStr++);
    }
    this->i2c->endTransmission();
    return true;
}

bool I2CDisplay::setup(TwoWire* i2cInterface, uint8_t address)
{
  this->i2c = i2cInterface;
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
  this->unsentChanges = false;

    // Send initialization - Wire has a buffer size of 32 bytes, so if init gets any bigger, send in chunks
  this->sendCmdList(initCmds, sizeof(initCmds));

  const uint8_t resetCmds[] = {SSD1306_COLUMNADDR, 0x00, 0x7F, SSD1306_PAGEADDR, 0x00, 0x07};
  this->sendCmdList(resetCmds, sizeof(resetCmds));

  this->clrscr(true);

  return true;
}

bool I2CDisplay::refresh(bool force)
{
  const uint32_t pageSz = 128;

  // Work through, page by page, and see if we need to send an update for this part of the display
  for(uint8_t pageNum=0; pageNum < 8; pageNum++)
  {
    if (force || 0 != memcmp(&this->displayBuffer[pageNum * pageSz], &this->writeCache[pageNum * pageSz], pageSz))
    {
      uint8_t resetCmds[] = {SSD1306_PAGEADDR, 0x00, 0x07};
      resetCmds[1] = resetCmds[2] = pageNum;
      this->sendCmdList(resetCmds, sizeof(resetCmds));

      memcpy(&this->displayBuffer[pageNum * pageSz], &this->writeCache[pageNum * pageSz], pageSz);

      // Okay, this page has some updates, write it
      for(uint16_t q=pageNum * pageSz; q < (pageNum+1) * pageSz; q++)
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
      
    }
  }
  this->unsentChanges = false;
  return true;
}

bool I2CDisplay::setXPos(uint8_t x)
{
  this->xPos = MIN(x, MAX_X_CHARS-1);
  return true;
}

bool I2CDisplay::setYPos(uint8_t y)
{
  this->yPos = MIN(y, MAX_Y_CHARS-1);
  return true;
}

uint8_t I2CDisplay::getXPos()
{
  return this->xPos;
}

uint8_t I2CDisplay::getYPos()
{
  return this->yPos;
}


bool I2CDisplay::clrscr(bool refresh)
{
  const uint8_t resetCmds[] = {SSD1306_COLUMNADDR, 0x00, 0x7F, SSD1306_PAGEADDR, 0x00, 0x07};
  memset(this->writeCache, 0x00, DISPLAY_BUFFER_SIZE);
  this->xPos = 0;
  this->yPos = 0;

  if (refresh)
  {
    this->sendCmdList(resetCmds, sizeof(resetCmds));
    memset(this->displayBuffer, 0x00, DISPLAY_BUFFER_SIZE);

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
    this->unsentChanges = false;
  }
  return true;
}

uint32_t I2CDisplay::putstr(const char* s, int32_t x, int32_t y)
{
  uint32_t charsWritten = 0;
  if (NULL == s)
    return charsWritten;
  
  while(*s)
  {
    charsWritten += putc((const uint8_t)*s++, charsWritten?-1:x, charsWritten?-1:y);
  }
  return charsWritten;
}

bool I2CDisplay::drawISELogo() 
{
  uint32_t xOffset = 84;
  uint32_t rows = sizeof(iseLogo) / logoWidth;
  uint32_t logoByte = 0;

  for (uint32_t row=0; row < rows; row++)
  {
    for(uint32_t column=0; column < logoWidth; column++)
    {
      this->writeCache[row * 128 + column + xOffset] = iseLogo[logoByte++];
    }
  }
  return true;
}


// This array is used to double the height of the font
// at execution time.  Prevents having to store double the font data

uint32_t I2CDisplay::putc(const uint8_t c, int32_t x, int32_t y)
{
  if (-1 == x)
    x = this->xPos;
  if (-1 == y)
    y = this->yPos;

  x = MAX(0, MIN(x, MAX_X_CHARS-1));
  y = MAX(0, MIN(y, MAX_Y_CHARS-1));

  uint32_t xPixelOffset = x * (stdFont_width + 1);
 
  for (uint32_t i = 0; i<stdFont_width; i++)
  {
    uint8_t upperByte = 0;
    uint8_t lowerByte = 0;

    lowerByte = fontExpander[0x0F & stdFont[c][i]];
    upperByte = fontExpander[0x0F & (stdFont[c][i]>>4)];

    this->writeCache[y * 256 + xPixelOffset + i] = lowerByte;
    this->writeCache[y * 256 + 128 + xPixelOffset + i] = upperByte;
  }

  this->writeCache[y * 256 + 128 + xPixelOffset + stdFont_width] = 0x00;

  // Increment the internal cursor
  this->xPos = x+1;
  if (this->xPos > MAX_X_CHARS-1)
  {
    y++;
    this->xPos = 0;
  }

  if (y > MAX_Y_CHARS-1)
    y = 0;
  this->yPos = y;

  return 1;
}
