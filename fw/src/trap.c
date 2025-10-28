#include "trap.h"

extern void trap_entry(void);

static trap_handler_t current_handler;

/* Called from trap_entry with the registers already saved. */
void trap_dispatch(uint32_t cause, uint32_t epc)
{
    if (current_handler)
        current_handler(cause, epc);
}

void trap_install(trap_handler_t handler)
{
    current_handler = handler;
    csr_write(mtvec, (uint32_t)trap_entry);
}
