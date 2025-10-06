#ifndef TRACE_H
#define TRACE_H

#include <stdio.h>

#include "cpu.h"

/* One line per executed instruction: "<pc>: <insn>" plus " x<n>=<value>" if a register changed. */
void trace_step(FILE *out, const StepInfo *s);

/* "TRAP cause=0x<hex> tval=0x<hex> -> pc=0x<hex>" where pc is the handler address. */
void trace_trap(FILE *out, uint32_t cause, uint32_t tval, uint32_t handler_pc);

#endif /* TRACE_H */
