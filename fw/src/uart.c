#include "memory_map.h"
#include "uart.h"

#define UART_TX_REG     ((volatile uint32_t *)(UART_BASE + UART_TX))
#define UART_STATUS_REG ((volatile uint32_t *)(UART_BASE + UART_STATUS))
#define TX_WAIT_LIMIT   1000u    /* the emulated UART is always ready; the bound is just discipline */

void uart_putc(char c)
{
    for (uint32_t i = 0; i < TX_WAIT_LIMIT; i++)
        if (*UART_STATUS_REG & UART_STATUS_TXRDY)
            break;
    *UART_TX_REG = (uint8_t)c;       /* send even if the wait timed out: better a garbled byte than a hang */
}

void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

void uart_put_hex32(uint32_t v)
{
    for (int shift = 28; shift >= 0; shift -= 4)
        uart_putc("0123456789abcdef"[(v >> shift) & 0xFu]);
}

void uart_put_dec(uint32_t v)
{
    char digits[10];                 /* 2^32 - 1 has 10 digits */
    int n = 0;
    do {
        digits[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
        uart_putc(digits[--n]);
}
