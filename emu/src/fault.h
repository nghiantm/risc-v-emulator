#ifndef FAULT_H
#define FAULT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    FAULT_RAM_DATA_STUCK0, FAULT_RAM_DATA_STUCK1, FAULT_RAM_ADDR_STUCK,
    FAULT_RAM_BITFLIP,     FAULT_RAM_DROP_WRITES,
    FAULT_SENSOR_BAD_ID,   FAULT_SENSOR_STUCK,    FAULT_SENSOR_OUT_OF_RANGE,
    FAULT_SENSOR_NEVER_READY, FAULT_SENSOR_NO_IRQ
} FaultKind;

typedef struct {
    FaultKind kind;
    uint32_t  bit;     /* data_stuck0/1: 0..31; bitflip: 0..31 */
    uint32_t  line;    /* addr_stuck: byte-offset bit 2..15 (DUT_RAM is 64 KiB, word accesses) */
    uint32_t  addr;    /* bitflip: ABSOLUTE bus address inside DUT RAM */
    uint32_t  every;   /* drop_writes: N >= 1 */
} Fault;

#define FAULT_MAX 16
typedef struct { Fault items[FAULT_MAX]; size_t count; } FaultSet;

/* Parses "name" or "name:key=val,key=val" (values decimal or 0x hex).
 * Returns false and writes a message to err (size errlen) on unknown name,
 * missing/extra/duplicate/invalid parameter. */
bool fault_parse(const char *spec, Fault *out, char *err, size_t errlen);
bool fault_set_has(const FaultSet *fs, FaultKind k);

#endif /* FAULT_H */
