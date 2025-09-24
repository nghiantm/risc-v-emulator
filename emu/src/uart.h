#ifndef UART_H
#define UART_H

#include <stdio.h>

#include "bus.h"

typedef struct {
    Device dev;
    FILE  *out;     /* TX bytes go here */
} Uart;

void uart_init(Uart *u, FILE *out);

#endif /* UART_H */
