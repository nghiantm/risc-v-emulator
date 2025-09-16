#include "bus.h"

void bus_init(Bus *b)
{
    *b = (Bus){0};
}

bool bus_add(Bus *b, Device *d)
{
    if (b->count == BUS_MAX_DEVICES)
        return false;
    b->devices[b->count++] = d;
    return true;
}

/* The device holding the whole access [addr, addr+size), or NULL if the
 * address is unmapped or the access runs past the device's end (which also
 * covers straddling two devices: nothing is touched in either). */
static Device *find_device(Bus *b, uint32_t addr, unsigned size_bytes)
{
    if (size_bytes != 1 && size_bytes != 2 && size_bytes != 4)
        return NULL;
    for (size_t i = 0; i < b->count; i++) {
        Device *d = b->devices[i];
        uint64_t end = (uint64_t)d->base + d->size;
        if (addr >= d->base && addr < end)
            return (uint64_t)addr + size_bytes <= end ? d : NULL;
    }
    return NULL;
}

bool bus_read(Bus *b, uint32_t addr, unsigned size_bytes, uint32_t *out)
{
    Device *d = find_device(b, addr, size_bytes);
    return d && d->read && d->read(d, addr - d->base, size_bytes, out);
}

bool bus_write(Bus *b, uint32_t addr, unsigned size_bytes, uint32_t value)
{
    Device *d = find_device(b, addr, size_bytes);
    if (!d || !d->write || !d->write(d, addr - d->base, size_bytes, value))
        return false;
    /* The store still completes to RAM; the first 4-byte store to tohost is
     * remembered so the machine loop can stop on it. */
    if (b->tohost_enabled && !b->tohost_written && size_bytes == 4 && addr == b->tohost_addr) {
        b->tohost_written = true;
        b->tohost_value = value;
    }
    return true;
}

void bus_tick(Bus *b)
{
    for (size_t i = 0; i < b->count; i++)
        if (b->devices[i]->tick)
            b->devices[i]->tick(b->devices[i]);
}
