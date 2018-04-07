
# FredISP Firmware

### Firmware for the USB ISP programmer FredISP

This folder contains the firmware for the FredISP v1.0 board. The projec was created whit APOS tool, and the code are based in the FabISP programmer and V-USB library.

To compile open the firmware folder in a terminal and type "make".
To install type "make install".

Note: you need to have installed the avr-gcc toolchain and the avrdude software.

The firmware run	 at 16 Mhz, but the frequency could be changed in the Makefile.

----

### Compile

1. Edit the DEVICE and CLOCK options in the makefile.
2. Enter the Firmware folder from the terminal and execute the "make" command.


There are compiled versions inside the "hex" folder for the attiny44 and attiny84 micros with frequencies of 12, 16 and 20 Mhz in case you do not want to compile the firmware. To install them just run the command:

```bash 
avrdude -c MYPROGRAMMER -p attiny44 -U flash:w:fredisp-1.0-t44-16Mhz.hex
```
---
### Install

1. Connect the FredISP hardware to another ISP programmer like USBASP, AVR-DRAGON, etc.
2. Edit the PROGRAMMER option in the makefile to configure your hardware ISP programmer.
3. Execute the command "make install" to download the firmware and program the fuses on the AVR microcontroller.