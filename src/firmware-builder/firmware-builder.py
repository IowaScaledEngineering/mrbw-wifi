import sys
import os
import hashlib
from datetime import datetime

files_to_include = [
  'version.py',
  'code.py',
  'esu.py',
  'font5x8.bin',
  'ise.lgo',
  'mrbus.py',
  'MRBusThrottle.py',
  'screen.py',
  'switches.py',
  'systemstate.py',
  'withrottle.py',
  'lib/adafruit_framebuf.mpy',
  'lib/adafruit_ssd1306.mpy',
  'lib/font5x8.bin'
]

class FirmwareFile:
  def __init__(self, name, size, offset):
    self.name = name
    self.dataSz = size
    self.offset = offset

def main(path = '.', outputFilename = 'firmware.bin', semver = (0,0,0), gitver = 'UNKNWN') -> int:
  filearchive = { }
  datablob = bytes()
  for filename in files_to_include:
    with open(path + '/' + filename, "rb") as fp:
      
      filedata = fp.read()
      offset = len(datablob)
      datablob = datablob + filedata
      filearchive[filename] = FirmwareFile(filename, len(filedata), offset)
      print("Adding file [%s] - len [%d] offset [%d]" % (filearchive[filename].name, filearchive[filename].dataSz, filearchive[filename].offset))

  dataHash = hashlib.md5(datablob)
  print("Data MD5 is [%s]" % dataHash.hexdigest())

  with open(outputFilename, 'wb') as fp:
    now = datetime.now()
    creationTimestamp = now.strftime("%Y%m%d/%H%M%S")
    print("Created at [%s]" % (creationTimestamp))
    # Write header
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
    
    headerData = bytes("ISEFWBIN", "utf-8")
    headerData = headerData + bytes("01", "utf-8")
    headerData = headerData + bytes("MRBWWIFI", "utf-8")
    headerData = headerData + bytes(creationTimestamp, "utf-8")
    (major, minor, delta) = semver
    headerData = headerData + bytes("%02d.%02d.%02d+%6.6s" % (major, minor, delta, gitver), "utf-8")
    
    headerData = headerData + bytes("ENDH", "utf-8")
    numFiles = len(filearchive)
    dataOffset = len(headerData) + (numFiles + 1) * (10 + 10 + 255) + 32
    
    record = "%010d%010d%-255.255s" % (numFiles, len(datablob), dataHash.hexdigest())
    headerData = headerData + bytes(record, "utf-8")
    # Write file records
    for filename in filearchive.keys():
      filedata = filearchive[filename]
      record = "%010d%010d%-255.255s" % (filedata.dataSz, filedata.offset + dataOffset, filedata.name)
      headerData = headerData + bytes(record, "utf-8")
    
    # Write header MD5
    headerHash = hashlib.md5(headerData)
    print("Header MD5 is [%s]" % headerHash.hexdigest())
    headerData = headerData + bytes(headerHash.hexdigest(), "utf-8")
    
    # Write data
    if dataOffset != len(headerData):
      print("Fatal ERROR: Header data len [%d] does not match predicted offset [%d]" % (len(headerData), dataOffset))
      return -1
    else:
      print("Header length [%d]" % (dataOffset))
    fp.write(headerData)
    fp.write(datablob)

  return 0

if __name__ == '__main__':
  path = '../withrottle-firmware'
  try:
    path = sys.argv[1]
  except:
    pass

  outputFilename = 'firmware.bin'
  try:
    outputFilename = sys.argv[2]
  except:
    pass

  sys.exit(main(path, outputFilename))
