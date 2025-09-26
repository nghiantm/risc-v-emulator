#ifndef EXIT_REQUEST_H
#define EXIT_REQUEST_H

#include <stdint.h>

typedef enum { EXIT_NONE = 0, EXIT_PASS, EXIT_FAIL } ExitKind;

typedef struct {
    ExitKind kind;        /* EXIT_NONE until a device/tohost sets it; first request wins */
    uint32_t code;        /* tohost: value >> 1 (test number).  SYSCON: the (value >> 16) fail code. 0 for pass. */
    const char *source;   /* "tohost" or "syscon" — for the RESULT line */
} ExitRequest;

/* First request wins. */
static inline void exit_request(ExitRequest *r, ExitKind kind, uint32_t code, const char *source)
{
    if (r->kind != EXIT_NONE)
        return;
    r->kind = kind;
    r->code = code;
    r->source = source;
}

#endif /* EXIT_REQUEST_H */
