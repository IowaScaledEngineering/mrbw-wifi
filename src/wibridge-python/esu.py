# *************************************************************************
# Title:    Client for ESU CabControl DCC System Network Interface
# Authors:  Michael D. Petersen <railfan@drgw.net>
#           Nathan D. Holmes <maverick@drgw.net>
# File:     esu.py
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
#   This class provides a way to interface with an ESU CabControl system
#   in order to provide basic functionality, like acquiring locomotives and
#   then setting speed, direction, and function outputs.
# 
#   Many thanks to ESU for providing the protocol documentation that allowed
#   this to be developed.  Thankfully, while I don't speak German, Google 
#   Translate does rather well.
# 
# *************************************************************************

import socketpool
import re
import time
import supervisor

class ESUConnection:
  """An interface to talk to an ESU CabControl command station via the network in order to
     control model railway locomotives via DCC or other supported protocols."""
  # Some pre-compiled regexs used in response parsing
  
  #RElocAdd = re.compile("10\s+id\[(?P<objID>\d+)\].*")
  # REend = re.compile("<END (?P<errCd>\d+) \((?P<errStr>[a-zA-Z0-9 ]+)\)>.*") 
  # REend = re.compile("<.*") #END.*")# (?P<errCd>\d+) .*") #\((?P<errStr>[a-zA-Z0-9 ]+)\)>.*") 
   
  def __init__(self, socketpool, debug = False):
    """Constructor for the object.  Any internal initialization should occur here."""
    self.conn = None
    self.socketpool = socketpool
    self.debug = True
    self.ip = None
    self.port = 15471
    self.recvData = ""
    self.lastUpdate = 0
    self.endMatch = re.compile(r'<END (\d+) (A-Za-z0-9_)+>')
    self.REglobalList = re.compile("(\d+)\s+addr\[(\d+)\].*")
    self.RElocAdd = re.compile("10\s+id\[(\d+)\].*")
    self.REfuncGet = re.compile("(\d+)\s+func\[(\d+),(\d+)\].*")
    
  def connect(self, ip, port = 15471, conn = None):
    """Connect this object to an ESU CabControl command station on the IP address specified."""
    if port is not None:
      self.port = port
    self.ip = ip
      

    if conn is not None:
      print("ESU Command station connection reusing scan port")
      self.conn = conn
    else:
      if self.debug:
        print("ESU Trying to connect to %s on port %d" % (ip, port))
      self.conn = self.socketpool.socket(socketpool.SocketPool.AF_INET, socketpool.SocketPool.SOCK_STREAM)
      self.conn.settimeout(5)
      self.conn.connect((self.ip, self.port))

    self.conn.settimeout(0)
    self.recvData = ""
    self.lastUpdate = time.monotonic()
    
    (errCode, errTxt, results) = self.rxtx("get(1, info)")
    if 0 == errCode:
      print("Command station version info:\n%s" % (results))

    print("ESU Command station connection succeeded on %s:%d" % (self.ip, self.port))

  def disconnect(self):
    """Disconnect from the CabControl command station in a clean way."""
    print("ESU Disconnecting")
    try:
      self.conn.close()
      print("ESU Command station connection closed successfully")
    except:
      print("ESU Command station connection closed with exception, ignoring")
    self.conn = None
    self.recvData = ""

  def rxtx(self, cmdStr=None, timeout=1000):
    # No connection?  Can't do much here
    recvBuffer = bytearray(256)

    # If we have a command, send it
    if cmdStr is not None:
      cmdStr = cmdStr.strip()
      self.lastUpdate = time.monotonic()
      if self.debug:
        print("ESU TX: Sending [%s]" % (cmdStr))
      self.conn.send(str.encode(cmdStr))
      replyStart = "<REPLY %s>" % (cmdStr)

    haveReply = False
    timedOut = False
    replyLines = None
    errCode = 999
    errTxt = "ERROR"
    inReply = False

    rxStart = supervisor.ticks_ms()

    if cmdStr is None:
      try:
        bytesReceived = self.conn.recv_into(recvBuffer, len(recvBuffer))
        if bytesReceived > 0:
          self.recvData += recvBuffer[0:bytesReceived].decode()
      except OSError as e:
        if e.args[0] not in [ errno.EAGAIN, errno.ETIMEDOUT ]:
          raise e
          print("RXTX: " + str(e))
      return

    while not haveReply and not timedOut:
      if (supervisor.ticks_ms()) > (rxStart + timeout):
        timedOut = True
        if self.debug:
          print("ESU RX Timeout")

      try:
        try:
          bytesReceived = self.conn.recv_into(recvBuffer, len(recvBuffer))
          
        except OSError as e:
          if e.args[0] not in [ errno.EAGAIN, errno.ETIMEDOUT ]:
            raise e
            print("RXTX: " + str(e))
          continue # The "safe" errors

        if bytesReceived > 0:
          self.recvData += recvBuffer[0:bytesReceived].decode()
          #print("ESU RX: recvBuffer(%d)=[%s]" % (bytesReceived, recvBuffer[0:bytesReceived].decode()))
          #print("ESU RX: recvData=[%s]" % (self.recvData))
          # Split received data into lines
          lines = self.recvData.split('\n')
          if not lines[-1].endswith('\n'):
            extraData = lines.pop()
          else:
            extraData = ""
          extraLines = []

          for line in lines:
            #print("ESU RX: Line [%s]" % (line))
          
            line = line.strip()
            if haveReply:
              extraLines.append(line)

            elif not inReply:
              if line == replyStart:
                #print("Found starting line [%s]" % (line))
                replyLines = []
                inReply = True
              else:
                print("Unsolicited line [%s]" % (line))
            else:
              if len(line) > 10 and line[0:4] == "<END":
                (e1, errCode, errTxt) = line[1:-1].split(' ')
                #print("Found end line [%s]" % (line))
                haveReply = True
              else:
                replyLines.append(line)

          if haveReply:
            self.recvData = '\n'.join(extraLines) + extraData
          # Otherwise, recvData still has a bunch of stuff in it

      except Exception as e:
        #print(e)
        #time.sleep(0.001)
        pass # FIXME - Maybe handle connection errors differently?

    
    #print("ESU rxtx_new end haveReply=%s timedOut=%s [%s]" % (haveReply, timedOut, replyLines))
    
    return (int(errCode), errTxt, replyLines)

  def locomotiveAdd(self, locAddr, locoName=""):
    """Internal function for adding a locomotive to the command station's object table."""
    print("ESU: Adding locomotive address [%d]" % (locAddr))
    cmdStr = "create(10, addr[%d], append)" % ( locAddr )
    #FIXME result = self.rxtx(cmdStr, self.RElocAdd)
    (errCode, errTxt, results) = self.rxtx(cmdStr)
    
    if 0 != errCode:
      # Eh, now what?
      raise Exception("Bad command station reply on esuLocomotiveAdd <%d %s>" % (errCode, errTxt))
    
    m = self.RElocAdd.match(results[0])
    objID = int(m.group(1)) 
    print("ESU: Locomotive [%d] added at objID [%d]" % (locAddr, objID))
    return objID

  def locomotiveObjectGet(self, locAddr, cabID, isLongAddress):
    """Acquires and returns a handle that will be used to control a locomotive address."""
    print(("ESU: locomotiveObjectGet(%d, 0x%02X)" % (locAddr, cabID)))
    cmdStr = "queryObjects(10,addr)"
    (errCode, errTxt, results) = self.rxtx(cmdStr)

    if 0 != errCode:
      # Eh, now what?
      raise Exception("Bad command station reply on locomotiveObjectGet <%d %s>" % (errCode, errTxt))

#    print("Got reply %d [%s]\n%s" % (errCode, errTxt, results))

    for line in results:
      m = self.REglobalList.match(line)
      if m is None:
        print("Line [%s] does not match REglobalList" % (line))
        continue
      if int(m.group(2)) == locAddr:
        objID = int(m.group(1))
        print("ESU: Found locomotive %s at object %d" % (locAddr, objID))
        return objID

    print("ESU: Need to add this locomotive")
    objID = self.locomotiveAdd(locAddr)
    
    cmdStr = "request(%d, control, force)" % (objID)
    self.rxtx(cmdStr)
    return objID

  def locomotiveEmergencyStop(self, objID):
    """Issues an emergency stop command to a locomotive handle that has been previously acquired with locomotiveObjectGet()."""
    objID = int(objID)
    cmdStr = "set (%d, stop)" % (objID)
    self.rxtx(cmdStr)

  # For the purposes of this function, direction of 0=forward, 1=reverse
  def locomotiveSpeedSet(self, objID, speed, direction=0):
    """Sets the speed and direction of a locomotive via a handle that has been previously acquired with locomotiveObjectGet().  
       Speed is 0-127, Direction is 0=forward, 1=reverse."""
    objID = int(objID)
    speed = int(speed)
    direction = int(direction)
      
    if direction != 0 and direction != 1:
      speed = 0
      direction = 0
      
    if speed >= 127 or speed < 0:
      speed = 0

    cmdStr = "set(%d, speed[%d], dir[%d])" % (objID, speed, direction)
    print("ESU: locomotiveSpeedSet(%d): set speed %d %s" % (objID, speed, ["FWD","REV"][direction]))
    self.rxtx(cmdStr)
    print("ESU: locomotiveSpeedSet complete")
   
  def locomotiveFunctionSet(self, objID, funcNum, funcVal):
    """Sets or clears a function on a locomotive via a handle that has been previously acquired with locomotiveObjectGet().  
       funcNum is 0-28 for DCC, funcVal is 0 or 1."""
    objID = int(objID)
    funcNum = int(funcNum)
    funcVal = int(funcVal)
    print("ESU: object %d set function %d to %d" % (int(objID), funcNum, funcVal))
    cmdStr = "set(%d, func[%d,%d])" % (objID, funcNum, funcVal)
    (errCode, errTxt, results) = self.rxtx(cmdStr)
    if 0 == errCode:
      print("ESU: function set complete")
    else:
      print("ESU: function set failed")

  def locomotiveFunctionDictSet(self, objID, funcDict):
    """Don't use this!  An effort to set multiple functions at a time that doesn't really work yet."""
    objID = int(objID)
    funcStr = ""
    for funcNum in funcDict:
      funcNum = int(funcNum)
      funcVal = int(funcDict[funcNum])
      funcStr = funcStr + ", func[%d,%d]" % (funcNum, funcVal)
     
    cmdStr = "set(%d%s])" % (objID, funcStr)
    self.rxtx(cmdStr)
      
    print("ESU locomotiveFunctionSet(%d): set func %d to %d" % (int(objID), funcNum, funcVal))

  def locomotiveDisconnect(self, objID):
    print("ESU locomotiveDisconnect(%d): disconnect" % (int(objID)))
    cmdStr = "release(%d, control)" % (objID)
    self.rxtx(cmdStr)
 
  def locomotiveFunctionsGet(self, objID):
    print("ESU locomotiveFunctionsGet(%d)" % (int(objID)))
    retFuncs = [0] * 29

    #for funcNum in range (0,29):
    #  cmdStr = "get(%d, func[%d])" % (objID, funcNum)
    #  (errCode, errTxt, results) = self.rxtx(cmdStr)
    #  if 0 == errCode:
    #    m = self.REfuncGet.match(results[0])
    #    if m is not None and int(m.group(2)) != 0:
    #      retFuncs[funcNum] = 1
    #      print("ESU locomotiveFunctionsGet(%d) func %d=1" % (objID, funcNum))
    #  else:
    #    print("ESU locomotiveFunctionsGet(%d) no rx for func %d" % (objID, funcNum))

    return retFuncs

  def update(self):
    """This should be called frequently within the main program loop.  While it doesn't do anything for ESU,
       other command station interfaces have housekeeping work that needs to be done periodically."""
    if time.monotonic() > self.lastUpdate + 10:
      (errCode, errTxt, results) = self.rxtx("get(1, status)")
      if 0 == errCode:
        print("ESU: ping successful")
      else:
        print("ESU: ping failed")
