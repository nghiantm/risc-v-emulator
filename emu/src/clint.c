#include "clint.h"
#include "memory_map.h"

/* Which 64-bit register an offset belongs to and whether it is the high word.
 * false for any offset that is not one of the four registers. */
static bool locate(Clint *c, uint32_t offset, uint64_t **reg, bool *high)
{
    switch (offset) {
    case CLINT_MTIMECMP_LO: *reg = &c->mtimecmp; *high = false; return true;
    case CLINT_MTIMECMP_HI: *reg = &c->mtimecmp; *high = true;  return true;
    case CLINT_MTIME_LO:    *reg = &c->mtime;    *high = false; return true;
    case CLINT_MTIME_HI:    *reg = &c->mtime;    *high = true;  return true;
    default:                return false;
    }
}

static bool clint_read(Device *d, uint32_t offset, unsigned size, uint32_t *out)
{
    uint64_t *reg;
    bool high;
    if (size != 4 || !locate((Clint *)d->state, offset, &reg, &high))
        return false;
    *out = (uint32_t)(high ? *reg >> 32 : *reg);
    return true;
}

static bool clint_write(Device *d, uint32_t offset, unsigned size, uint32_t value)
{
    uint64_t *reg;
    bool high;
    if (size != 4 || !locate((Clint *)d->state, offset, &reg, &high))
        return false;
    if (high)
        *reg = (*reg & 0xFFFFFFFFull) | (uint64_t)value << 32;
    else
        *reg = (*reg & ~0xFFFFFFFFull) | value;
    return true;
}

static void clint_tick(Device *d)
{
    ((Clint *)d->state)->mtime++;
}

void clint_init(Clint *c)
{
    c->mtime = 0;
    c->mtimecmp = ~0ull;
    c->dev = (Device){ .name = "clint", .base = CLINT_BASE, .size = CLINT_SIZE,
                       .read = clint_read, .write = clint_write, .tick = clint_tick, .state = c };
}

bool clint_timer_pending(const Clint *c)
{
    return c->mtime >= c->mtimecmp;
}
