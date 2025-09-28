#ifndef TRACE_H
#define TRACE_H

#include <stdio.h>

#include "cpu.h"

/* One line per executed instruction: "<pc>: <insn>" plus " x<n>=<value>" if a register changed. */
void trace_step(FILE *out, const StepInfo *s);

#endif /* TRACE_H */
