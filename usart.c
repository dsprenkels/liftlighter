#include <avr/io.h>
#include <string.h>

void usart_transmit_buf(const uint8_t *buf, const size_t len) {
	int i;

	for (i = 0; i < len; i++) {
		// wait until transmit buffer is empty
		while (! (UCSRA & (1 << UDRE))) {}

		// put char in transmit buffer
		UDR = buf[i];
	}
}


void usart_transmit_str(const char *src) {
	const size_t len = strlen(src);
	usart_transmit_buf((const uint8_t*) src, len);
}
