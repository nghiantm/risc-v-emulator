#include <stdlib.h>

#include "ram.h"

/* Misaligned accesses just work: bytes are assembled little-endian one by one.
 * The bus has already guaranteed offset+size is inside the device. */
static bool ram_read(Device *d, uint32_t offset, unsigned size, uint32_t *out)
{
    const uint8_t *m = ((Ram *)d->state)->mem;
    uint32_t v = 0;
    for (unsigned i = 0; i < size; i++)
        v |= (uint32_t)m[offset + i] << (8 * i);
    *out = v;
    return true;
}

static bool ram_write(Device *d, uint32_t offset, unsigned size, uint32_t value)
{
    uint8_t *m = ((Ram *)d->state)->mem;
    for (unsigned i = 0; i < size; i++)
        m[offset + i] = (uint8_t)(value >> (8 * i));
    return true;
}

bool ram_init(Ram *r, const char *name, uint32_t base, uint32_t size)
{
    r->mem = calloc(size, 1);
    if (!r->mem)
        return false;
    r->dev = (Device){ .name = name, .base = base, .size = size,
                       .read = ram_read, .write = ram_write, .state = r };
    return true;
}

void ram_free(Ram *r)
{
    free(r->mem);
    r->mem = NULL;
}
