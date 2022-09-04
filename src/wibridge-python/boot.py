import board
import digitalio
import time
import storage

switchFR = digitalio.DigitalInOut(board.IO21)
switchFR.direction = digitalio.Direction.INPUT
switchFR.pull = digitalio.Pull.UP
time.sleep(0.05)

if not switchFR.value:
  print("Erasing filesystem")
  storage.erase_filesystem()
  print("Done")
  while(1):
    pass
