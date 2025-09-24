#ifndef SYSCON_H
#define SYSCON_H

#include "bus.h"
#include "machine.h"

typedef struct {
    Device       dev;
    ExitRequest *req;
} Syscon;

void syscon_init(Syscon *s, ExitRequest *req);

#endif /* SYSCON_H */
