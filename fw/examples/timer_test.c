/* Takes N timer interrupts, each re-armed by the handler, and passes only if exactly N arrived. */
#include <stdint.h>

#include "clint.h"
#include "trap.h"
#include "uart.h"

#define N_INTERRUPTS 5u
#define PERIOD_TICKS 1000u
#define WAIT_LIMIT   1000000u     /* bounded: a broken timer must fail, not hang */

#define CAUSE_MTIMER 0x80000007u

static volatile uint32_t count;

static void handler(uint32_t cause, uint32_t epc)
{
    (void)epc;
    if (cause != CAUSE_MTIMER)
        return;
    count++;
    if (count < N_INTERRUPTS)
        clint_set_timer_after(PERIOD_TICKS);
    else
        clint_disable_timer();          /* last one: stop MTIP or the handler would re-enter forever */
}

int main(void)
{
    trap_install(handler);
    irq_enable_timer();
    clint_set_timer_after(PERIOD_TICKS);
    irq_global_enable();

    for (uint32_t i = 0; i < WAIT_LIMIT && count < N_INTERRUPTS; i++)
        ;
    irq_global_disable();

    uart_puts("timer interrupts: ");
    uart_put_dec(count);
    uart_puts("/");
    uart_put_dec(N_INTERRUPTS);
    uart_puts("\n");
    return count == N_INTERRUPTS ? 0 : 1;
}
