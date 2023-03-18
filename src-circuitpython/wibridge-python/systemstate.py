import re
import wifi

class SystemState:
  def __init__(self):
    self.isAutoNetwork = True
    self.hostName = "mrbw-wifi"
    self.cmdStationType = None
    self.networkSSID = ""
    self.networkPassword = ""
    self.ipv4 = ""
    self.throttlesConnected = 0
    self.baseAddr = 0
    self.throttleTimeoutSeconds = 60
    self.statusBroadcastIntervalSeconds = 2
    self.isWifiConnected = False
    self.isCmdStnConnected = False
    self.forceScreenUpdate = False
    self.cmdStationIP = None
    self.cmdStationPort = None
    self.conflictingBaseSeen = 0
    self.rssi = -100
    self.lps = 0
    self.configurationValid = False
    self.configurationError = "Conf Not Loaded"
    self.lnwiSSIDMatch = re.compile(r'Dtx\d+-[A-Za-z0-9 ]+_[0-9A-F][0-9A-F][0-9A-F][0-9A-F]-[0-7]')
    self.dccexSSIDMatch = re.compile(r'DCCEX_[A-Fa-f0-9][A-Fa-f0-9][A-Fa-f0-9][A-Fa-f0-9][A-Fa-f0-9][A-Fa-f0-9]')


  def getMrbusBaseAddr(self):
    return 0xD0 + self.baseAddr

  def getInterfaceVerPktText(self):
    # General pattern
    # WF-LNWI
    # WF-WTHR
    # WF-ESU
    # NO WIFI - no wifi
    # NO CMD  - no command station
    
    if not self.isWifiConnected:
      return [ ord('N'), ord('O'), ord(' '), ord(' '), ord('W'), ord('I'), ord('F'), ord('I') ]
    elif not self.isCmdStnConnected or self.cmdStationType is None:
      return [ ord('N'), ord('O'), ord(' '), ord('C'), ord('M'), ord('S'), ord('T'), ord('N') ]
    elif self.cmdStationType == "lnwi":
      return [ ord('W'), ord('I'), ord('F'), ord('I'), ord('L'), ord('N'), ord('W'), ord('I') ]
    elif self.cmdStationType == "withrottle":
      return [ ord('W'), ord('I'), ord('F'), ord('I'), ord('W'), ord('T'), ord('H'), ord('R') ]
    elif self.cmdStationType == "esu":
      return [ ord('W'), ord('I'), ord('F'), ord('I'), ord('E'), ord('S'), ord('U'), ord(' ') ]

    return [ ord('W'), ord('I'), ord('F'), ord('I'), ord('U'), ord('N'), ord('K'), ord('N') ]

  def scanForNetworks(self):
    # Scan for all possible connnection types    
    print("Available WiFi networks:")
    networks = wifi.radio.start_scanning_networks()
    networks = sorted(networks, key=lambda net: net.rssi, reverse=True)
    wifi.radio.stop_scanning_networks()

    autonets = []

    for network in networks:
      print("[%-32.32s] Ch: %2d RSSI: %d Auth: %s" % (str(network.ssid, "utf-8"), network.channel, network.rssi, network.authmode))
      if (self.cmdStationType is None or self.cmdStationType == 'lnwi') and self.lnwiSSIDMatch.match(network.ssid):
        autonets.append(('lnwi', network.ssid, ''))
      
      elif (self.cmdStationType is None or self.cmdStationType == 'esu') and network.ssid == 'ESUWIFI':
        autonets.append(('esu', network.ssid, '123456789'))
      
      elif (self.cmdStationType is None or self.cmdStationType == 'withrottle') and network.ssid == 'MRCWi-Fi':
        autonets.append(('withrottle', network.ssid, ''))
      
      elif (self.cmdStationType is None or self.cmdStationType == 'withrottle') and network.ssid == 'RPi-JMRI':
        if self.cmdStationIP is None:
          self.cmdStationIP = "192.168.6.1"
        autonets.append(('withrottle', network.ssid, 'rpI-jmri'))

      elif (self.cmdStationType is None or self.cmdStationType == 'withrottle') and self.dccexSSIDMatch.match(network.ssid):
        password = "PASS_" + network.ssid[6:]
        autonets.append(('withrottle', network.ssid, password))
        if self.cmdStationIP is None:
          self.cmdStationIP = "192.168.4.1"
        if self.cmdStationPort is None:
          self.cmdStationPort = 2560

       # If we found an automatic network to connect to, break out
      if len(autonets):
        (self.cmdStationType,self.networkSSID,self.networkPassword) = autonets[0]
        return
       
    self.networkSSID = None
    self.networkPassword = ""
    return


  def readConfigurationFile(self, filename):
    print("reading file %s" % (filename))
    jsConf = { }

    try:
      with open(filename, 'r') as fp:
        for fline in fp:
          fline = fline.strip()
          #Ignore empty lines
          if len(fline) == 0:
            continue
          # Ignore comments
          if fline[0] == '#':
            continue
          kv = fline.split('=', 1)
          if len(kv) < 2:
            continue
          key = kv[0].strip()
          value = kv[1].strip()
          
          if len(key) == 0 or len(value) == 0:
            continue
          jsConf[key] = value

      try:
        for key in jsConf.keys():
          if key == 'ssid' and len(jsConf['ssid'].strip()) != 0:
            self.isAutoNetwork = False
            self.networkSSID = jsConf['ssid']

          elif key == 'password' and len(jsConf['password'].strip()) != 0:
            self.networkPassword = jsConf['password']
          
          elif key == 'mode' and jsConf['mode'] in ['lnwi', 'withrottle', 'esu']:
            self.cmdStationType = jsConf['mode']
            if self.cmdStationType == 'lnwi':
              self.cmdStationPort = 12090
            elif self.cmdStationType == 'withrottle' and self.cmdStationPort == None:
              self.cmdStationPort = 12090
            elif self.cmdStationType == 'esu' and self.cmdStationPort == None:
              self.cmdStationPort = 15471

          elif key == 'serverPort' and len(jsConf['serverPort'].strip()) != 0:
            self.cmdStationPort = int(jsConf['serverPort'].strip())

          elif key == 'serverIP' and len(jsConf['serverIP'].strip()) != 0:
            self.cmdStationIP = jsConf['serverIP'].strip()


        # FIXME: If we have a network but we don't have a command station type, force it


        # If we get to the end and we don't have an ssid (autonetwork = false), then reset everything
        if self.isAutoNetwork:
          self.networkSSID = None
          self.networkPassword = None
        
        self.configurationValid = True
      except:
        self.configurationError = "Syntax Error"
        self.configurationValid = False
        return

    except Exception as e:
      self.configurationValid = False
      self.configurationError = "File Not Found"
      print(e)
