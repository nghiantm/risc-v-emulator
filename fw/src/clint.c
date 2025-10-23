#include "clint.h"
#include "memory_map.h"

#define REG(off) ((volatile uint32_t *)(CLINT_BASE + (off)))

Mtime64 clint_read_mtime(void)
{
    /* Re-read hi until stable so a carry between the two reads cannot tear the value. */
    uint32_t hi, lo, hi2 = *REG(CLINT_MTIME_HI);
    do {
        hi = hi2;
        lo = *REG(CLINT_MTIME_LO);
        hi2 = *REG(CLINT_MTIME_HI);
    } while (hi != hi2);
    return (Mtime64){ lo, hi };
}

/* mtimecmp is written as two words. Writing lo first could briefly leave a new lo with
 * an old (small) hi, i.e. a compare value in the past, and raise a spurious interrupt.
 * So: park hi at all-ones (compare is huge), set lo, then set the real hi. */
static void set_mtimecmp(uint32_t lo, uint32_t hi)
{
    *REG(CLINT_MTIMECMP_HI) = 0xFFFFFFFFu;
    *REG(CLINT_MTIMECMP_LO) = lo;
    *REG(CLINT_MTIMECMP_HI) = hi;
}

void clint_set_timer_after(uint32_t delta)
{
    Mtime64 now = clint_read_mtime();
    uint32_t lo = now.lo + delta;
    uint32_t hi = now.hi + (lo < now.lo);          /* carry out of the low word */
    set_mtimecmp(lo, hi);
}

void clint_disable_timer(void)
{
    set_mtimecmp(0xFFFFFFFFu, 0xFFFFFFFFu);
}
