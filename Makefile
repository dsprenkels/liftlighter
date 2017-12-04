MCU=atmega8
AVRDUDEMCU=m8
CC=avr-gcc
CFLAGS += -std=c99 -pedantic -Wall -Wshadow -Wpointer-arith \
         -Wcast-qual -Wformat-security \
         -g -O2 -mcall-prologues -mmcu=$(MCU)
OBJ2HEX=/usr/bin/avr-objcopy
AVRDUDE=/usr/local/bin/avrdude
TARGET=main
BAUDRATE=9600
RESET=25

all: $(TARGET)

$(TARGET): main.c random.o nl_dst.o
nl_dst.o: nl_dst.c nl_dst.h
random.o: random.c random.h

$(TARGET).hex: $(TARGET)
	$(OBJ2HEX) -j .text -j .data -O ihex $(TARGET) $(TARGET).hex

$(TARGET).eep: $(TARGET)
	$(OBJ2HEX) -j .eeprom --change-section-lma .eeprom=0 -O ihex $(TARGET) $(TARGET).eep

.PHONY: start
start: flash
	sudo gpio -g mode $(RESET) out
	sudo gpio -g write $(RESET) 1

.PHONY: stop
stop:
	sudo gpio -g mode $(RESET) out
	sudo gpio -g write $(RESET) 0

.PHONY: size
size: $(TARGET)
	avr-size $(TARGET)

flash: all
	sudo $(AVRDUDE) -p $(AVRDUDEMCU) -P /dev/spidev0.0 -c linuxspi -b $(BAUDRATE) -U flash:w:$(TARGET).hex:i -U eeprom:w:eeprom.hex

fuse:
	sudo $(AVRDUDE) -p $(AVRDUDEMCU) -P /dev/spidev0.0 -c linuxspi -b $(BAUDRATE) -U lfuse:w:0xe1:m -U hfuse:w:0xd9:m

.PHONY: clean
clean:
	rm -f $(TARGET) $(TARGET).hex *.obj *.o
