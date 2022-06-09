v 20130925 2
T 66900 40900 9 10 1 0 0 0 1
ProtoThrottle Receiver for Wifi Systems (ESU/Digitrax/MRC/JMRI)
T 66800 40600 9 10 1 0 0 0 1
mrbw-wifi.sch
T 67000 40300 9 10 1 0 0 0 1
1
T 68500 40300 9 10 1 0 0 0 1
1
T 70800 40300 9 10 1 0 0 0 1
Nathan Holmes
C 40000 40000 0 0 0 title-bordered-D.sym
T 72500 42500 9 10 1 0 0 6 3
Notes:
1) All capacitors are ceramic (X7R/X5R) unless otherwise noted.
2) All capacitors, resistors, and LEDs are 0805 unless otherwise noted.
C 56500 43900 1 270 1 led-3.sym
{
T 56250 43950 5 10 1 1 270 6 1
device=RED (ASSOC) LED
T 57050 44350 5 10 1 1 270 6 1
refdes=D2
T 56500 43900 5 10 0 0 180 2 1
footprint=0805
}
N 56700 43800 56700 43900 4
C 56800 43500 1 0 1 gnd-1.sym
C 66300 45000 1 0 0 hole-1.sym
{
T 66300 45000 5 10 0 1 0 0 1
device=HOLE
T 66500 45600 5 10 1 1 0 4 1
refdes=H1
T 66300 45000 5 10 0 0 0 0 1
footprint=STANDOFF_HEX_n4
}
C 66800 45000 1 0 0 hole-1.sym
{
T 66800 45000 5 10 0 1 0 0 1
device=HOLE
T 67000 45600 5 10 1 1 0 4 1
refdes=H2
T 66800 45000 5 10 0 0 0 0 1
footprint=STANDOFF_HEX_n4
}
C 67300 45000 1 0 0 hole-1.sym
{
T 67300 45000 5 10 0 1 0 0 1
device=HOLE
T 67500 45600 5 10 1 1 0 4 1
refdes=H3
T 67300 45000 5 10 0 0 0 0 1
footprint=STANDOFF_HEX_n4
}
C 67800 45000 1 0 0 hole-1.sym
{
T 67800 45000 5 10 0 1 0 0 1
device=HOLE
T 68000 45600 5 10 1 1 0 4 1
refdes=H4
T 67800 45000 5 10 0 0 0 0 1
footprint=STANDOFF_HEX_n4
}
C 69300 53900 1 0 0 gnd-1.sym
N 69400 54200 69400 57400 4
N 69100 54500 69400 54500 4
N 69100 54800 69400 54800 4
N 69100 55100 69400 55100 4
N 69100 55400 69400 55400 4
N 69100 55700 69400 55700 4
C 69100 57000 1 180 0 switch-dip8-1.sym
{
T 67700 54425 5 8 0 0 180 0 1
device=219-8MST
T 68800 54250 5 10 1 1 180 0 1
refdes=SW1
T 69100 57000 5 10 0 0 180 0 1
footprint=DIPSW16
}
N 69100 56000 69400 56000 4
N 69100 56300 69400 56300 4
N 69100 56600 69400 56600 4
C 48000 49900 1 270 0 capacitor-1.sym
{
T 48700 49700 5 10 0 1 270 0 1
device=CAPACITOR
T 48900 49700 5 10 0 0 270 0 1
symversion=0.1
T 48000 49900 5 10 0 0 0 0 1
footprint=0805
T 48300 49600 5 10 1 1 0 0 1
refdes=C5
T 48300 49100 5 10 1 1 0 0 1
value=1uF
T 48300 48900 5 10 1 1 0 0 1
description=16V
}
C 46800 49900 1 270 0 capacitor-1.sym
{
T 47500 49700 5 10 0 1 270 0 1
device=CAPACITOR
T 47700 49700 5 10 0 0 270 0 1
symversion=0.1
T 46800 49900 5 10 0 0 0 0 1
footprint=0805
T 47100 49600 5 10 1 1 0 0 1
refdes=C6
T 47100 49100 5 10 1 1 0 0 1
value=8.2pF
T 47100 48900 5 10 1 1 0 0 1
description=16V, NP0
}
N 48200 49900 48200 50100 4
N 47000 49900 47000 50100 4
C 46900 48700 1 0 0 gnd-1.sym
C 48100 48700 1 0 0 gnd-1.sym
T 46900 49800 9 10 1 0 0 6 2
Place C5 & C6
near XBee pin 1
N 47000 50100 50600 50100 4
C 49100 46800 1 270 1 resistor-1.sym
{
T 49500 47100 5 10 0 0 90 2 1
device=RESISTOR
T 49000 47300 5 10 1 1 0 7 1
refdes=R8
T 49000 47100 5 10 1 1 0 7 1
value=4.7k
T 49100 46800 5 10 0 0 90 2 1
footprint=0805
}
C 44200 57100 1 0 0 gnd-1.sym
N 44000 57600 44300 57600 4
N 44300 57600 44300 57400 4
C 42600 56500 1 0 0 gnd-1.sym
N 44000 58800 49900 58800 4
N 44000 58200 45000 58200 4
{
T 45100 58200 5 10 1 1 0 1 1
netname=D+
}
N 44000 58500 45000 58500 4
{
T 45100 58500 5 10 1 1 0 1 1
netname=D-
}
N 58300 55400 57800 55400 4
{
T 57700 55400 5 10 1 1 0 7 1
netname=D+
}
N 58300 55700 57800 55700 4
{
T 57700 55700 5 10 1 1 0 7 1
netname=D-
}
C 56800 60600 1 0 0 3.3V-plus-1.sym
N 57000 60600 57000 59000 4
N 57000 59000 58300 59000 4
C 57800 59200 1 0 0 gnd-1.sym
N 57900 59500 58300 59500 4
N 58300 59500 58300 59300 4
N 63300 59300 62800 59300 4
N 62800 59300 62800 59000 4
C 63400 59000 1 0 1 gnd-1.sym
C 48900 58600 1 270 0 capacitor-1.sym
{
T 49600 58400 5 10 0 1 270 0 1
device=CAPACITOR
T 49200 58300 5 10 1 1 0 0 1
refdes=C2
T 49800 58400 5 10 0 0 270 0 1
symversion=0.1
T 49200 57800 5 10 1 1 0 0 1
value=10uF
T 48900 58600 5 10 0 0 0 0 1
footprint=0805
T 49200 57600 5 10 1 1 0 0 1
description=16V
}
N 49100 58800 49100 58600 4
C 48100 58600 1 270 0 capacitor-1.sym
{
T 48800 58400 5 10 0 1 270 0 1
device=CAPACITOR
T 48400 58300 5 10 1 1 0 0 1
refdes=C1
T 49000 58400 5 10 0 0 270 0 1
symversion=0.1
T 48400 57800 5 10 1 1 0 0 1
value=0.1uF
T 48100 58600 5 10 0 0 0 0 1
footprint=0805
T 48400 57600 5 10 1 1 0 0 1
description=16V
}
N 48300 58600 48300 58800 4
N 48300 57700 48300 56900 4
N 48300 56900 53900 56900 4
N 49100 56900 49100 57700 4
C 51600 56600 1 0 0 gnd-1.sym
N 50800 58200 50800 56900 4
C 52400 58500 1 270 0 capacitor-1.sym
{
T 53100 58300 5 10 0 1 270 0 1
device=CAPACITOR
T 52700 58200 5 10 1 1 0 0 1
refdes=C3
T 53300 58300 5 10 0 0 270 0 1
symversion=0.1
T 52700 57700 5 10 1 1 0 0 1
value=22uF
T 52400 58500 5 10 0 0 0 0 1
footprint=0805
T 52700 57500 5 10 1 1 0 0 1
description=6.3V
}
N 51700 58800 53900 58800 4
N 52600 58800 52600 58500 4
N 52600 56900 52600 57600 4
C 53700 59200 1 0 0 3.3V-plus-1.sym
C 54100 56900 1 90 0 led-3.sym
{
T 54350 56850 5 10 1 1 90 0 1
device=GREEN LED
T 53750 57650 5 10 1 1 0 6 1
refdes=D1
T 54100 56900 5 10 0 0 0 0 1
footprint=0805
}
C 54000 57800 1 90 0 resistor-1.sym
{
T 53600 58100 5 10 0 0 90 0 1
device=RESISTOR
T 54100 58300 5 10 1 1 0 1 1
refdes=R3
T 54100 58100 5 10 1 1 0 1 1
value=330
T 54000 57800 5 10 0 0 90 0 1
footprint=0805
}
N 53900 58700 53900 59200 4
C 57700 60400 1 270 0 capacitor-1.sym
{
T 58400 60200 5 10 0 1 270 0 1
device=CAPACITOR
T 58000 60100 5 10 1 1 0 0 1
refdes=C4
T 58600 60200 5 10 0 0 270 0 1
symversion=0.1
T 57700 59600 5 10 1 1 0 6 1
value=10uF
T 57700 60400 5 10 0 0 0 0 1
footprint=0805
T 57700 59400 5 10 1 1 0 6 1
description=16V
}
N 57900 60400 57000 60400 4
T 52800 60700 9 10 1 0 0 0 3
NOTE: Per ESP32-S2-SOLO design guide
place 10uF as close to module power
pins as possible
C 50600 46300 1 0 0 xb3_mmt.sym
{
T 53000 50700 5 10 0 0 0 0 1
device=XBEE
T 53100 50500 5 10 0 0 0 0 1
footprint=XB3_MMT
T 52550 50800 5 10 1 1 0 3 1
refdes=U3
}
C 54800 46600 1 0 1 gnd-1.sym
N 54700 46900 54700 51000 4
N 54700 47100 54500 47100 4
N 54700 50400 54500 50400 4
N 54700 51000 54500 51000 4
C 51800 53600 1 0 1 xbprog-1.sym
{
T 51800 55200 5 10 0 1 0 6 1
device=AVRPROG
T 51800 53600 5 10 0 0 0 6 1
footprint=TC2030
T 51200 54900 5 10 1 1 0 6 1
refdes=J2
}
N 49500 54600 50400 54600 4
C 50300 53300 1 0 0 gnd-1.sym
N 50400 53600 50400 53800 4
N 51800 53800 52100 53800 4
{
T 52200 53800 5 10 1 1 0 1 1
netname=\_RESET
}
C 50300 46600 1 0 0 gnd-1.sym
N 50400 47100 50600 47100 4
N 50400 46900 50400 50400 4
N 50400 50400 50600 50400 4
N 50400 47700 50600 47700 4
C 46800 50100 1 0 0 3.3V-plus-1.sym
N 50600 49800 49800 49800 4
N 49500 49500 49500 54800 4
N 49800 51700 56200 51700 4
N 56200 51700 56200 56300 4
N 55800 52000 55800 56600 4
N 55800 52000 49500 52000 4
N 49500 49500 50600 49500 4
C 56800 44900 1 90 0 resistor-1.sym
{
T 56400 45200 5 10 0 0 90 0 1
device=RESISTOR
T 56900 45400 5 10 1 1 0 1 1
refdes=R4
T 56900 45200 5 10 1 1 0 1 1
value=330
T 56800 44900 5 10 0 0 90 0 1
footprint=0805
}
N 56700 44900 56700 44800 4
N 54500 48600 56700 48600 4
N 56700 48600 56700 45800 4
C 49900 43900 1 270 1 led-3.sym
{
T 49900 43900 5 10 0 0 180 2 1
footprint=0805
T 49750 44850 5 10 1 1 270 5 1
device=YELLOW (RSSI) LED
T 50450 44350 5 10 1 1 270 6 1
refdes=D3
}
N 50100 44900 50100 44800 4
C 50200 44900 1 90 0 resistor-1.sym
{
T 49800 45200 5 10 0 0 90 0 1
device=RESISTOR
T 50300 45400 5 10 1 1 0 1 1
refdes=R5
T 50300 45200 5 10 1 1 0 1 1
value=330
T 50200 44900 5 10 0 0 90 0 1
footprint=0805
}
N 50600 48600 50100 48600 4
N 50100 48600 50100 45800 4
N 51800 54600 53100 54600 4
N 53100 54600 53100 51700 4
N 50600 48000 49200 48000 4
N 49200 47700 49200 54200 4
N 49200 54200 50400 54200 4
C 49300 46300 1 0 1 gnd-1.sym
N 49200 46600 49200 46800 4
C 50200 43500 1 0 1 gnd-1.sym
N 50100 43800 50100 43900 4
N 54500 48900 55000 48900 4
N 55000 48900 55000 56900 4
N 55000 54200 51800 54200 4
C 49600 54800 1 90 0 resistor-1.sym
{
T 49200 55100 5 10 0 0 90 0 1
device=RESISTOR
T 49700 55300 5 10 1 1 0 1 1
refdes=R6
T 49700 55100 5 10 1 1 0 1 1
value=10k
T 49600 54800 5 10 0 0 90 0 1
footprint=0805
}
C 49300 55700 1 0 0 3.3V-plus-1.sym
T 50800 53000 9 10 1 0 0 0 2
XBee Prog 
Header
C 66500 58800 1 90 0 resistor-1.sym
{
T 66100 59100 5 10 0 0 90 0 1
device=RESISTOR
T 66600 59300 5 10 1 1 0 1 1
refdes=R7
T 66600 59100 5 10 1 1 0 1 1
value=10k
T 66500 58800 5 10 0 0 90 0 1
footprint=0805
}
C 66200 59700 1 0 0 3.3V-plus-1.sym
N 65300 58800 66800 58800 4
{
T 66900 58800 5 10 1 1 0 1 1
netname=/RESET
}
N 55400 48000 55400 57200 4
N 54500 48000 55400 48000 4
N 61900 54000 61900 53500 4
{
T 61900 53400 5 10 1 1 90 7 1
netname=SCL
}
C 61300 46400 1 0 0 oled-128x64-1.sym
{
T 63700 50800 5 10 0 0 0 0 1
device=XBEE
T 62450 50000 5 10 1 1 0 3 1
refdes=DISP1
T 63800 50600 5 10 0 0 0 0 1
footprint=OLED_096IN
}
C 61400 48900 1 0 0 gnd-1.sym
N 61500 49200 62000 49200 4
N 62000 49200 62000 48700 4
C 60900 49800 1 0 0 3.3V-plus-1.sym
N 61100 49800 61100 48400 4
N 61100 48400 62000 48400 4
N 62000 48100 59200 48100 4
{
T 59100 48100 5 10 1 1 0 7 1
netname=SCL
}
N 62000 47800 59200 47800 4
{
T 59100 47800 5 10 1 1 0 7 1
netname=SDA
}
N 62800 55100 63300 55100 4
{
T 63400 55100 5 10 1 1 0 1 1
netname=/BOOTLOAD
}
N 67800 56600 67300 56600 4
{
T 67200 56600 5 10 1 1 0 7 1
netname=/BOOTLOAD
}
C 66000 58600 1 270 0 switch-pushbutton-no-1-4p.sym
{
T 66600 58200 5 10 1 1 0 0 1
refdes=SW2
T 66700 58200 5 10 0 0 270 0 1
device=SWITCH_PUSHBUTTON_NO
T 67100 58200 5 10 0 0 270 0 1
footprint=TYCO_fsmjsma
}
N 65300 57400 69400 57400 4
N 66000 58600 66200 58600 4
N 66200 58600 66200 58800 4
C 65100 58700 1 270 0 capacitor-1.sym
{
T 65800 58500 5 10 0 1 270 0 1
device=CAPACITOR
T 65400 58400 5 10 1 1 0 0 1
refdes=C7
T 66000 58500 5 10 0 0 270 0 1
symversion=0.1
T 65100 57900 5 10 1 1 0 6 1
value=1uF
T 65100 58700 5 10 0 0 0 0 1
footprint=0805
T 65100 57700 5 10 1 1 0 6 1
description=16V
}
N 65300 57400 65300 57800 4
N 65300 58700 65300 58800 4
T 66600 57900 9 10 1 0 0 0 1
RESET Switch
T 69600 56600 9 10 1 0 0 1 1
Bootloader Mode
T 69600 56300 9 10 1 0 0 1 1
Firmware Factory Reset
N 67800 56300 67300 56300 4
{
T 67200 56300 5 10 1 1 0 7 1
netname=/FW_RESET
}
N 66000 57600 66200 57600 4
N 66200 57600 66200 57400 4
T 69600 54500 9 10 1 0 0 1 1
Base Addr - 1
T 69600 54800 9 10 1 0 0 1 1
Base Addr - 2
T 69600 55100 9 10 1 0 0 1 1
Base Addr - 4
T 69600 55400 9 10 1 0 0 1 1
Base Addr - 8
T 69600 55700 9 10 1 0 0 1 1
Base Addr - 16
N 67800 54500 67300 54500 4
{
T 67200 54500 5 10 1 1 0 7 1
netname=SW1
}
N 67800 54800 67300 54800 4
{
T 67200 54800 5 10 1 1 0 7 1
netname=SW2
}
N 67800 55100 67300 55100 4
{
T 67200 55100 5 10 1 1 0 7 1
netname=SW3
}
N 67800 55400 67300 55400 4
{
T 67200 55400 5 10 1 1 0 7 1
netname=SW4
}
N 67800 55700 67300 55700 4
{
T 67200 55700 5 10 1 1 0 7 1
netname=SW5
}
N 67800 56000 67300 56000 4
{
T 67200 56000 5 10 1 1 0 7 1
netname=SW6
}
C 65600 51500 1 0 0 ws2812b-1.sym
{
T 67500 52995 5 10 1 1 0 0 1
refdes=U4
T 65600 52595 5 10 0 1 0 0 1
footprint=WS2812
T 65800 52995 5 10 1 1 0 0 1
device=WS2812B
}
C 66600 53200 1 0 0 3.3V-plus-1.sym
N 62200 52300 65700 52300 4
C 66700 51200 1 0 0 gnd-1.sym
T 67300 51200 9 10 1 0 0 0 2
CIRCUITPYTHON
DIAG LED
C 58300 54000 1 0 0 esp32s2-solo-1.sym
{
T 60700 58400 5 10 0 0 0 0 1
device=XBEE
T 60550 57300 5 10 1 1 0 3 1
refdes=U1
T 60800 58200 5 10 0 0 0 0 1
footprint=ESP32_S2_SOLO
}
N 62200 54000 62200 52300 4
N 58300 58700 57800 58700 4
{
T 57700 58700 5 10 1 1 0 7 1
netname=/RESET
}
N 55400 57200 58300 57200 4
N 58300 56900 55000 56900 4
N 58300 56600 55800 56600 4
N 58300 56300 56200 56300 4
N 60700 54000 60700 53500 4
{
T 60700 53400 5 10 1 1 90 7 1
netname=SW5
}
N 61300 54000 61300 53500 4
{
T 61300 53400 5 10 1 1 90 7 1
netname=/FW_RESET
}
N 61000 54000 61000 53500 4
{
T 61000 53400 5 10 1 1 90 7 1
netname=SW6
}
N 60400 54000 60400 53500 4
{
T 60400 53400 5 10 1 1 90 7 1
netname=SW4
}
N 59500 54000 59500 53500 4
{
T 59500 53400 5 10 1 1 90 7 1
netname=SW1
}
N 59800 54000 59800 53500 4
{
T 59800 53400 5 10 1 1 90 7 1
netname=SW2
}
N 60100 54000 60100 53500 4
{
T 60100 53400 5 10 1 1 90 7 1
netname=SW3
}
N 61600 54000 61600 53500 4
{
T 61600 53400 5 10 1 1 90 7 1
netname=SDA
}
C 59700 48400 1 270 1 resistor-1.sym
{
T 60100 48700 5 10 0 0 90 2 1
device=RESISTOR
T 59600 48900 5 10 1 1 0 7 1
refdes=R1
T 59600 48700 5 10 1 1 0 7 1
value=2k
T 59700 48400 5 10 0 0 90 2 1
footprint=0805
}
N 59800 48400 59800 48100 4
N 59800 49300 59800 49600 4
N 59800 49600 61100 49600 4
C 60400 48400 1 270 1 resistor-1.sym
{
T 60800 48700 5 10 0 0 90 2 1
device=RESISTOR
T 60300 48900 5 10 1 1 0 7 1
refdes=R2
T 60300 48700 5 10 1 1 0 7 1
value=2k
T 60400 48400 5 10 0 0 90 2 1
footprint=0805
}
N 60500 49300 60500 49600 4
N 60500 48400 60500 47800 4
C 41800 56800 1 0 0 usb-minib.sym
{
T 41900 59100 5 10 1 1 0 0 1
refdes=J1
T 41800 56800 5 10 0 0 0 0 1
footprint=EDAC_690-005-299-043
}
C 49900 58200 1 0 0 az1117i-1.sym
{
T 51200 59250 5 10 1 1 0 0 1
refdes=U2
T 51000 59250 5 10 1 1 0 6 1
device=AZ1117I
T 51100 58400 5 10 1 1 0 2 1
footprint=SOT223
}
N 51700 59000 52000 59000 4
N 52000 59000 52000 58800 4
N 49800 49800 49800 51700 4
