import time
import re

import board
import digitalio
import busio
import mrbus
import microcontroller
import ipaddress
import wifi
import socketpool

import withrottle
import esu
import MRBusThrottle
from switches import Switches
from screen import Screen
from systemstate import SystemState

import gc

gc.collect()
print("Initial memory %d" % (gc.mem_free()))

debug = False

uart = busio.UART(board.IO17, board.IO18, baudrate=115200, timeout=0, receiver_buffer_size = 1024)
i2c = busio.I2C(board.SCL, board.SDA, frequency=400000)
dipSwitches = Switches()
mrbee = mrbus.mrbus(0x03, uart)

screen = Screen()
screen.init(i2c)

screen.splash_screen()
splashStart = time.monotonic()

while time.monotonic() - 1 < splashStart:
  pass

sysState = SystemState()
sysState.readConfigurationFile("/config.txt")

if not sysState.configurationValid:
  screen.clearScreen()
#     000000000011111111112
#     012345678901234567890
#     Configuration Invalid
  screen.writeBufferedText("Configuration Invalid", 0, 0)
  screen.writeBufferedText(sysState.configurationError[0:22], 0, 1)
  screen.writeBufferedText(sysState.configurationError[22:43], 0, 1)
  screen.writeBufferedText(sysState.configurationError[43:64], 0, 1)
  screen.flushBufferedText()
  while True:
    pass

screen.writeText('C', 7, 3)

wifi.radio.hostname = sysState.hostName

if sysState.isAutoNetwork:
  sysState.scanForNetworks()

screen.writeText('N', 8, 3)

while time.monotonic() - 5 < splashStart:
  pass


lastpkt = time.monotonic()

print("My MAC addr:", [hex(i) for i in wifi.radio.mac_address])


#print("Connecting to %s"%secrets["ssid"])
#wifi.radio.connect(secrets["ssid"], secrets["password"])
#print("Connected to %s!"%secrets["ssid"])
#print("My IP address is", wifi.radio.ipv4_address)

#ipv4 = "%s" % (wifi.radio.ipv4_address)

#write_text(display, "NT:" + secrets["ssid"], 0, 0)
#write_text(display, "IP:" + ipv4, 0, 1)


loopCnt = 0
displayUpdateTime = 0
throttles = { }
lastLoopUpdate = time.monotonic() - 2
lastStatusTime = time.monotonic() - 2
screen.clearScreen()

gitver = [ 0x12, 0x34, 0x56 ]

def serverFind(pool, timeout, port):
  """Given a port, this searches the local class C subnet for anything with that port open."""
  conn = pool.socket(socketpool.SocketPool.AF_INET, socketpool.SocketPool.SOCK_STREAM)
  (o1,o2,o3,o4) = ("%s" % sysState.ipv4).split('.')

  print("Starting server scan on subnet (%s.%s.%s.255)" % (o1, o2, o3))
  for i in range(1,255):
    scanIP = "%s.%s.%s.%d" % (o1, o2, o3, i)
    conn.settimeout(timeout)
    try:
      print("Trying %s:%d" % (scanIP, port))
      conn.connect((scanIP, port))
      return (scanIP, conn)
    except Exception as e:
      time.sleep(0.1)
      print(e)

  return None

def rxtx(conn):
  recvBuffer = bytearray(256)
  # If we have a command, send it
  cmdStr = "get(1,info)"
  print("ESU TX: Sending [%s]" % (cmdStr))
  conn.send(str.encode(cmdStr))
  
  while True:
    try:
      bytesReceived = conn.recv_into(recvBuffer, len(recvBuffer))
      print("ESU RX: %d [%s]" % (bytesReceived, recvBuffer[0:bytesReceived].decode()))
    except Exception as e:
      print("RXTX: ", e)
      time.sleep(0.2)

while True:
  loopCnt += 1
  currentTime = time.monotonic()
  
  showTimeDiag = False
  # Housekeeping Tasks - Run on a Schedule
  
  # Every second, update the screen
  if sysState.forceScreenUpdate or currentTime > lastLoopUpdate + 1:
    sysState.lps = loopCnt / (currentTime - lastLoopUpdate)
    sysState.throttlesConnected = len(throttles)
    lastLoopUpdate = currentTime

    if wifi.radio.ap_info is not None:
      sysState.rssi = wifi.radio.ap_info.rssi
    sysState.baseAddr = dipSwitches.getBaseAddr()
    mrbee.addr = sysState.getMrbusBaseAddr()
    
    screen.status_screen(sysState)
    showTimeDiag = False
    loopCnt = 0
    sysState.forceScreenUpdate = False

  # Send periodic version packets so throttles see us, even if we aren't useful
  if currentTime > lastStatusTime + sysState.statusBroadcastIntervalSeconds:
    if debug:
      print("PT-BRIDGE: Sending status packet")
    statusPacket = [ ord('v'), 0x80, gitver[2], gitver[1], gitver[0], 1, 0 ] + sysState.getInterfaceVerPktText()
    mrbee.sendpkt(0xFF, statusPacket)
    lastStatusTime = currentTime

  if showTimeDiag:
    loopStartTime = time.monotonic_ns()

  # If we don't have a wifi connection, the first order of business is to get one
  if not sysState.isWifiConnected:
    sysState.isCmdStnConnected = False

    if sysState.isAutoNetwork and sysState.networkSSID is None:
      sysState.scanForNetworks()

    if sysState.networkSSID is None:
      continue

    if debug:
      print("My MAC addr:", [hex(i) for i in wifi.radio.mac_address])
      print("Connecting to [%s] - pass [%s]" % (sysState.networkSSID, sysState.networkPassword))
    
    try:
      if sysState.networkPassword == "":
        wifi.radio.connect(sysState.networkSSID)
      else:
        wifi.radio.connect(sysState.networkSSID, sysState.networkPassword)
#      wifi.radio.connect(sysState.networkSSID, sysState.networkPassword)
      pool = socketpool.SocketPool(wifi.radio)
      print("My IP address is", wifi.radio.ipv4_address)
      print("My gateway is", wifi.radio.ipv4_gateway)

      pingTime = wifi.radio.ping(wifi.radio.ipv4_gateway)
      if None != pingTime:
        print("ping is %d" % (pingTime))

      sysState.ipv4 = wifi.radio.ipv4_address
      sysState.ipv4_gateway = wifi.radio.ipv4_gateway
      sysState.isWifiConnected = True

    except Exception as e:
      print("Connect loop:", e)
      time.sleep(0.1)
      sysState.isWifiConnected = False

    # Head to the top of the loop to get the screen updates
    continue

  # At this point, wifi is good, see if we need to build the command station connection

  if not sysState.isCmdStnConnected:
    # FIXME: Do an IP scan if we don't have an IP
    openSocket = None
    cmdStationIP = sysState.cmdStationIP
    cmdStationPort = sysState.cmdStationPort

    if sysState.cmdStationType in ['lnwi', 'withrottle'] and cmdStationPort is None:
      cmdStationPort = 12090
    elif sysState.cmdStationType in ['esu'] and cmdStationPort is None:
      cmdStationPort = 15471

    if cmdStationIP is None:
      # Go figure it out
      if sysState.cmdStationType == 'lnwi':
        # LNWIs are always on x.x.x.1, usually 192.168.(channel).1
        (o1, o2, o3, o4) = ("%s" % sysState.ipv4).split('.')
        cmdStationIP = "%s.%s.%s.1" % (o1, o2, o3)
      else:
        # Gotta go scan the subnet
        (cmdStationIP, openSocket) = serverFind(pool, 0.2, cmdStationPort)

    print("cmdStationIP = %s:%d" % (cmdStationIP, cmdStationPort))
    if cmdStationIP is None:
      continue

    if sysState.cmdStationType == 'lnwi':
      try:
        commandStation = withrottle.WiThrottleConnection(pool)
        commandStation.connect(cmdStationIP, cmdStationPort, "LNWI", conn=openSocket)
        while uart.read(1024) is not None: # Flush RX buffer
          pass

        sysState.isCmdStnConnected = True
      except Exception as e:
        print(e)
        try:
          commandStation.disconnect()
        except Exception as e:
          pass
        
        # If EHOSTUNREACH or something, we probably don't have a network
        time.sleep(1)
        sysState.isWifiConnected = False
        continue
    
    elif sysState.cmdStationType == 'withrottle':
      try:
        commandStation = withrottle.WiThrottleConnection(pool)
        commandStation.connect(cmdStationIP, cmdStationPort, "JMRI", conn=openSocket)
        while uart.read(1024) is not None: # Flush RX buffer
          pass

        sysState.isCmdStnConnected = True
      except:
        continue

    elif sysState.cmdStationType == 'esu':
      try:
        commandStation = esu.ESUConnection(pool)
        commandStation.connect(cmdStationIP, cmdStationPort, conn=openSocket)
        while uart.read(1024) is not None: # Flush RX buffer
          pass

        sysState.isCmdStnConnected = True
      except Exception as e:
        print(e)
        continue
      while uart.read(1024) is not None: # Flush RX buffer
        pass

    if not sysState.isCmdStnConnected:
      continue # No point going on without a command station

  try:
    commandStation.update()
  except OSError as e:
    # Probably lost connection, reset command station
    print("Command station update exception: ", e)
    commandStation.disconnect()
    sysState.isCmdStnConnected = False
    # Write something to the screen here
    sysState.forceScreenUpdate = True
    continue


  pkt = mrbee.getpkt()
  
  if pkt is not None:
    #print("Got packet from [0x%02X]" % (pkt.src))
    if pkt.src == sysState.getMrbusBaseAddr():
      print("Conflicting ProtoThrottle base station detected!!!\nTurning Error LED on\n")
      errorLightOn = True

    # If this looks like a throttle packet, do something with it
    if pkt.cmd == 0x53 and len(pkt.data) == 9 and sysState.getMrbusBaseAddr() == pkt.dest:
      # Create a MRBusThrottle object for every new Protothrottle that shows up
      if pkt.src not in throttles:
        throttles[pkt.src] = MRBusThrottle.MRBusThrottle(pkt.src)
        print("PT-BRIDGE: Adding throttle %02X" % (pkt.src))

      try:
        throttles[pkt.src].update(commandStation, pkt)
      except OSError as e:
        # Probably lost connection, reset command station
        print("Command station update exception: ", e)
        commandStation.disconnect()
        sysState.isCmdStnConnected = False
        sysState.forceScreenUpdate = True
        continue
      except Exception as e:
        # Non-network exception, don't reset the command station
        print("Command station update exception, non-network: ", e)
        continue

      if debug:
        print("PT-BRIDGE: Done processing packet from throttle %02X" % (pkt.src))

  ## Clean up throttles that haven't sent a packet in a while
  throttlesToDelete = [ ]

  for (key,throttle) in throttles.items():
    updateTime = throttle.getLastUpdateTime()
    if (updateTime + sysState.throttleTimeoutSeconds) < currentTime:
      print("Throttle address 0x%02X has timed out, removing" % key)
      throttles[key].disconnect(commandStation)
      throttlesToDelete.append(key)
      print("Throttle disconnected")

  for key in throttlesToDelete:
    del throttles[key]

