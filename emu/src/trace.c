#include "trace.h"

void trace_step(FILE *out, const StepInfo *s)
{
    fprintf(out, "%08x: %08x", (unsigned)s->pc, (unsigned)s->insn);
    if (s->wrote)
        fprintf(out, " x%u=%08x", (unsigned)s->rd, (unsigned)s->value);
    fputc('\n', out);
}
