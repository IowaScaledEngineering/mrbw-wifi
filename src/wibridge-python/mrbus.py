class packet(object):
  def __init__(self, dest, src, cmd, data):
    self.dest=dest
    self.src=src
    self.cmd=cmd
    self.data=data

  def __hash__(self):
    return hash(repr(self))

  def __eq__(self, other):
    return repr(self)==repr(other)

  def __repr__(self):
    return "mrbus.packet(0x%02x, 0x%02x, 0x%02x, %s)"%(self.dest, self.src, self.cmd, repr(self.data))

  def __str__(self):
    c='(0x%02X'%self.cmd
    if self.cmd >= 32 and self.cmd <= 127:
      c+=" '%c')"%self.cmd
    else:
      c+="    )"
    return "packet(0x%02X->0x%02X) %s %2d:%s"%(self.src, self.dest, c, len(self.data), ["0x%02X"%d for d in self.data])

def mrbusCRC16Update(crc, a):
   MRBus_CRC16_HighTable = [ 0x00, 0xA0, 0xE0, 0x40, 0x60, 0xC0, 0x80, 0x20, 0xC0, 0x60, 0x20, 0x80, 0xA0, 0x00, 0x40, 0xE0 ]
   MRBus_CRC16_LowTable =  [ 0x00, 0x01, 0x03, 0x02, 0x07, 0x06, 0x04, 0x05, 0x0E, 0x0F, 0x0D, 0x0C, 0x09, 0x08, 0x0A, 0x0B ]
   crc16_h = (crc>>8) & 0xFF
   crc16_l = crc & 0xFF
   
   i = 0
   
   while i < 2:
      if i != 0:
         w = ((crc16_h << 4) & 0xF0) | ((crc16_h >> 4) & 0x0F)
         t = (w ^ a) & 0x0F
      else:
         t = (crc16_h ^ a) & 0xF0
         t = ((t << 4) & 0xF0) | ((t >> 4) & 0x0F)
         
      crc16_h = (crc16_h << 4) & 0xFF
      crc16_h = crc16_h | (crc16_l >> 4)
      crc16_l = (crc16_l << 4) & 0xFF
      
      crc16_h = crc16_h ^ MRBus_CRC16_HighTable[t]
      crc16_l = crc16_l ^ MRBus_CRC16_LowTable[t]
      i = i + 1
      
   return (crc16_h<<8) | crc16_l

def mrbusCRC16Calculate(data):
   mrbusPktLen = data[2]
   crc = 0
   
   for i in range(0, mrbusPktLen):
      if i == 3 or i == 4:
         continue
      else:
         a = data[i]
      crc = mrbusCRC16Update(crc, a)
      
   return crc

class mrbus(object):
  def disconnect(self):
     self.mrbs.disconnect()

  def __init__(self, addr, serialPort):
    self.rxBuffer = []
    self.rxExcapeNext = False
    self.rxProcessing = False
    self.addr=addr
    self.pktlst=[]
    self.handlern=0
    self.handlers=[]
    self.serial = serialPort
    self.debug = False

  def getpkt(self):
    rxBuffer = self.rxBuffer
    rxExcapeNext = self.rxExcapeNext
    
  
    while True:
      incomingBytes = self.serial.read(1)
      if incomingBytes is None:
        break  # No packet to return

      # Convert our incoming byte to something useful - an int
      incomingByte = incomingBytes[0] #int.from_bytes(incomingByte, "big")

      if self.debug:
        print("mrbee got byte [0x%02x]" % incomingByte)

      if 0x7E == incomingByte:
        if self.debug:
          print("mrbee starting new packet")
        self.rxBuffer = [ incomingByte ]
        self.rxExcapeNext = False
        self.rxProcessing = True
        self.rxExpectedPktLen = 255
        
      elif 0x7D == incomingByte:
        if self.debug:
          print("mrbee setting escape")
        self.rxExcapeNext = True
     
      else:
        if not self.rxProcessing:
          if self.debug:
            print("mrbee got byte %02X outside processing, ignoring\n" % (incomingByte))
          continue
     
        if self.rxExcapeNext:
          incomingByte = incomingByte ^ 0x20
          self.rxExcapeNext = 0

        self.rxBuffer.append(incomingByte)
  
        if len(self.rxBuffer) == 3:
          self.rxExpectedPktLen = self.rxBuffer[1] * 256 + self.rxBuffer[2] + 4

        if len(self.rxBuffer) == self.rxExpectedPktLen:
          #self.logger.debug("mrbee - think we may have a packet")                 
          pktChecksum = 0
          for i in range(3, self.rxExpectedPktLen):
            pktChecksum = (pktChecksum + self.rxBuffer[i]) & 0xFF
                             
          if 0xFF != pktChecksum:
            # Error, conintue
            if self.debug:
              print("mrbee - checksum error - checksum is %02X" % (pktChecksum))

          pktDataOffset = 0

          if 0x80 == self.rxBuffer[3]:
            # 16 bit addressing
            pktDataOffset = 14
          elif 0x81 == self.rxBuffer[3]:
            # 64 bit addressing
            pktDataOffset = 8
          else:
            # Some other API frame, just dump it
            self.rxBuffer = [ ]
            self.rxExcapeNext = False
            self.rxProcessing = False
          
          if 0 != pktDataOffset:
            retPacket = packet(self.rxBuffer[pktDataOffset + 0], self.rxBuffer[pktDataOffset + 1], self.rxBuffer[pktDataOffset + 5], self.rxBuffer[(pktDataOffset + 6):-1])
            return retPacket

    return None


  def sendpkt(self, dest, data, src=None):
     if src == None:
        src = self.addr

     txPktLen = 10 + len(data)   # 5 MRBus overhead, 5 XBee, and the data

     txBuffer = [ 0x7E, 0x00, txPktLen, 0x01, 0x00, 0xFF, 0xFF, 0x00, dest, src, len(data) + 5, 0, 0 ] + data
#     txBuffer.append(0x7E)       # 0 - Start 
#     txBuffer.append(0x00)       # 1 - Len MSB
#     txBuffer.append(txPktLen)   # 2 - Len LSB
#     txBuffer.append(0x01)       # 3 - API being called - transmit by 16 bit address
#     txBuffer.append(0x00)       # 4 - Frame identifier
#     txBuffer.append(0xFF)       # 5 - MSB of dest address - broadcast 0xFFFF
#     txBuffer.append(0xFF)       # 6 - LSB of dest address - broadcast 0xFFFF
#     txBuffer.append(0x00)       # 7 - Transmit Options
     
#     txBuffer.append(dest)           # 8 / 0 - Destination
#     txBuffer.append(src)            # 9 / 1 - Source
#     txBuffer.append(len(data) + 5)  # 10/ 2 - Length
#     txBuffer.append(0)              # 11/ 3 - CRC High
#     txBuffer.append(0)              # 12/ 4 - CRC Low

#     print("MRBee transmitting from %02X to %02X" % (src, dest))

#     for b in data:
#        txBuffer.append(b & 0xFF)

     crc = mrbusCRC16Calculate(txBuffer[8:])
     txBuffer[11] = 0xFF & crc
     txBuffer[12] = 0xFF & (crc >> 8)

     xbeeChecksum = 0
     for i in range(3, len(txBuffer)):
        xbeeChecksum = (xbeeChecksum + txBuffer[i]) & 0xFF
     xbeeChecksum = (0xFF - xbeeChecksum) & 0xFF;
     txBuffer.append(xbeeChecksum)

     txBufferEscaped = [ txBuffer[0] ]
     
     escapedChars = frozenset([0x7E, 0x7D, 0x11, 0x13])

     for i in range(1, len(txBuffer)):
        if txBuffer[i] in escapedChars:
           txBufferEscaped.append(0x7D)
           txBufferEscaped.append(txBuffer[i] ^ 0x20)
        else:
           txBufferEscaped.append(txBuffer[i])

     #print("txBufferEscaped is %d bytes" % (len(txBufferEscaped)))
     #pkt = ""
     #for i in txBufferEscaped:
     #   pkt = pkt + "%02x " % (i)
     #print(pkt)
     self.serial.write(bytes(txBufferEscaped))

