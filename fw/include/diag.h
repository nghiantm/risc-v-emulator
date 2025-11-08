#ifndef FW_DIAG_H
#define FW_DIAG_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DIAG_DATA_BUS = 0, DIAG_ADDR_BUS, DIAG_MARCH_C_MINUS,
    DIAG_SENSOR_ID, DIAG_SENSOR_READY, DIAG_SENSOR_RANGE, DIAG_SENSOR_VARIATION, DIAG_SENSOR_IRQ,
    DIAG_COUNT                       /* enum value = bit index in the SYSCON fail mask */
} DiagId;

typedef struct { bool pass; const char *detail; } DiagResult;   /* detail: short, no newlines, "" on pass */

static inline DiagResult diag_pass(void) { return (DiagResult){ true, "" }; }
static inline DiagResult diag_fail(const char *detail) { return (DiagResult){ false, detail }; }

/* Prints "DIAG <name> PASS" or "DIAG <name> FAIL <detail>" and updates the tally. */
void diag_report(DiagId id, DiagResult r);
/* Prints the summary line and ends the run through SYSCON: pass if nothing failed,
 * else (mask << 16) | SYSCON_FAIL_TAG. */
void diag_finish(void) __attribute__((noreturn));

/* Builds a failure detail in one shared buffer (valid until the next diag_detail_begin).
 * Output is silently truncated rather than overflowing. */
void diag_detail_begin(void);
void diag_detail_str(const char *s);
void diag_detail_hex(uint32_t v);    /* "0x" + 8 hex digits */
void diag_detail_dec(uint32_t v);
const char *diag_detail_end(void);

#endif
