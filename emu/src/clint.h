#ifndef CLINT_H
#define CLINT_H

#include "bus.h"

typedef struct {
    Device   dev;
    uint64_t mtime;       /* +1 per machine-loop iteration */
    uint64_t mtimecmp;    /* reset to all ones: no interrupt until software arms it */
} Clint;

void clint_init(Clint *c);
bool clint_timer_pending(const Clint *c);    /* mtime >= mtimecmp, 64-bit unsigned */

#endif /* CLINT_H */
