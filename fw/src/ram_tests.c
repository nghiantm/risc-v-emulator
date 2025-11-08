#include "memory_map.h"
#include "ram_tests.h"

#define DUT_WORDS (DUT_RAM_SIZE / 4u)
#define DUT       ((volatile uint32_t *)DUT_RAM_BASE)

DiagResult ram_test_data_bus(void)
{
    volatile uint32_t *w = DUT;
    /* Walking 1s find a bit stuck at 0 (or shorted low); walking 0s find one stuck at 1. */
    for (int pass = 0; pass < 2; pass++) {
        for (uint32_t bit = 0; bit < 32; bit++) {
            uint32_t pattern = pass == 0 ? 1u << bit : ~(1u << bit);
            *w = pattern;
            uint32_t got = *w;
            if (got != pattern) {
                diag_detail_begin();
                diag_detail_str("wrote ");
                diag_detail_hex(pattern);
                diag_detail_str(" read ");
                diag_detail_hex(got);
                return diag_fail(diag_detail_end());
            }
        }
    }
    return diag_pass();
}

#define ADDR_BITS 14u                        /* word offsets 4<<k for k = 0..13 cover byte-offset bits 2..15 */
#define MARK_BACKGROUND 0xAAAAAAAAu
#define MARK_PROBE      0x55555555u

static uint32_t probe_offset(uint32_t i)     /* i = 0 -> offset 0; i = 1..14 -> 4 << (i-1) */
{
    return i == 0 ? 0 : 4u << (i - 1);
}

static uint32_t lowest_bit(uint32_t v)
{
    uint32_t n = 0;
    while (!(v & 1u)) {
        v >>= 1;
        n++;
    }
    return n;
}

DiagResult ram_test_addr_bus(void)
{
    /* Do not trust old contents: first stamp every probe location with the background value. */
    for (uint32_t i = 0; i <= ADDR_BITS; i++)
        *(volatile uint32_t *)(DUT_RAM_BASE + probe_offset(i)) = MARK_BACKGROUND;

    /* Then change one location at a time. If an address line is stuck, two offsets reach the same
     * cell, so changing one also changes the other. Each location is put back afterwards. */
    for (uint32_t p = 0; p <= ADDR_BITS; p++) {
        volatile uint32_t *pw = (volatile uint32_t *)(DUT_RAM_BASE + probe_offset(p));
        *pw = MARK_PROBE;
        for (uint32_t q = 0; q <= ADDR_BITS; q++) {
            if (q == p)
                continue;
            uint32_t got = *(volatile uint32_t *)(DUT_RAM_BASE + probe_offset(q));
            if (got != MARK_BACKGROUND) {
                diag_detail_begin();
                diag_detail_str("write to ");
                diag_detail_hex(DUT_RAM_BASE + probe_offset(p));
                diag_detail_str(" disturbed ");
                diag_detail_hex(DUT_RAM_BASE + probe_offset(q));
                diag_detail_str(" (differ at addr bit ");
                diag_detail_dec(lowest_bit(probe_offset(p) ^ probe_offset(q)));
                diag_detail_str(")");
                return diag_fail(diag_detail_end());
            }
        }
        *pw = MARK_BACKGROUND;
    }
    return diag_pass();
}

static DiagResult march_mismatch(uint32_t index, uint32_t expected, uint32_t got)
{
    diag_detail_begin();
    diag_detail_str("addr ");
    diag_detail_hex(DUT_RAM_BASE + 4u * index);
    diag_detail_str(" expected ");
    diag_detail_hex(expected);
    diag_detail_str(" got ");
    diag_detail_hex(got);
    return diag_fail(diag_detail_end());
}

/* March C-: up(w0) up(r0,w1) up(r1,w0) down(r0,w1) down(r1,w0) down(r0).
 * 0 and 1 are the whole-word values 0x00000000 and 0xFFFFFFFF, so every bit sees both polarities. */
DiagResult ram_test_march_c_minus(void)
{
    const uint32_t ZERO = 0x00000000u, ONES = 0xFFFFFFFFu;
    volatile uint32_t *w = DUT;

    for (uint32_t i = 0; i < DUT_WORDS; i++)
        w[i] = ZERO;
    for (uint32_t i = 0; i < DUT_WORDS; i++) {
        uint32_t got = w[i];
        if (got != ZERO) return march_mismatch(i, ZERO, got);
        w[i] = ONES;
    }
    for (uint32_t i = 0; i < DUT_WORDS; i++) {
        uint32_t got = w[i];
        if (got != ONES) return march_mismatch(i, ONES, got);
        w[i] = ZERO;
    }
    for (uint32_t i = DUT_WORDS; i-- > 0; ) {
        uint32_t got = w[i];
        if (got != ZERO) return march_mismatch(i, ZERO, got);
        w[i] = ONES;
    }
    for (uint32_t i = DUT_WORDS; i-- > 0; ) {
        uint32_t got = w[i];
        if (got != ONES) return march_mismatch(i, ONES, got);
        w[i] = ZERO;
    }
    for (uint32_t i = DUT_WORDS; i-- > 0; ) {
        uint32_t got = w[i];
        if (got != ZERO) return march_mismatch(i, ZERO, got);
    }
    return diag_pass();
}
