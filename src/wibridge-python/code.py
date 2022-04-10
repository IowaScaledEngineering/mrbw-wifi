import time
import board
import digitalio
import busio
import mrbus
import microcontroller
import ipaddress
import wifi
import socketpool
import adafruit_ssd1306

import withrottle
import MRBusThrottle

# Get wifi details and more from a secrets.py file
try:
    from secrets import secrets
except ImportError:
    print("WiFi secrets are kept in secrets.py, please add them there!")
    raise




def write_text(display, txtStr, xPos, yPos):
  display.text(txtStr, xPos * 5 + 1, yPos * 8 + 1, True)
  display.show()
  
uart = busio.UART(board.A0, board.A1, baudrate=115200, timeout=0.000001, receiver_buffer_size = 1024)
i2c = busio.I2C(board.SCL, board.SDA)

led = digitalio.DigitalInOut(board.LED)
led.direction = digitalio.Direction.OUTPUT

mrbee = mrbus.mrbus(0x03, uart)
lastpkt = time.monotonic()

display = adafruit_ssd1306.SSD1306_I2C(128, 32, i2c)

print("My MAC addr:", [hex(i) for i in wifi.radio.mac_address])

print("Available WiFi networks:")
for network in wifi.radio.start_scanning_networks():
    print("\t%s\t\tRSSI: %d\tChannel: %d" % (str(network.ssid, "utf-8"),
            network.rssi, network.channel))
wifi.radio.stop_scanning_networks()

print("Connecting to %s"%secrets["ssid"])
wifi.radio.connect(secrets["ssid"], secrets["password"])
print("Connected to %s!"%secrets["ssid"])
print("My IP address is", wifi.radio.ipv4_address)

ipv4 = "%s" % (wifi.radio.ipv4_address)

write_text(display, "NT:" + secrets["ssid"], 0, 0)
write_text(display, "IP:" + ipv4, 0, 1)

pool = socketpool.SocketPool(wifi.radio)
newsocket = pool.socket(socketpool.SocketPool.AF_INET, socketpool.SocketPool.SOCK_STREAM)


commandStation = withrottle.WiThrottleConnection(pool)
commandStation.connect("192.168.7.1", 12090)

print(microcontroller.cpu.frequency)
print(microcontroller.cpu.temperature)

throttles = { }
baseAddress = 0xD0

loopCnt = 0
lastLoopUpdate = time.monotonic()

while True:
  loopCnt += 1
  pkt = mrbee.getpkt()

  curTime = time.monotonic()
  if curTime > lastLoopUpdate + 5:
    print("%d lps" % (loopCnt/(curTime - lastLoopUpdate)))
    lastLoopUpdate = curTime
    loopCnt = 0

  if pkt is None:
    continue

  if pkt is not None:
    #print("Processing %f ms, %f s since last pkt" % ((later-now) * 1000.0, later-lastpkt))
    lastpkt = time.monotonic()
    print(pkt)

  if pkt.src == baseAddress:
    print("Conflicting ProtoThrottle base station detected!!!\nTurning Error LED on\n")
    errorLightOn = True

  # Bypass anything that doesn't look like a throttle packet
  if pkt.cmd != 0x53 or len(pkt.data) != 9 or baseAddress != pkt.dest:
    continue


  # Create a MRBusThrottle object for every new Protothrottle that shows up
  if pkt.src not in throttles:
    throttles[pkt.src] = MRBusThrottle.MRBusThrottle(pkt.src)
    print("PT-BRIDGE: Processing packet from throttle %02X" % (pkt.src))

  throttles[pkt.src].update(commandStation, pkt)
#  print("PT-BRIDGE: Done processing packet from throttle %02X" % (pkt.src))

  commandStation.update()

    
