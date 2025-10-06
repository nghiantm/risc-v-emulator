#ifndef TRAP_H
#define TRAP_H

#include <stdint.h>

#include "cpu.h"

/* Enter a trap handler: save pc/cause/tval, push MIE into MPIE, jump to mtvec. `pc` is the
 * instruction that trapped (or, for an interrupt, the one that did not run). */
void trap_enter(Cpu *cpu, uint32_t cause, uint32_t tval, uint32_t pc);

/* mret: pop MPIE back into MIE. Returns the pc to resume at (mepc). */
uint32_t trap_return(Csr *csr);

#endif /* TRAP_H */
