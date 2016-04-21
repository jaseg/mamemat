
MCU 	 = atmega32u4
F_CPU  	 = 16000000
PORT     = /dev/serial/by-id/usb-Arduino_LLC_Arduino_Leonardo-if00

CFLAGS := -mmcu=$(MCU) -DF_CPU=$(F_CPU)UL -O2 -std=gnu99 -Wall -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums

all: main.elf

program: main.elf
	avrdude -vV -c avr109 -P $(PORT) -p $(MCU) -b 57600 -U flash:w:$<

main.elf: main.c usb_srs_hid.c
	avr-gcc $(CFLAGS) -o $@ $^
	avr-size $@

lst: main.lst usb_srs_hid.lst

%.lst: %.c
	avr-gcc -c -g -Wa,-a,-ad $(CFLAGS) $^ > $@

clean:
	rm -f main.elf 

.PHONY:	program clean lst
