# *************************************************************************
# Title:    Client for WiThrottle-based Clients (JMRI, Digitrax LNWI, MRC Wifi)
# Authors:  Nathan D. Holmes <maverick@drgw.net>
#           Michael D. Petersen <railfan@drgw.net>
# File:     withrottle.py
# License:  GNU General Public License v3
# 
# LICENSE:
#   Copyright (C) 2018 Michael Petersen & Nathan Holmes
#     
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 3 of the License, or
#   any later version.
# 
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
# DESCRIPTION:
#   This class provides a client to connect to clients that speak the
#   JMRI Wifi Throttle (WiThrottle) protocol and close derivatives (Digitrax LNWI)
# 
# *************************************************************************

import socketpool
import time

class WiThrottleConnection:
  """A client object to talk to a JMRI WiFi Throttle server or compatible.  
    This class is capable of handling multiple locomotives simultaneously via
    the multithrottle interface and its ability to multiplex throttles."""

  def __init__(self, socketpool):
    """Constructor for the object.  Any internal initialization should occur here."""
    self.socketpool = socketpool
    self.debug = False
    self.conn = None
    self.activeThrottles = { }
    self.funcStatus = { }
    self.funcUpdated = { }
    self.lastUpdate = 0
    self.recvData = ""
    self.ip = None
    self.port = None
    self.operatingMode = "JMRI"
    self.version = ""
    self.trackPowerOn = False
    self.heartbeatMaxInterval = 10
    self.serverName = ""
    self.serverID = ""


  def connect(self, ip, port, mode="JMRI", conn=None):
    """Since the LNWI only understands a subset of Multithrottle commands, open up a single connection
    to multiplex everything through."""
    if mode == "JMRI":
      self.operatingMode = "JMRI"
    elif mode == "LNWI":
      self.operatingMode = "LNWI"
    else:
      print("Operating Mode [%s] not understood, defaulting to JMRI" % (mode))
      self.operatingMode = "JMRI"

    print("%s Connect: Connecting to server [%s] port [%d]" % (self.operatingMode, ip, port))
    self.ip = ip
    self.port = port

    if conn is not None:
      self.conn = conn
      self.conn.settimeout(0)
    else:
      self.conn = self.socketpool.socket(socketpool.SocketPool.AF_INET, socketpool.SocketPool.SOCK_STREAM)
      """Set the socket timeout relatively large for the initial connect since they take awhile"""
      self.conn.settimeout(5)
      self.conn.connect((self.ip, self.port))
      self.conn.settimeout(0)

    self.recvData = ""

    self.rxtx("NProtoThrottle Bridge\n")
    self.rxtx("HUProtoThrottle Bridge\n")
    self.activeThrottles = { }
    print("%s Connect: complete" % (self.operatingMode))
    """Set the timeout for the socket r/w operations small to prevent blocking too long on receives."""
    self.conn.settimeout(0)


  def disconnect(self):
    print("%s Disconnect: Shutting down %s interface\n" % (self.operatingMode, self.operatingMode))
    """Shut down all throttle socket connections and disconnect from the WiThrottle server in a clean way."""
    for cabID,mtID in self.activeThrottles.items():
      try:
        self.rxtx("M%1.1s-*<;>r\n" % (mtID))
      except:
        if self.debug:
          print("In disconnect, write fails, socket must be dead")
        time.sleep(0.1)

    try:
      self.rxtx("Q\n")
    except: 
      print("Socket probably went away rudely")

    try:
      self.conn.close()
    except Exception as e: 
      print("Socket close error", e)

    self.activeThrottles = { }
    self.recvData = ""
    print("%s Disconnect: Disconnected" % (self.operatingMode))

  def rxtx(self, cmdStr):
    """Internal shared function for transacting with the WiThrottle server."""
    if cmdStr is not None:
      self.lastUpdate = time.monotonic()
      if self.debug:
        print("%s TX: Sending [%s]" % (self.operatingMode, cmdStr[:-1]))
      self.conn.send(str.encode(cmdStr))
      #time.sleep(0.05)
    try:
      recvBuffer = bytearray(256)
      bytesReceived = self.conn.recv_into(recvBuffer, len(recvBuffer))
      if bytesReceived > 0:
        self.recvData += recvBuffer[0:bytesReceived].decode()
        self.parseIncomingData()
    except:
      pass



  def parseIncomingData(self):
    # If there's no carriage returns, we don't have a complete response of any sort yet
    if '\n' not in self.recvData:
      return

    responseStrings = self.recvData.split('\n')

    # If there's trailing unfinished data, put it back in the recieve data queue, otherwise clear it
    if not self.recvData.endswith('\n'):
      self.recvData = responseStrings.pop()
    else:
      self.recvData = ""

    for resp in responseStrings:
      # Trim whitespace
      resp = resp.strip()

      # No length?  Nothing to do
      if len(resp) == 0:
        continue
      if self.debug:
        print(resp)

      if ('VN' == resp[0:2]):  # Protocol version
        self.version = resp[2:]

      elif ('RL' == resp[0:2]):  # Roster List, don't care right now
        pass

      elif ('PPA' == resp[0:3]):  # Track Power
        if resp[3:4] == '1': # Track power on
          self.trackPowerOn = True
        elif resp[3:4] == '0':  # Track power off
          self.trackPowerOn = False
        elif resp[3:4] == '2': # Track power unknown - assume the best, on...
          self.trackPowerOn = True

      elif ('PT' == resp[0:2]):  # Turnout lists, don't care right now
        pass

      elif ('PR' == resp[0:2]):  # Route lists, don't care right now
        pass

      elif ('*' == resp[0:1]):  # Heartbeat interval
        try:
          self.heartbeatMaxInterval = int(resp[1:])
        except:
          self.heartbeatMaxInterval = 10

      elif ('N' == resp[0:1]):  # Host controller name
        self.serverName = resp[1:]

      elif ('U' == resp[0:1]):  # Host controller name
        self.serverID = resp[1:]

      elif ('M' == resp[0:1]):  # Some sort of multithrottle response - parse this
        if self.debug:
          print("%s RX: Multithrottle update [%s]" % (self.operatingMode, resp))

        try:
          (throttle,cmd) = resp.split("<;>")
          if throttle[2:3] == 'S':
            # we've asked for somebody else's loco - steal it!
            # The format of this undocumented command appears to be:
            # MTSLxxxx<;>Lxxxx
            # And the response to steal it is the same
            print("%s RX: Cab [%s] needs to steal locomotive [%s]\n" % (self.operatingMode, throttle[1:2], cmd))
            cmdStr = resp
            self.conn.send(cmdStr)
                  
          elif throttle[2:3] == "A":
            if cmd[0:1] == 'F':
              funcNum = int(cmd[2:])
              funcVal = int(cmd[1:2])
              self.funcStatus[throttle[1:2]][funcNum] = funcVal
              if self.debug:
                print("%s RX: Cab [%s] set func %d to %d " % (self.operatingMode, throttle[1:2], funcNum, funcVal))
              if funcNum == 28:
                self.funcUpdated[throttle[1:2]] = True
        except:
          print("%s RX: Multithrottle packet exception" % (self.operatingMode))
               
      else:
        print("%s RX: Unknown host->client [%s]\n" % (self.operatingMode, resp))

  def update(self):
    """This should be called frequently within the main program loop.  This implements the keepalive heartbeat
       within the WiThrottle protocol."""
    heartbeatInterval = (self.heartbeatMaxInterval / 2)
    if heartbeatInterval < 1:
      heartbeatInterval = 1

    if time.monotonic() > self.lastUpdate + heartbeatInterval:
      self.rxtx("*\n")
    else:
      self.rxtx(None)

  def getAvailableMultithrottleLetter(self):
    mtLetters = set('ABCDEFGHIJKLMNOPQRSTUVWXYZ012345')
    usedMTLetters = set(self.activeThrottles.values())
    mtLetters = mtLetters.difference(usedMTLetters)
    return mtLetters.pop()

  def locomotiveDisconnect(self, objID):
    print("%s locomotiveDisconnect(%d): disconnect" % (self.operatingMode, objID['locoNum']))
    self.rxtx("M%1.1s-*<;>r\n" % (self.activeThrottles[objID['addr']]))
    del self.activeThrottles[objID['addr']]

  def locomotiveFunctionsGet(self, objID):
    if self.debug:
      print("%s locomotiveFunctionsGet(%d)" % (self.operatingMode, objID['locoNum']))
    throttleLetter = self.activeThrottles[objID['addr']]
    return self.funcStatus[throttleLetter]
         
  def locomotiveEmergencyStop(self, objID):
    """Issues an emergency stop command to a locomotive handle that has been previously acquired with locomotiveObjectGet()."""
    if self.debug:
      print("%s locomotiveEmergencyStop(%d)" % (self.operatingMode, objID['locoNum']))
    self.rxtx("M%1.1sA*<;>X\n" % self.activeThrottles[objID['addr']])

  def locomotiveObjectGet(self, locoNum, cabID, isLongAddress=True):
    """Acquires and returns a handle that will be used to control a locomotive address.  This will release
       any locomotive that cabID was previously controlling."""
    if self.debug:
      print("%s locomotiveObjectGet(%d, 0x%02X)" % (self.operatingMode, locoNum, cabID))

    if cabID not in self.activeThrottles:
      newThrottleLetter = self.getAvailableMultithrottleLetter()
      self.activeThrottles[cabID] = newThrottleLetter
      print("%s locomotiveObjectGet: Added throttle letter [%s] for PT cab 0x%02X (loco %d)" % (self.operatingMode, newThrottleLetter, cabID, locoNum))

    objID = {'addr':cabID, 'locoNum':locoNum, 'isLong':isLongAddress }

    self.funcStatus[self.activeThrottles[cabID]] = [0] * 29  # Array of 29 zeros for function status
    self.funcUpdated[self.activeThrottles[cabID]] = False
      
    #Drop anything this cab might have had before.  If nothing, no harm
    self.rxtx("M%1.1s-*<;>r\n" % (self.activeThrottles[objID['addr']]))

    if objID['isLong']:
      # Acquire new locomotive at long address
      self.rxtx("M%1.1s+L%d<;>L%d\n" % (self.activeThrottles[objID['addr']], objID['locoNum'], objID['locoNum']))
    else:
      self.rxtx("M%1.1s+S%d<;>S%d\n" % (self.activeThrottles[objID['addr']], objID['locoNum'], objID['locoNum']))

    for i in range(0,50):
      self.rxtx(None)
      # Check if we've gotten the function statuses from the command station yet
      if self.funcUpdated[self.activeThrottles[cabID]] is True:
        if self.debug:
          print("%s locomotiveObjectGet: Got func status for [%d] from LNWI" % (self.operatingMode, locoNum))
        break
      #time.sleep(0.01)

    # Turn on track power if it's off for some reason
    if self.trackPowerOn == False:
      self.rxtx("PPA1\n")

    return objID


  # For the purposes of this function, direction of 0=forward, 1=reverse
  def locomotiveSpeedSet(self, objID, speed, direction=0):
    """Sets the speed and direction of a locomotive via a handle that has been previously acquired with locomotiveObjectGet().  
       Speed is 0-127, Direction is 0=forward, 1=reverse."""
    speed = int(speed)
    direction = int(direction)

    if self.debug:
      print("%s locomotiveSpeedSet(%d): set speed %d %s" % (self.operatingMode, objID['locoNum'], speed, ["FWD","REV"][direction]))
      
    if direction != 0 and direction != 1:
      speed = 0
      direction = 0
      
    if speed >= 127 or speed < 0:
      speed = 0

    self.rxtx("M%1.1sA*<;>V%d\n" % (self.activeThrottles[objID['addr']], speed))
    # Direction is 0=REV, 1=FWD on WiThrottle
    self.rxtx("M%1.1sA*<;>R%d\n" % (self.activeThrottles[objID['addr']], [1,0][direction]))
   
  def locomotiveFunctionSet(self, objID, funcNum, funcVal):
    if self.operatingMode == "LNWI":
      self.locomotiveFunctionSetLNWI(objID, funcNum, funcVal)
    else:
      self.locomotiveFunctionSetJMRI(objID, funcNum, funcVal)

  def locomotiveFunctionSetJMRI(self, objID, funcNum, funcVal):
    funcNum = int(funcNum)
    funcVal = int(funcVal)

    # Thankfully, JMRI supports the "force function" ('f') command as described in the spec
    # so we can avoid all the nasties as we have in LNWI mode
    print("JMRI locomotiveFunctionSet(%d): set func %d to %d" % (objID['locoNum'], funcNum, funcVal))
    self.rxtx("M%1.1sA*<;>f%d%d\n" % (self.activeThrottles[objID['addr']], funcVal, funcNum))   

  def locomotiveFunctionSetLNWI(self, objID, funcNum, funcVal):
    """Sets or clears a function on a locomotive via a handle that has been previously acquired with locomotiveObjectGet().  
       funcNum is 0-28 for DCC, funcVal is 0 or 1."""

    # This is the nasty part.  The LNWI doesn't support the "force function" ('f') command, so we have to do 
    # weird crap here to actually get the function in the state we want.

    funcNum = int(funcNum)
    funcVal = int(funcVal)

    if self.debug:
      print("LNWI locomotiveFunctionSet(%d): set func %d to %d" % (objID['locoNum'], funcNum, funcVal))
 
    if funcNum == 2:  # 2 is non-latching, all others are latching
       self.rxtx("M%1.1sA*<;>F%d%d\n" % (self.activeThrottles[objID['addr']], funcVal, funcNum))
    else:
       if self.funcStatus[ self.activeThrottles[ objID['addr'] ] ] [funcNum] != funcVal:
         self.rxtx("M%1.1sA*<;>F1%d\n" % (self.activeThrottles[objID['addr']], funcNum) )
         self.rxtx("M%1.1sA*<;>F0%d\n" % (self.activeThrottles[objID['addr']], funcNum) )
