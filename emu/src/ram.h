#ifndef RAM_H
#define RAM_H

#include "bus.h"

typedef struct {
    Device   dev;
    uint8_t *mem;
} Ram;

/* Zero-filled byte array; any size, any alignment. false on allocation failure. */
bool ram_init(Ram *r, const char *name, uint32_t base, uint32_t size);
void ram_free(Ram *r);

#endif /* RAM_H */
