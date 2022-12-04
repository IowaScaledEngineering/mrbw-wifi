import board
import digitalio
import neopixel_write
import time
import os
import sys
import binascii
import re
import storage

# This is hideous, but most circuitpython implementations don't include MD5 or any of
# hashlib.  So I had to borrow this.  
SV = [0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 
      0x4787c62a, 0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af,
      0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 
      0x49b40821, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 
      0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 
      0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 
      0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 
      0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 
      0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x4881d05, 0xd9d4d039,
      0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, 0xf4292244, 0x432aff97,
      0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 
      0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 
      0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391]

def leftCircularShift(k,bits):
  bits = bits%32
  k = k%(2**32)
  upper = (k<<bits)%(2**32)
  result = upper | (k>>(32-(bits)))
  return(result)

def F(X,Y,Z):
  return( (X&Y)|((~X)&Z) )

def G(X,Y,Z):
  return( (X&Z) | (Y&(~Z)) )

def H(X,Y,Z):
  return( X^Y^Z )

def I(X,Y,Z):
  return( Y^(X|(~Z)) )

def FF(a,b,c,d,M,s,t):
  result = b + leftCircularShift( (a+F(b,c,d)+M+t), s)
  return(result)

def GG(a,b,c,d,M,s,t):
  result = b + leftCircularShift( (a+G(b,c,d)+M+t), s)
  return(result)

def HH(a,b,c,d,M,s,t):
  result = b + leftCircularShift( (a+H(b,c,d)+M+t), s)
  return(result)

def II(a,b,c,d,M,s,t):
  result = b + leftCircularShift( (a+I(b,c,d)+M+t), s)
  return(result)

def fmt8(num):
  bighex = "{0:08x}".format(num)
  binver = binascii.unhexlify(bighex)
  result = "{0:08x}".format(int.from_bytes(binver, 'little'))
  return(result)

def bitlen( bitstring ):
  return(len(bitstring)*8)

class MD5:
  def __init__(self):
    self.A = 0x67452301
    self.B = 0xefcdab89
    self.C = 0x98badcfe
    self.D = 0x10325476
    self.count = 0
    self.buffer = bytes()

  def update(self, msg):
    self.buffer = self.buffer + msg
    bufferSz = len(self.buffer)
    
    # Work through all full blocks
    blocks = bufferSz // 64
    for i in range(blocks):
      newblock = self.buffer[i*64:(i+1)*64]
      self.blockHash(newblock)

    self.count = self.count + blocks * 64
    self.buffer = self.buffer[blocks*64:]

  def blockHash(self, block):
    a = self.A
    b = self.B
    c = self.C
    d = self.D
    
    chunks = 16
    M = []
    size = len(block)//chunks
    for i in range(0, chunks):
      M.append( int.from_bytes( block[i*size:(i+1)*size], "little" ))

    #Rounds
    a = FF( a,b,c,d, M[0], 7, SV[0] )
    d = FF( d,a,b,c, M[1], 12, SV[1] )
    c = FF( c,d,a,b, M[2], 17, SV[2] )
    b = FF( b,c,d,a, M[3], 22, SV[3] )
    a = FF( a,b,c,d, M[4], 7, SV[4] )
    d = FF( d,a,b,c, M[5], 12, SV[5] )
    c = FF( c,d,a,b, M[6], 17, SV[6] )
    b = FF( b,c,d,a, M[7], 22, SV[7] )
    a = FF( a,b,c,d, M[8], 7, SV[8] )
    d = FF( d,a,b,c, M[9], 12, SV[9] )
    c = FF( c,d,a,b, M[10], 17, SV[10] )
    b = FF( b,c,d,a, M[11], 22, SV[11] )
    a = FF( a,b,c,d, M[12], 7, SV[12] )
    d = FF( d,a,b,c, M[13], 12, SV[13] )
    c = FF( c,d,a,b, M[14], 17, SV[14] )
    b = FF( b,c,d,a, M[15], 22, SV[15] )
    a = GG( a,b,c,d, M[1], 5, SV[16] )
    d = GG( d,a,b,c, M[6], 9, SV[17] )
    c = GG( c,d,a,b, M[11], 14, SV[18] )
    b = GG( b,c,d,a, M[0], 20, SV[19] )
    a = GG( a,b,c,d, M[5], 5, SV[20] )
    d = GG( d,a,b,c, M[10], 9, SV[21] )
    c = GG( c,d,a,b, M[15], 14, SV[22] )
    b = GG( b,c,d,a, M[4], 20, SV[23] )
    a = GG( a,b,c,d, M[9], 5, SV[24] )
    d = GG( d,a,b,c, M[14], 9, SV[25] )
    c = GG( c,d,a,b, M[3], 14, SV[26] )
    b = GG( b,c,d,a, M[8], 20, SV[27] )
    a = GG( a,b,c,d, M[13], 5, SV[28] )
    d = GG( d,a,b,c, M[2], 9, SV[29] )
    c = GG( c,d,a,b, M[7], 14, SV[30] )
    b = GG( b,c,d,a, M[12], 20, SV[31] )
    a = HH( a,b,c,d, M[5], 4, SV[32] )
    d = HH( d,a,b,c, M[8], 11, SV[33] )
    c = HH( c,d,a,b, M[11], 16, SV[34] )
    b = HH( b,c,d,a, M[14], 23, SV[35] )
    a = HH( a,b,c,d, M[1], 4, SV[36] )
    d = HH( d,a,b,c, M[4], 11, SV[37] )
    c = HH( c,d,a,b, M[7], 16, SV[38] )
    b = HH( b,c,d,a, M[10], 23, SV[39] )
    a = HH( a,b,c,d, M[13], 4, SV[40] )
    d = HH( d,a,b,c, M[0], 11, SV[41] )
    c = HH( c,d,a,b, M[3], 16, SV[42] )
    b = HH( b,c,d,a, M[6], 23, SV[43] )
    a = HH( a,b,c,d, M[9], 4, SV[44] )
    d = HH( d,a,b,c, M[12], 11, SV[45] )
    c = HH( c,d,a,b, M[15], 16, SV[46] )
    b = HH( b,c,d,a, M[2], 23, SV[47] )
    a = II( a,b,c,d, M[0], 6, SV[48] )
    d = II( d,a,b,c, M[7], 10, SV[49] )
    c = II( c,d,a,b, M[14], 15, SV[50] )
    b = II( b,c,d,a, M[5], 21, SV[51] )
    a = II( a,b,c,d, M[12], 6, SV[52] )
    d = II( d,a,b,c, M[3], 10, SV[53] )
    c = II( c,d,a,b, M[10], 15, SV[54] )
    b = II( b,c,d,a, M[1], 21, SV[55] )
    a = II( a,b,c,d, M[8], 6, SV[56] )
    d = II( d,a,b,c, M[15], 10, SV[57] )
    c = II( c,d,a,b, M[6], 15, SV[58] )
    b = II( b,c,d,a, M[13], 21, SV[59] )
    a = II( a,b,c,d, M[4], 6, SV[60] )
    d = II( d,a,b,c, M[11], 10, SV[61] )
    c = II( c,d,a,b, M[2], 15, SV[62] )
    b = II( b,c,d,a, M[9], 21, SV[63] )
    self.A = (self.A + a)%(2**32)
    self.B = (self.B + b)%(2**32)
    self.C = (self.C + c)%(2**32)
    self.D = (self.D + d)%(2**32)

  def hexdigest(self):
    # Pad up and finish any remaining data
    self.count = self.count + len(self.buffer)
    msgLen = (8*self.count)%(2**64)
    self.buffer = self.buffer + b'\x80'
    zeroPad = (448 - (msgLen+8)%512)%512
    zeroPad //= 8
    self.buffer = self.buffer + b'\x00'*zeroPad + msgLen.to_bytes(8, 'little')
    self.update(bytes())

    result = fmt8(self.A)+fmt8(self.B)+fmt8(self.C)+fmt8(self.D)
    return result

def getFilesize(fwFilename):
  #st_size = os.path.getsize(fwFilename)
  (st_mode, st_ino, st_dev, st_nlink, st_uid, st_gid, st_size, st_atime, st_mtime, st_ctime) = os.stat(fwFilename)
  return st_size
  
def directoryExists(path):
  (st_mode, st_ino, st_dev, st_nlink, st_uid, st_gid, fwFileSize, st_atime, st_mtime, st_ctime) = os.stat(path)
  #return os.path.isdir(path)
  return st_mode != 0
  
def doFirmwareUpgrade(fwFilename, devType="00000000", path='/', testRun=False, ledPin=None):
  # This should increment through each upgrade file in the firmware block
  # and replace on it on the drive

  # Header
  # 8 bytes of identifier (ISEFWBIN)
  # 2 bytes of firmware update format (currently "01" as a string)
  # 8 bytes target device name (to prevent firmware going on the wrong device)
  # 15 bytes creation timestamp (YYYYMMDD/HHMMSS)
  # 2 bytes major version (ascii)
  # 1 byte period
  # 2 bytes minor version (ascii)
  # 1 byte period
  # 2 bytes delta version (ascii)
  # 1 byte +
  # 6 bytes git version
  #  ... insert stuff here in the future
  # 4 bytes "ENDH"
  # File records:
  #   10 bytes size (ascii, 0 padded)
  #   10 bytes offset into data blob from start of file (ascii, 0 padded)
  #   255 bytes filename
  #   First record is special - size of number of file records (inclusive), size of data blob in bytes, filename of data MD5
  # 32 bytes MD5 of header (ASCII, hex)

  #fwFileSize = os.path.getsize(fwFilename)
  (st_mode, st_ino, st_dev, st_nlink, st_uid, st_gid, fwFileSize, st_atime, st_mtime, st_ctime) = os.stat(fwFilename)

  if fwFileSize < 359:
    raise Exception("FW header too small")

  with open(fwFilename, 'rb') as fwbp:
    ident = str(fwbp.read(8), "utf-8")
    if "ISEFWBIN" != ident:
      raise Exception("FW bad identifier")

    fwFormatVersion = int(str(fwbp.read(2), "utf-8"))

    fwTargetDevice = str(fwbp.read(8), "utf-8")
    if fwTargetDevice != devType:
      raise Exception("FW wrong target device")
 
    fwCreationTime = str(fwbp.read(15), "utf-8")
    print("Firmware for [%s] created at [%s]" % (fwTargetDevice, fwCreationTime))
    
    fwMajorVer = int(str(fwbp.read(2), "utf-8"))
    fwbp.read(1)
    fwMinorVer = int(str(fwbp.read(2), "utf-8"))
    fwbp.read(1)
    fwDeltaVer = int(str(fwbp.read(2), "utf-8"))
    fwbp.read(1)
    fwGitVer = str(fwbp.read(6), "utf-8")
    
    print("Firmware ver %d.%d.%d [%6.6s]" % (fwMajorVer, fwMinorVer, fwDeltaVer, fwGitVer))

    # Now, read until we get ENDH
    termStr = ""
    while termStr != "ENDH":
      s = str(fwbp.read(1), "utf-8")
      if s not in ['E', 'N', 'D', 'H']:
        termStr = ""
      else:
        termStr = termStr + s
    # Now we're at the start of file records
    # Remember, the first one is special
    
    numFiles = int(str(fwbp.read(10), "utf-8"))
    dataSz = int(str(fwbp.read(10), "utf-8"))
    dataMD5 = str(fwbp.read(255), "utf-8").strip()

    # Data should start at current offset + length of file entries + header MD5
    dataOffset = fwbp.tell() + (10 + 10 + 255) * (numFiles) + 32

    print("Found [%d] files at offset [%d]" % (numFiles, dataOffset))


    try:
      if ledPin != None:
        color = bytearray([128, 0, 128])
        neopixel_write.neopixel_write(ledPin, color)

      # Go get the header MD5 - it's after the file entries
      savePosition = fwbp.tell()
      fwbp.seek(savePosition + (numFiles) * 275)
      headerMD5 = str(fwbp.read(32), "utf-8")

      fwbp.seek(0)
      header = fwbp.read(dataOffset-32)
      h = MD5()
      h.update(header)
      fileHeaderMD5 = h.hexdigest()
      
      if fileHeaderMD5 != headerMD5:
        print("Header MD5 (%d bytes) MISMATCH = [%s = %s]" % (dataOffset-32, fileHeaderMD5, headerMD5))
        raise Exception("Header MD5 does not match!")
      else:
        print("Header MD5 matches [%s]" % (fileHeaderMD5))

      # Go to the start of data, make its MD5
      fwbp.seek(dataOffset)
      h = MD5()
      bytesRemaining = dataSz
      while bytesRemaining > 0:
        blockSz = min(256, bytesRemaining)
        h.update(fwbp.read(blockSz))
        bytesRemaining = bytesRemaining - blockSz
      
      fileDataMD5 = h.hexdigest()

      if fileDataMD5 != dataMD5:
        print("Data MD5 (%d bytes) MISMATCH [%s = %s]" % (dataSz, fileDataMD5, dataMD5))
        raise Exception("Data MD5 does not match!")
      else:
        print("Data MD5 matches [%s]" % (fileDataMD5))
    except Exception as e:
      raise e

    # Go back to the real first file entry
    fwbp.seek(savePosition)
    if ledPin != None:
      color = bytearray([0, 192, 128])
      neopixel_write.neopixel_write(ledPin, color)

    print("Found %d files" % (numFiles))
    for n in range(0, numFiles):
      fileSize = int(str(fwbp.read(10), "utf-8"))
      fileOffset = int(str(fwbp.read(10), "utf-8"))
      fileName = str(fwbp.read(255), "utf-8").strip()
      print("File [%d:%s] size=[%d] offset=[%d]" % (n, fileName, fileSize, fileOffset))
      
      savePosition = fwbp.tell()
      fwbp.seek(fileOffset)
      bytesRemaining = fileSize
      
      fileDir = fileName.split('/')
      # if this file has a directory path, we might need to create it
      if len(fileDir) > 0:
        for i in range(1,len(fileDir)):
          subPath = '/'.join(fileDir[:i])
          print("Path component %d [%s]" % (i, subPath))
          if not directoryExists(path + '/' + subPath):
            print("Creating path %s" % (path + '/' + subPath))
            if not testRun:
              os.mkdir(path + '/' + subPath)

      if not testRun:
        with open(path + "/" + fileName, "wb") as fp:
          # read in 256 byte chunks
          while bytesRemaining > 0:
            blockSz = min(256, bytesRemaining)
            if not testRun:
              fp.write(fwbp.read(blockSz))
            bytesRemaining = bytesRemaining - blockSz
          fp.flush()

      fwbp.seek(savePosition)

    if ledPin != None:
      color = bytearray([255, 0, 0])
      neopixel_write.neopixel_write(ledPin, color)

dryRun = False

try:
  firmwareName = None
  firmwareMatch = re.compile(r'mrbwwifi-[0-7]_[0-7]_[0-7].bin')
  for filename in os.listdir("/"):
    if firmwareMatch.match(filename):
      firmwareName = "/" + filename
      break

  if firmwareName != None:
    with open(firmwareName, "rb") as firmwareFile:
      pass # Try to open the firmware updater - if we fail, it'll bomb with an exception and we'll go on

    import storage
    import microcontroller
    if not dryRun:
      storage.disable_usb_drive()
      storage.remount("/", False)
    ledPin = None
    try:
      try:
        ledPin = digitalio.DigitalInOut(board.IO45)
        ledPin.direction = digitalio.Direction.OUTPUT
      except Exception as e:
        print("Cannot initialize LED pin\n%s" % (e))

      doFirmwareUpgrade(firmwareName, "MRBWWIFI", "/", dryRun, ledPin=ledPin)
      os.remove(firmwareName)
      os.sync()
      time.sleep(1)
      microcontroller.reset()
      # Have we successfully upgraded?  Remove the firmware file
      # Reset!

    except Exception as e:
      print(e)
      if ledPin != None:
        while(1):
          color = bytearray([0, 255, 0])
          neopixel_write.neopixel_write(pin, color)
          time.sleep(0.5)
          color = bytearray([0, 0, 0])
          neopixel_write.neopixel_write(pin, color)
          time.sleep(0.5)
  
except:
  # No new firmware, go on with life
  pass

switchFR = digitalio.DigitalInOut(board.IO21)
switchFR.direction = digitalio.Direction.INPUT
switchFR.pull = digitalio.Pull.UP
time.sleep(0.05)

if not switchFR.value:
  try:
    ledPin = digitalio.DigitalInOut(board.IO45)
    ledPin.direction = digitalio.Direction.OUTPUT
    neopixel_write.neopixel_write(ledPin, bytearray([0,0,64]))
    for i in range(0,10):
      time.sleep(0.5)
      neopixel_write.neopixel_write(ledPin, bytearray([0,0,0]))
      time.sleep(0.5)
      neopixel_write.neopixel_write(ledPin, bytearray([0,0,64]))
  except:
    ledPin = None

  print("Erasing filesystem")
  storage.erase_filesystem()
  if ledPin != None:
    neopixel_write.neopixel_write(ledPin, bytearray([0,64,0]))

  print("Done")
  while(1):
    pass
