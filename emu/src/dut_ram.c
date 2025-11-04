#include <stdlib.h>

#include "dut_ram.h"
#include "memory_map.h"

/* addr_stuck: address line K reads as 0 on every access, so offsets with and without bit K alias. */
static uint32_t effective_offset(const DutRam *r, uint32_t offset)
{
    if (!r->faults)
        return offset;
    for (size_t i = 0; i < r->faults->count; i++)
        if (r->faults->items[i].kind == FAULT_RAM_ADDR_STUCK)
            offset &= ~(1u << r->faults->items[i].line);
    return offset;
}

/* The stored 32-bit word containing byte `offset`, with the read-side faults applied.
 * Sub-word reads take their bytes from this word, so a stuck bit shows up at every access size. */
static uint32_t faulted_word(const DutRam *r, uint32_t offset)
{
    uint32_t base = offset & ~3u;
    uint32_t w = 0;
    for (unsigned i = 0; i < 4; i++)
        w |= (uint32_t)r->mem[base + i] << (8 * i);
    if (!r->faults)
        return w;
    for (size_t i = 0; i < r->faults->count; i++) {
        const Fault *f = &r->faults->items[i];
        switch (f->kind) {
        case FAULT_RAM_DATA_STUCK0: w &= ~(1u << f->bit); break;
        case FAULT_RAM_DATA_STUCK1: w |=  (1u << f->bit); break;
        case FAULT_RAM_BITFLIP:
            if (((f->addr - DUT_RAM_BASE) & ~3u) == base)
                w ^= 1u << f->bit;
            break;
        default: break;
        }
    }
    return w;
}

static bool dut_read(Device *d, uint32_t offset, unsigned size, uint32_t *out)
{
    const DutRam *r = d->state;
    uint32_t v = 0;
    for (unsigned i = 0; i < size; i++) {
        uint32_t eff = effective_offset(r, offset + i);
        v |= ((faulted_word(r, eff) >> (8 * (eff & 3u))) & 0xFFu) << (8 * i);
    }
    *out = v;
    return true;
}

static bool dut_write(Device *d, uint32_t offset, unsigned size, uint32_t value)
{
    DutRam *r = d->state;
    r->write_count++;                                  /* counted even when the write is then dropped */
    if (r->faults)
        for (size_t i = 0; i < r->faults->count; i++) {
            const Fault *f = &r->faults->items[i];
            if (f->kind == FAULT_RAM_DROP_WRITES && r->write_count % f->every == 0)
                return true;                           /* the bus sees success; the data is lost */
        }
    for (unsigned i = 0; i < size; i++)
        r->mem[effective_offset(r, offset + i)] = (uint8_t)(value >> (8 * i));
    return true;
}

bool dut_ram_init(DutRam *r, const FaultSet *faults)
{
    *r = (DutRam){ .faults = faults };
    r->mem = calloc(DUT_RAM_SIZE, 1);
    if (!r->mem)
        return false;
    r->dev = (Device){ .name = "dut_ram", .base = DUT_RAM_BASE, .size = DUT_RAM_SIZE,
                       .read = dut_read, .write = dut_write, .state = r };
    return true;
}

void dut_ram_free(DutRam *r)
{
    free(r->mem);
    r->mem = NULL;
}
