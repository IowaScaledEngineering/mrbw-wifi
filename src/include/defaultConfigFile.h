#pragma once


const char* defaultConfigFileContents = 
"# This file is a simple key = value configuration file\n" \
"# Any line that starts with # is a comment and ignored\n" \
"# Any line without a value (right of the equals) is ignored\n" \
"\n" \
"# If you want to manually configure the wifi network to connect to, please\n" \
"# fill in ssid, password (blank if open), and mode\n" \
"\n" \
"ssid = \n" \
"password = \n" \
"\n" \
"# Mode is the command station type - can be lnwi, withrottle, or esu \n" \
"mode = \n" \
"\n" \
"# If you want to manually configure your server address, do so here \n" \
"serverIP = \n" \
"serverPort = \n" \
"\n";
