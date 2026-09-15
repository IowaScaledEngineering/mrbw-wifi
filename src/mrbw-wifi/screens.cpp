
#include "screens.h"
#include "versions.h"

void drawSplashScreen(SystemState& state, I2CDisplay& display)
{
  char buffer[32];
  display.clrscr();
  display.putstr("Iowa Scaled", 0, 0);
  display.putstr("Engineering", 0, 1);
  display.putstr("MRBW-WIFI  ", 0, 2);
  snprintf(buffer, sizeof(buffer), "%d.%d.%d", MAJOR_VERSION, MINOR_VERSION, DELTA_VERSION);
  display.putstr(buffer, 0, 3);
  
  snprintf(buffer, sizeof(buffer)-1, "%02X%02X", state.macAddr[4], state.macAddr[5]);
  display.putstr(buffer, 15, 4);
  
  display.drawISELogo();  
  display.refresh();
}


void drawStatusScreen(SystemState& state, I2CDisplay& display)
{
  char lineBuf[24];
  char lpsText[16];
  const uint8_t spinnerChars[] = { '-', '\\', '|', '/' };
  static uint8_t spinnerNum = 0;
//     000000000011111111112
//     012345678901234567890
//  0:[a:LNWI    s T:nn B:aa] a=A/auto, C/config file (LNWI/ESU /WTHR/NONE) s=spinner nn=throttles connected aa=base addr  
//  1:[SSID-NAME-HERE-HERE-H] (scrolling)
//  2:[192.168.255.255:ppppp]
//  3:[R:-sss cc xxC xxxxlps] s=rssi, cc=channel

  // Update spinner
  spinnerNum = (spinnerNum + 1) % sizeof(spinnerChars);

  display.clrscr(false);

  memset(lineBuf, 0, sizeof(lineBuf));
  const char* cmdStnStr = "UNKN";
  switch(state.cmdStnType)
  {
    case CMDSTN_NONE:
      cmdStnStr = "NONE";
      break;
    case CMDSTN_LNWI:
      cmdStnStr = "LNWI";
      break;
    case CMDSTN_DCCEX:
      cmdStnStr = "DCCX";
      break;      
    case CMDSTN_JMRI:
      cmdStnStr = "WTHR";
      break;
    case CMDSTN_ESU:
      cmdStnStr = "ESU ";
      break;
    default:
      cmdStnStr = "UNKN";
      break;
  }

//  snprintf(lineBuf, sizeof(lineBuf), "%c:%4.4s    %c T:%02d %c:%02d", state.isAutoNetwork?'A':'C', cmdStnStr, spinnerChars[spinnerNum], state.activeThrottles, state.isConflictingBasePresent()?'*':'B', state.baseAddress);
  snprintf(lineBuf, sizeof(lineBuf), "%1.1d:%4.4s    %c T:%02d %c:%02d", state.activeConfigNum, cmdStnStr, spinnerChars[spinnerNum], state.activeThrottles, state.isConflictingBasePresent()?'*':'B', state.baseAddress);
  display.putstr(lineBuf, 0, 0);

  if (0 == strlen(state.ssid))
  {
    snprintf(lineBuf, sizeof(lineBuf), "%-21.21s", "(Searching...)");
  } else {
    snprintf(lineBuf, sizeof(lineBuf), "%-21.21s", state.ssid);
  }
  display.putstr(lineBuf, 0, 1);

  if (state.isWifiConnected)
  {
    char ipBuffer[64];
    switch(state.ipDisplayLine)
    {
      case DISPLAY_IP_LOCAL:  // Two phases for 
      case DISPLAY_IP_LOCAL2:
        snprintf(lineBuf, sizeof(lineBuf), "L:%-19.19s", state.localIP.toString().c_str());
        break;
      case DISPLAY_IP_CMDSTN:
      case DISPLAY_IP_CMDSTN2:
        snprintf(ipBuffer, sizeof(ipBuffer), "%s:%d", state.cmdStnIP.toString().c_str(), state.cmdStnPort);
        snprintf(lineBuf, sizeof(lineBuf), "C:%-19.19s", ipBuffer);
        break;
      default:
        break;
    }
    state.ipDisplayLine = (IPLineDisplay)((state.ipDisplayLine + 1) % DISPLAY_IP_MAX_FIELDS);

  } else {
    snprintf(lineBuf, sizeof(lineBuf), "%-21.21s", "(No Network)");
  }
  display.putstr(lineBuf, 0, 2);

  uint32_t lps = state.loopCnt;
  state.loopCnt = 0;

  if (lps < 10000)
    snprintf(lpsText, sizeof(lpsText), "%04lul/s", lps);
  else if (lps >= 10000)
    snprintf(lpsText, sizeof(lpsText), "%03lukl/s", lps / 1000);
  else if (lps >= 100000)
    snprintf(lpsText, sizeof(lpsText), "%03luMl/s", lps / 1000000);

  snprintf(lineBuf, sizeof(lineBuf), "R:%02lddB  40C %7.7s", state.rssi, lpsText);
  display.putstr(lineBuf, 0, 3);

  display.refresh();
}
