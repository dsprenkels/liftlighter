MCU=atmega8
AVRDUDEMCU=m8
CC=avr-gcc
CFLAGS += -std=c99 -pedantic -Wall -Wshadow -Wpointer-arith \
         -Wcast-qual -Wformat-security \
         -g -O2 -mcall-prologues -mmcu=$(MCU)
OBJ2HEX=/usr/bin/avr-objcopy
AVRDUDE=/usr/local/bin/avrdude
EXECUTABLE=main
BAUDRATE=9600
RESET=25

all: $(EXECUTABLE)

$(EXECUTABLE): main.c random.o

$(EXECUTABLE).hex: $(EXECUTABLE)
	$(OBJ2HEX) -R .eeprom -O ihex $(EXECUTABLE) $(EXECUTABLE).hex

.PHONY: start
start: flash
	sudo gpio -g mode $(RESET) out
	sudo gpio -g write $(RESET) 1

.PHONY: stop
stop:
	sudo gpio -g mode $(RESET) out
	sudo gpio -g write $(RESET) 0

.PHONY: size
size: $(EXECUTABLE)
	avr-size $(EXECUTABLE)

flash: all
	sudo $(AVRDUDE) -p $(AVRDUDEMCU) -P /dev/spidev0.0 -c linuxspi -b $(BAUDRATE) -U flash:w:$(TARGET).hex:i

fuse:
	sudo $(AVRDUDE) -p $(AVRDUDEMCU) -P /dev/spidev0.0 -c linuxspi -b $(BAUDRATE) -U lfuse:w:0xe1:m -U hfuse:w:0xd9:m

.PHONY: clean
clean:
	rm -f $(TARGET) $(TARGET).hex *.obj *.o
