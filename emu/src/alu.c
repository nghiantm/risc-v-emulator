#include "alu.h"

#define SIGN_BIT 0x80000000u

static uint32_t neg(uint32_t v) { return 0u - v; }              /* two's complement negate */
static int is_neg(uint32_t v) { return (v & SIGN_BIT) != 0; }
static uint32_t magnitude(uint32_t v) { return is_neg(v) ? neg(v) : v; }   /* INT_MIN stays 0x80000000, correct as unsigned */

static uint32_t mulhu(uint32_t a, uint32_t b)
{
    return (uint32_t)(((uint64_t)a * b) >> 32);
}

/* Signed high product from the unsigned one. Reading a as signed means a_s = a - 2^32 when its
 * sign bit is set, so a_s*b = a*b - 2^32*b; the high word therefore drops by b. Same for b. */
static uint32_t mulh(uint32_t a, uint32_t b)
{
    uint32_t hi = mulhu(a, b);
    if (is_neg(a))
        hi -= b;
    if (is_neg(b))
        hi -= a;
    return hi;
}

static uint32_t mulhsu(uint32_t a, uint32_t b)                  /* a signed, b unsigned */
{
    uint32_t hi = mulhu(a, b);
    if (is_neg(a))
        hi -= b;
    return hi;
}

/* Signed division on magnitudes, so no C signed division (and no INT_MIN / -1 overflow) happens.
 * Spec edge cases: x / 0 = all ones, x % 0 = x; INT_MIN / -1 = INT_MIN, INT_MIN % -1 = 0.
 * The latter falls out of the magnitudes: 0x80000000 / 1 = 0x80000000, same sign, no negate. */
static uint32_t div_s(uint32_t a, uint32_t b)
{
    if (b == 0)
        return 0xFFFFFFFFu;
    uint32_t q = magnitude(a) / magnitude(b);
    return is_neg(a) != is_neg(b) ? neg(q) : q;                 /* quotient is negative iff signs differ */
}

static uint32_t rem_s(uint32_t a, uint32_t b)
{
    if (b == 0)
        return a;
    uint32_t r = magnitude(a) % magnitude(b);
    return is_neg(a) ? neg(r) : r;                              /* remainder takes the dividend's sign */
}

uint32_t alu_m(uint32_t funct3, uint32_t a, uint32_t b)
{
    switch (funct3) {
    case 0:  return a * b;                                      /* low 32 bits are the same signed or unsigned */
    case 1:  return mulh(a, b);
    case 2:  return mulhsu(a, b);
    case 3:  return mulhu(a, b);
    case 4:  return div_s(a, b);
    case 5:  return b == 0 ? 0xFFFFFFFFu : a / b;
    case 6:  return rem_s(a, b);
    default: return b == 0 ? a : a % b;
    }
}
