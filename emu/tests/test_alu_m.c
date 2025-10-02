#include <stdio.h>

#include "alu.h"
#include "cpu.h"
#include "machine.h"
#include "memory_map.h"
#include "test_util.h"

#define MIN 0x80000000u     /* INT_MIN as a register bit pattern */
#define NEG1 0xFFFFFFFFu    /* -1, also UINT_MAX */
#define NEG(n) (0u - (n))

enum { MUL, MULH, MULHSU, MULHU, DIV, DIVU, REM, REMU };

#define M(op, a, b, want) CHECK_EQ_U32(alu_m(op, a, b), want)

static void test_mul(void)
{
    M(MUL, 3, 4, 12);
    M(MUL, NEG(3), 4, NEG(12));
    M(MUL, MIN, 2, 0);                 /* overflow keeps the low 32 bits */
    M(MUL, NEG1, NEG1, 1);
    M(MUL, 0, NEG1, 0);
}

static void test_mulh(void)
{
    M(MULH, 3, 4, 0);
    M(MULH, NEG1, NEG1, 0);            /* (-1) * (-1) = 1 */
    M(MULH, MIN, MIN, 0x40000000u);    /* 2^62 */
    M(MULH, NEG1, 1, NEG1);            /* -1 */
    M(MULH, 0x7FFFFFFFu, 0x7FFFFFFFu, 0x3FFFFFFFu);
    M(MULH, MIN, NEG1, 0);             /* 2^31 */
    M(MULH, 0, MIN, 0);
}

static void test_mulhsu(void)
{
    M(MULHSU, NEG1, NEG1, NEG1);       /* -1 * UINT_MAX = -(2^32 - 1) */
    M(MULHSU, MIN, NEG1, MIN);
    M(MULHSU, 1, NEG1, 0);
    M(MULHSU, NEG1, 1, NEG1);
    M(MULHSU, NEG1, 0, 0);
}

static void test_mulhu(void)
{
    M(MULHU, NEG1, NEG1, 0xFFFFFFFEu);
    M(MULHU, MIN, 2, 1);               /* 2^32 */
    M(MULHU, MIN, MIN, 0x40000000u);
    M(MULHU, 3, 4, 0);
}

static void test_div(void)
{
    M(DIV, 20, 3, 6);
    M(DIV, NEG(20), 3, NEG(6));        /* rounds toward zero */
    M(DIV, 20, NEG(3), NEG(6));
    M(DIV, NEG(20), NEG(3), 6);
    M(DIV, 0, 5, 0);
    M(DIV, MIN, 2, 0xC0000000u);
    M(DIV, NEG1, MIN, 0);
}

static void test_div_by_zero(void)
{
    M(DIV, 5, 0, NEG1);
    M(DIV, NEG(5), 0, NEG1);
    M(DIV, MIN, 0, NEG1);
    M(DIV, 0, 0, NEG1);
    M(DIVU, 5, 0, NEG1);
    M(DIVU, NEG1, 0, NEG1);
    M(REM, 5, 0, 5);
    M(REM, NEG(5), 0, NEG(5));
    M(REM, MIN, 0, MIN);
    M(REMU, 5, 0, 5);
    M(REMU, NEG1, 0, NEG1);
}

static void test_int_min_over_minus_one(void)
{
    M(DIV, MIN, NEG1, MIN);            /* overflow: quotient is INT_MIN, no trap */
    M(REM, MIN, NEG1, 0);
    M(DIV, MIN, 1, MIN);
    M(REM, MIN, 1, 0);
}

static void test_divu_remu(void)
{
    M(DIVU, 20, 3, 6);
    M(DIVU, NEG1, 2, 0x7FFFFFFFu);
    M(DIVU, MIN, MIN, 1);
    M(DIVU, 5, NEG1, 0);
    M(REMU, 20, 3, 2);
    M(REMU, NEG1, 16, 15);
    M(REMU, 5, NEG1, 5);
    M(REMU, MIN, 3, 2);
}

static void test_rem(void)
{
    M(REM, 20, 3, 2);
    M(REM, NEG(20), 3, NEG(2));        /* sign follows the dividend */
    M(REM, 20, NEG(3), 2);
    M(REM, NEG(20), NEG(3), NEG(2));
    M(REM, 7, MIN, 7);
    M(REM, MIN, 7, NEG(2));
    M(REM, 0, 5, 0);
}

/* Words from riscv64-unknown-elf-as: a1 = -6, a2 = 4, then every M op once. */
static const uint32_t prog[] = {
    0xffa00093, /* addi x1,x0,-6 */
    0x00400113, /* addi x2,x0,4 */
    0x022081b3, /* mul x3,x1,x2 */
    0x02209233, /* mulh x4,x1,x2 */
    0x0220a2b3, /* mulhsu x5,x1,x2 */
    0x0220b333, /* mulhu x6,x1,x2 */
    0x0220c3b3, /* div x7,x1,x2 */
    0x0220d433, /* divu x8,x1,x2 */
    0x0220e4b3, /* rem x9,x1,x2 */
    0x0220f533, /* remu x10,x1,x2 */
};

static void test_through_cpu_step(void)
{
    Machine m;
    StepInfo info;
    CHECK(machine_init(&m));
    for (uint32_t i = 0; i < sizeof prog / sizeof prog[0]; i++)
        CHECK(bus_write(&m.bus, MAIN_RAM_BASE + 4 * i, 4, prog[i]));
    m.cpu.pc = MAIN_RAM_BASE;
    for (uint32_t i = 0; i < sizeof prog / sizeof prog[0]; i++)
        CHECK(!cpu_step(&m.cpu, &m.bus, &info).raised);
    CHECK_EQ_U32(m.cpu.x[3], NEG(24));      /* mul    */
    CHECK_EQ_U32(m.cpu.x[4], NEG1);         /* mulh   */
    CHECK_EQ_U32(m.cpu.x[5], NEG1);         /* mulhsu */
    CHECK_EQ_U32(m.cpu.x[6], 3);            /* mulhu: 0xFFFFFFFA * 4 = 0x3_FFFFFFE8 */
    CHECK_EQ_U32(m.cpu.x[7], NEG1);         /* div: -6 / 4 = -1 */
    CHECK_EQ_U32(m.cpu.x[8], 0x3FFFFFFEu);  /* divu */
    CHECK_EQ_U32(m.cpu.x[9], NEG(2));       /* rem: -6 % 4 = -2 */
    CHECK_EQ_U32(m.cpu.x[10], 2);           /* remu */
    machine_free(&m);
}

int main(void)
{
    test_mul();
    test_mulh();
    test_mulhsu();
    test_mulhu();
    test_div();
    test_div_by_zero();
    test_int_min_over_minus_one();
    test_divu_remu();
    test_rem();
    test_through_cpu_step();
    TEST_MAIN_RETURN();
}
