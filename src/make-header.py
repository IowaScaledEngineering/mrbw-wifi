import os
import tempfile

root_dir = 'wibridge-python'
file_list = [ 'boot.py', 'config.txt', 'code.py', 'esu.py', 'font5x8.bin', 'ise.lgo', 'mrbus.py', 'MRBusThrottle.py', 'screen.py', 'secrets.py', 'switches.py', 'systemstate.py', 'withrottle.py', 'lib/adafruit_framebuf.mpy', 'lib/adafruit_ssd1306.mpy', 'lib/font5x8.bin' ]
fData = { }

outFile = 'mrbwwifi-init.h'
tmpFile = '/tmp/crap'

os.system("echo '#ifndef _MRBWWIFI_H_' > %s" % (outFile))
os.system("echo '#define _MRBWWIFI_H_' >> %s" % (outFile))

for fname in file_list:
  cmd = 'cd %s ; xxd -i %s > %s' % (root_dir, fname, tmpFile)
  os.system(cmd)
  with open(tmpFile, 'r') as f:
    fdata = f.read()
    fData[fname] = {'varname': fname.replace('.', '_').replace('/', '_'), 'data':fdata.replace("unsigned", "const unsigned") }


lines = []
for fname in fData.keys():
  fs = fData[fname]
  with open(outFile, 'a') as f:
    f.write(fs['data'])
  lines.append("    { \"/%s\", %s_len, %s }" % (fname, fs['varname'], fs['varname']))

otherStuff = """
typedef struct
{
    const char* fileNameStr;
    const unsigned int fileLen;
    const unsigned char* fileContents;
} FlashFile;

const FlashFile initFiles[] = 
{
"""

otherStuff = otherStuff + ',\n'.join(lines) + '\n};\n'

with open(outFile, 'a') as f:
  f.write(otherStuff)



os.system("echo '#endif' >> %s" % (outFile))
