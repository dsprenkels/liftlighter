#include "uart.h"
#include <avr/io.h>
#include <unistd.h>

void UART_init()
{
	// Set double speed mode
	UCSRA |= (1 << U2X);

	// Set baud rate to 9600
	UBRRH = 0;
	UBRRL = 12;

	// Enable transmissions
	UCSRB = 1 << TXEN;

	// Set frame format
	UCSRC = (1<<URSEL)|(1<<USBS)|(3<<UCSZ0);
}

void UART_transmit(uint8_t data)
{
	// Wait for empty transmit buffer
	while ( !(UCSRA & (1<<UDRE)) );

	// Put data into buffer, send the character
	UDR = data;
}

void UART_send_buf(uint8_t *buf, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		UART_transmit(buf[i]);
	}
}

void UART_send_str(char *str) {
	for (size_t i = 0; str[i] != '\0'; i++) {
		UART_transmit(str[i]);
	}
}

void UART_send_strn(char *str, size_t n) {
	for (size_t i = 0; str[i] != '\0' && i < n; i++) {
		UART_transmit(str[i]);
	}
}
