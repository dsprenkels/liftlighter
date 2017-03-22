#include <avr/io.h>
#include <string.h>

void usart_transmit_buf(const char *buf, const int len) {
	int i;

	for (i = 0; i < len; i++) {
		// wait until transmit buffer is empty
		while (! (UCSRA & (1 << UDRE))) {}

		// put char in transmit buffer
		UDR = buf[i];
	}
}


void usart_transmit_str(const char *src) {
	const int len = strlen(src);
	return usart_transmit_buf(src, len);
}
