#ifndef USART_H
#define USART_H

#include <avr/io.h>
#include <util/setbaud.h>

void usart_init()
{
	UBRRH = (unsigned char) UBRRH_VALUE; // set baud rate
	UBRRL = (unsigned char) UBRRL_VALUE;
	UCSRC = (1 << UCSZ1) | (1 << UCSZ0); // use 8-bit fragments
	UCSRB = (1 << TXEN); // enable only tx
	// do not enable any interrupts
}

void usart_transmit_buf(const uint8_t *buf, const int len);

void usart_transmit_str(const char *src);

#endif /* USAR_H */
