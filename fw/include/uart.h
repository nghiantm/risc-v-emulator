#ifndef FW_UART_H
#define FW_UART_H

#include <stdint.h>

void uart_putc(char c);
void uart_puts(const char *s);
void uart_put_hex32(uint32_t v);   /* exactly 8 lowercase hex digits, no prefix */
void uart_put_dec(uint32_t v);

#endif
