#pragma once


const char* defaultConfigFileContents = 
"# This file is a simple key = value configuration file\n" \
"# Any line that starts with # is a comment and ignored\n" \
"# Any line without a value (right of the equals) is ignored\n" \
"\n" \
"# If you want to manually configure the wifi network to connect to, please\n" \
"# fill in ssid, password (blank if open), and mode\n" \
"\n" \
"# Up to 9 configurations are supported by duplicating \n" \
"# ssid/password/mode/serverIP/serverPort and changing the index number\n" \
"# They will be searched in order, starting at 1, and the first one with a corresponding\n" \
"# wireless network seen by the receiver will be used\n" \
"\n" \
"ssid[1] = \n" \
"password[1] = \n" \
"\n" \
"# Mode is the command station type - can be lnwi, withrottle, dccex, wfd30, or esu \n" \
"mode[1] = \n" \
"\n" \
"# If you want to manually configure your server address, do so here \n" \
"serverIP[1] = \n" \
"serverPort[1] = \n" \
"\n" \
"# If you want to use your JMRI or DCC-EX as your fast clock source, set this to cmdstn, otherwise leave at none \n" \
"fastClockSource[1] = none\n" \
"\n" \
"# ** GLOBAL OPTIONS **" \
"# Control here is global, not configuration specific, and do not have an index number\n" \
"# Controls the verbosity of debug logging on the USB serial console - options are error, warn, info and debug \n" \
"logLevel = info\n" \
"\n"
;
