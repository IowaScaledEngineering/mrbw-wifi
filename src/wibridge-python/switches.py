import board
import digitalio

class Switches:
  def __init__(self):
    self.currentState = 0
    self.baseAddr = 0
    self.init()

  def setAsInput(self, sw):
    sw.direction = digitalio.Direction.INPUT
    sw.pull = digitalio.Pull.UP

  def init(self):
    self.baseAddr16 = digitalio.DigitalInOut(board.IO9)
    self.setAsInput(self.baseAddr16)
    
    self.baseAddr8 = digitalio.DigitalInOut(board.IO10)
    self.setAsInput(self.baseAddr8)
    
    self.baseAddr4 = digitalio.DigitalInOut(board.IO11)
    self.setAsInput(self.baseAddr4)

    self.baseAddr2 = digitalio.DigitalInOut(board.IO12)
    self.setAsInput(self.baseAddr2)

    self.baseAddr1 = digitalio.DigitalInOut(board.IO13)
    self.setAsInput(self.baseAddr1)

    self.switchA = digitalio.DigitalInOut(board.IO14)
    self.setAsInput(self.switchA)

    self.switchFR = digitalio.DigitalInOut(board.IO21)
    self.setAsInput(self.switchFR)
      
  def getBaseAddr(self):
    self.baseAddr = (not self.baseAddr16.value) * 16 + (not self.baseAddr8.value) * 8 + (not self.baseAddr4.value) * 4 + (not self.baseAddr2.value) * 2 + (not  self.baseAddr1.value) * 1
    return self.baseAddr


