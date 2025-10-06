#include "trace.h"

void trace_step(FILE *out, const StepInfo *s)
{
    fprintf(out, "%08x: %08x", (unsigned)s->pc, (unsigned)s->insn);
    if (s->wrote)
        fprintf(out, " x%u=%08x", (unsigned)s->rd, (unsigned)s->value);
    fputc('\n', out);
}

void trace_trap(FILE *out, uint32_t cause, uint32_t tval, uint32_t handler_pc)
{
    fprintf(out, "TRAP cause=0x%x tval=0x%x -> pc=0x%x\n", (unsigned)cause, (unsigned)tval, (unsigned)handler_pc);
}
