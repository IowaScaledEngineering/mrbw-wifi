import adafruit_ssd1306
import time
import microcontroller
import supervisor
import board
import neopixel_write
import digitalio
import board
from version import _version_major_, _version_minor_, _version_delta_, _version_git_

WHITE = bytearray([64, 64, 64])
RED = bytearray([0, 64, 0])
AMBER = bytearray([32, 64, 0])
GREEN = bytearray([64, 0, 0])
OFF = bytearray([0,0,0])

class Screen:
  def __init__(self):
    self.display = None
    self.spinner = 0
    self.spinnerElements = [ '-', '\\', '|', '/' ]
    self.displayBuffer = ['                     ', '                     ', '                     ', '                     '] 
    self.networkText = { 'lnwi':'LNWI', 'esu':'ESU ', 'withrottle':'WTHR' }
    self.displayWriteBuffer = ['                     ', '                     ', '                     ', '                     '] 
    self.lastSSID = ''
    self.debug = False

    self.ledPin = None
    try:
      self.ledPin = digitalio.DigitalInOut(board.IO45)
      self.ledPin.direction = digitalio.Direction.OUTPUT
    except Exception as e:
      print("Cannot init LED pin\n%s" % (e))
      self.ledPin = None

    self.lastLEDColor = None

  def setLED(self, color):
    if self.lastLEDColor != color:
      self.lastLEDColor = color
      if self.ledPin != None:
        neopixel_write.neopixel_write(self.ledPin, self.lastLEDColor)

  def init(self, i2c):
    self.display = adafruit_ssd1306.SSD1306_I2C(128, 32, i2c)
    self.clearScreen()
    self.setLED(OFF)
    
  def clearScreen(self):
    self.display.fill(0)
    self.display.show()

  def writeText(self, txtStr, xPos, yPos, clearArea=False):
    if clearArea:
      self.display.fill_rect(xPos * 6 + 1, yPos * 8 + 1, len(txtStr) * 7, 8, 0);
    self.display.text(txtStr, xPos * 6 + 1, yPos * 8 + 1, True)
    self.display.show()

  def flushBufferedText(self):
    for lineNum in range(0,4):
      if self.displayBuffer[lineNum] == self.displayWriteBuffer[lineNum]:
        continue
      for charNum in range(0,22):
        if self.displayWriteBuffer[lineNum][charNum:charNum+1] != self.displayBuffer[lineNum][charNum:charNum+1]:
          self.display.fill_rect(charNum * 6 + 1, lineNum * 8 + 1, 6, 8, 0);
          self.display.text(self.displayWriteBuffer[lineNum][charNum:charNum+1], charNum * 6 + 1, lineNum * 8 + 1, True)

    for lineNum in range(0,4):
      self.displayBuffer[lineNum] = self.displayWriteBuffer[lineNum]
    self.display.show()

  def flushBufferedText_slow(self):
    self.display.fill(0)
    for lineNum in range(0,4):
      self.display.text(self.displayWriteBuffer[lineNum], 0, lineNum * 8 + 1, True)
      self.displayBuffer[lineNum] = self.displayWriteBuffer[lineNum]
    self.display.show()


  def writeBufferedText(self, txtStr, xPos, yPos):
    tmpbuffer = self.displayWriteBuffer[yPos][0:xPos] + txtStr + self.displayWriteBuffer[yPos][xPos + len(txtStr):]
    self.displayWriteBuffer[yPos] = tmpbuffer

  def status_screen(self, sysState):
#     000000000011111111112
#     012345678901234567890
#  0:[a:LNWI    s T:nn B:aa] a=A/auto, C/config file (LNWI/ESU /WTHR/NONE) s=spinner nn=throttles connected aa=base addr  
#  1:[SSID-NAME-HERE-HERE-H] (scrolling)
#  2:[192.168.255.255:ppppp]
#  3:[R:-sss cc xxC xxxxlps] s=rssi, cc=channel
    if self.debug:
      displayUpdateStart = time.monotonic_ns()

    # Update spinner
    self.spinner = (self.spinner + 1) % len(self.spinnerElements)
    self.writeBufferedText(self.spinnerElements[self.spinner], 10, 0);

    if supervisor.runtime.serial_connected:
      self.writeBufferedText('U', 11, 0);
    else:
      self.writeBufferedText(' ', 11, 0);
    # Update throttles connected and base address
    self.writeBufferedText("T:%02d B:%02d" % (sysState.throttlesConnected, sysState.baseAddr), 12, 0);

    # Update SSID line if we're connected
    ledColor = RED
    
    if not sysState.isWifiConnected:
      if sysState.isAutoNetwork:
        newSSIDText = "%-21.21s" % ("(Searching...)")
      else:
        newSSIDText = "%-21.21s" % (sysState.networkSSID)
      ipText = "%-21.21s" % ("(Not Connected)")
      rssiText = "R:---"
    else:
      ledColor = AMBER
      newSSIDText = "%-21.21s" % (sysState.networkSSID)
      ipText = "%-21.21s" % (sysState.ipv4)
      rssiText = "R:%02ddB" % (min(0, max(-99, sysState.rssi)))
    self.writeBufferedText(newSSIDText, 0, 1);
    self.writeBufferedText(ipText, 0, 2);
    self.writeBufferedText(rssiText, 0, 3);

    if sysState.lps < 10000:
      self.writeBufferedText("%4dl/s" % (sysState.lps), 14, 3)
    else:
      self.writeBufferedText("%3dkl/s" % (sysState.lps/1000), 14, 3)

    self.writeBufferedText("%-2d" % (microcontroller.cpu.temperature), 10, 3)
    self.writeBufferedText("C", 12, 3)


    if sysState.isAutoNetwork:
      self.writeBufferedText("A:", 0, 0);
    else: 
      self.writeBufferedText("C:", 0, 0);
    
    networkText = "NONE"
    if sysState.cmdStationType in self.networkText.keys():
      networkText = self.networkText[sysState.cmdStationType]
    self.writeBufferedText(networkText, 2, 0)
    if self.debug:
      print("Content %d uS" % ((time.monotonic_ns() - displayUpdateStart) / 1000))

    if sysState.isCmdStnConnected:
      ledColor = GREEN

    self.setLED(ledColor)
    self.flushBufferedText()
    if self.debug:
      print("Flush %d uS" % ((time.monotonic_ns() - displayUpdateStart) / 1000))

  def splash_screen(self, version=""):
    self.clearScreen()
    self.writeText("Iowa Scaled", 0, 0)
    self.writeText("Engineering", 0, 1)
    self.writeText("MRBW-WIFI  ", 0, 2)
    self.writeText("%d.%d.%d %6.6s" % (_version_major_, _version_minor_, _version_delta_, _version_git_), 0, 3)
    logoXOffset = 84

    with open("/ise.lgo", 'rb') as logo:
      w = int.from_bytes(logo.read(1), "big")
      h = int.from_bytes(logo.read(1), "big")
      b = bytearray(logo.read(1 + (w*h // 8)))

      for row in range(0,h):
        for col in range(0,w):
          byte = ((row*w) + col) // 8
          bit = ((row*w) + col) % 8
          if b[byte] & (1<<bit):
            self.display.pixel(col+logoXOffset, row, 1)
      self.display.show()
