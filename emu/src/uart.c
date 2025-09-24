#include "memory_map.h"
#include "uart.h"

static bool uart_read(Device *d, uint32_t offset, unsigned size, uint32_t *out)
{
    (void)d;
    if (offset != UART_STATUS || size != 4)
        return false;
    *out = UART_STATUS_TXRDY;           /* transmitter is never busy */
    return true;
}

static bool uart_write(Device *d, uint32_t offset, unsigned size, uint32_t value)
{
    (void)size;                         /* TX accepts any size; low byte is sent */
    if (offset != UART_TX)
        return false;
    fputc((int)(value & 0xFFu), ((Uart *)d->state)->out);
    return true;
}

void uart_init(Uart *u, FILE *out)
{
    u->out = out;
    u->dev = (Device){ .name = "uart", .base = UART_BASE, .size = UART_SIZE,
                       .read = uart_read, .write = uart_write, .state = u };
}
