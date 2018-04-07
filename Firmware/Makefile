# Name: Makefile
# Author:    Alfredo Orozco de la Paz
# Copyright: Copyright ©2018 Alfredo Orozco de la Paz
# License:   GNU GPL v2 (see License.txt), GNU GPL v3

FREDISP_SERIALPROD_NUM = \'O\',\'P\',\'E\',\'N\'

PROJECT_NAME = FredISP
DEVICE       = attiny44
#CLOCK        = 12000000
#CLOCK        = 20000000
CLOCK        = 12000000
PROGRAMMER   = avrispmkII
FUSES        = -U hfuse:w:0xDF:m -U lfuse:w:0xFF:m #-U efuse:w:0xFF:m
AVRDUDE      = avrdude -c $(PROGRAMMER) -p $(DEVICE)


OBJECT_FILES = ./include/usbdrv/usbdrv.o
OBJECT_FILES += ./include/usbdrv/usbdrvasm.o
OBJECT_FILES += ./include/usbdrv/oddebug.o
OBJECT_FILES += main.o

INCLUDEPATHS =  -I ./
INCLUDEPATHS += -I ./include
INCLUDEPATHS += -I ./include/usbdrv

CFLAGS = -ffunction-sections -fpermissive
LDFLAGS = -Wl,-gc-sections

COMPILE = avr-gcc $(CFLAGS) $(LDFLAGS) -Wall -Os -DF_CPU=$(CLOCK) -mmcu=$(DEVICE) $(INCLUDEPATHS) -DPROD_NUMBER=$(FREDISP_SERIALPROD_NUM)

# symbolic targets:
all: $(PROJECT_NAME).hex

.c.o:
	$(COMPILE) -c $< -o $@

.cpp.o:
	$(COMPILE) -c $< -o $@

.S.o:
	$(COMPILE) -x assembler-with-cpp -c $< -o $@

.c.s:
	$(COMPILE) -S $< -o $@

.cpp.s:
	$(COMPILE) -S $< -o $@

flash:	all
	$(AVRDUDE) -U flash:w:$(PROJECT_NAME).hex:i

fuse:
	$(AVRDUDE) $(FUSES)

erase:
	$(AVRDUDE) -e

reset:
	$(AVRDUDE)

install: flash fuse

clean:
	rm -f $(PROJECT_NAME).hex $(PROJECT_NAME).elf $(OBJECT_FILES)

$(PROJECT_NAME).elf: $(OBJECT_FILES)
	$(COMPILE) -o $(PROJECT_NAME).elf $(OBJECT_FILES)

$(PROJECT_NAME).hex: $(PROJECT_NAME).elf
	rm -f $(PROJECT_NAME).hex
	avr-objcopy -j .text -j .data -O ihex $(PROJECT_NAME).elf $(PROJECT_NAME).hex
	avr-size --format=avr --mcu=$(DEVICE) $(PROJECT_NAME).elf

disasm:	$(PROJECT_NAME).elf
	avr-objdump -d $(PROJECT_NAME).elf

cpp:
	$(COMPILE) -E main.c
