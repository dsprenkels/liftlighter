MCU=atmega8
AVRDUDEMCU=m8
CC=avr-gcc
CFLAGS=-Wall -g -O2 -mcall-prologues -mmcu=$(MCU)
OBJ2HEX=/usr/bin/avr-objcopy
AVRDUDE=/usr/local/bin/avrdude
EXECUTABLE=main
BAUDRATE=9600
RESET=25

all: $(EXECUTABLE)

$(EXECUTABLE): main.c random.o usart.o

$(EXECUTABLE).hex: $(EXECUTABLE)
	$(OBJ2HEX) -R .eeprom -O ihex $(EXECUTABLE) $(EXECUTABLE).hex

.fuzzy: start
start: flash
	sudo gpio -g mode $(RESET) out
	sudo gpio -g write $(RESET) 1

.fuzzy: stop
stop:
	sudo gpio -g mode $(RESET) out
	sudo gpio -g write $(RESET) 0

flash: all
	sudo $(AVRDUDE) -p $(AVRDUDEMCU) -P /dev/spidev0.0 -c linuxspi -b $(BAUDRATE) -U flash:w:$(TARGET).hex:i

fuse:
	sudo $(AVRDUDE) -p $(AVRDUDEMCU) -P /dev/spidev0.0 -c linuxspi -b $(BAUDRATE) -U lfuse:w:0xe1:m -U hfuse:w:0xd9:m

.fuzzy: clean
clean:
	rm -f $(TARGET) $(TARGET).hex *.obj *.o
