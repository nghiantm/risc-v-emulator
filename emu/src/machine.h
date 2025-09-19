#ifndef MACHINE_H
#define MACHINE_H

#include <stdbool.h>
#include <stdint.h>

#include "bus.h"

typedef enum { EXIT_NONE = 0, EXIT_PASS, EXIT_FAIL } ExitKind;

typedef struct {
    ExitKind kind;        /* EXIT_NONE until a device/tohost sets it; first request wins */
    uint32_t code;        /* tohost: value >> 1 (test number).  SYSCON: the (value >> 16) fail code. 0 for pass. */
    const char *source;   /* "tohost" or "syscon" — for the RESULT line */
} ExitRequest;

typedef enum {            /* values equal the process exit codes */
    RUN_PASS = 0, RUN_FAIL = 1, RUN_TIMEOUT = 2, RUN_ERROR = 3
} RunResult;

/* Fields are added as the modules that own them appear (cpu, clint, sensor, faults). */
typedef struct {
    Bus bus;
    ExitRequest exit_req;
    bool trace;
} Machine;

/* First request wins. */
static inline void exit_request(ExitRequest *r, ExitKind kind, uint32_t code, const char *source)
{
    if (r->kind != EXIT_NONE)
        return;
    r->kind = kind;
    r->code = code;
    r->source = source;
}

#endif /* MACHINE_H */
