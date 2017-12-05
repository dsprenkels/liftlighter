#ifndef UART_H_
#define UART_H_

#include <avr/io.h>
#include <unistd.h>

// Will init with a baud rate of 9600
void UART_init();

// Send one character
void UART_transmit(unsigned char data);

// Send a byte buffer
void UART_send_buf(uint8_t *buf, size_t len);

// Send a string
void UART_send_str(char *str);
void UART_send_strn(char *str, size_t n);

#endif /* UART_H_ */
