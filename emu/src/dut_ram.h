#ifndef DUT_RAM_H
#define DUT_RAM_H

#include "bus.h"
#include "fault.h"

/* RAM under test: plain storage plus the five RAM faults, applied on the access path. */
typedef struct {
    Device   dev;
    uint8_t *mem;
    const FaultSet *faults;      /* may be NULL; read on every access */
    uint64_t write_count;        /* write accesses seen, for ram_drop_writes */
} DutRam;

bool dut_ram_init(DutRam *r, const FaultSet *faults);   /* false on allocation failure */
void dut_ram_free(DutRam *r);

#endif /* DUT_RAM_H */
