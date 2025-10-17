/* CLINT registers, mip/mie/MIE gating, and machine-timer interrupt delivery. */
#include <stdio.h>
#include <string.h>

#include "clint.h"
#include "cpu.h"
#include "csr.h"
#include "machine.h"
#include "memory_map.h"
#include "test_util.h"

#define BASE MAIN_RAM_BASE
#define LEN(p) (sizeof(p) / sizeof((p)[0]))
#define X(n, want) CHECK_EQ_U32(m.cpu.x[n], want)

/* --- CLINT device, through the bus --- */
static void test_clint_registers(void)
{
    Machine m;
    uint32_t v;
    CHECK(machine_init(&m));
    Bus *b = &m.bus;

    CHECK(bus_read(b, CLINT_BASE + CLINT_MTIME_LO, 4, &v));     CHECK_EQ_U32(v, 0);
    CHECK(bus_read(b, CLINT_BASE + CLINT_MTIMECMP_LO, 4, &v));  CHECK_EQ_U32(v, 0xFFFFFFFFu);   /* reset: never fires */
    CHECK(bus_read(b, CLINT_BASE + CLINT_MTIMECMP_HI, 4, &v));  CHECK_EQ_U32(v, 0xFFFFFFFFu);
    CHECK(!clint_timer_pending(&m.clint));

    CHECK(bus_write(b, CLINT_BASE + CLINT_MTIMECMP_LO, 4, 0x11111111u));
    CHECK(bus_write(b, CLINT_BASE + CLINT_MTIMECMP_HI, 4, 0x22222222u));
    CHECK(m.clint.mtimecmp == 0x2222222211111111ull);
    CHECK(bus_read(b, CLINT_BASE + CLINT_MTIMECMP_HI, 4, &v));  CHECK_EQ_U32(v, 0x22222222u);

    CHECK(bus_write(b, CLINT_BASE + CLINT_MTIME_LO, 4, 0xFFFFFFFFu));
    CHECK(bus_write(b, CLINT_BASE + CLINT_MTIME_HI, 4, 0x1));
    CHECK(m.clint.mtime == 0x1FFFFFFFFull);
    bus_tick(b);                                                   /* carries into the high word */
    CHECK(m.clint.mtime == 0x200000000ull);
    CHECK(bus_read(b, CLINT_BASE + CLINT_MTIME_HI, 4, &v));     CHECK_EQ_U32(v, 2);
    CHECK(bus_read(b, CLINT_BASE + CLINT_MTIME_LO, 4, &v));     CHECK_EQ_U32(v, 0);

    /* only the four registers exist, only as 32-bit accesses */
    CHECK(!bus_read(b, CLINT_BASE + 0x0, 4, &v));
    CHECK(!bus_write(b, CLINT_BASE + 0x4008, 4, 0));
    CHECK(!bus_read(b, CLINT_BASE + CLINT_MTIME_LO, 2, &v));
    CHECK(!bus_read(b, CLINT_BASE + CLINT_MTIME_LO, 1, &v));
    CHECK(!bus_write(b, CLINT_BASE + CLINT_MTIMECMP_LO, 1, 0));
    CHECK(!bus_read(b, CLINT_BASE + CLINT_MTIME_LO + 2, 4, &v));   /* misaligned */
    machine_free(&m);
}

static void test_timer_pending_compare(void)
{
    Clint c;
    clint_init(&c);
    c.mtimecmp = 10;
    c.mtime = 9;   CHECK(!clint_timer_pending(&c));
    c.mtime = 10;  CHECK(clint_timer_pending(&c));                 /* >= : equality fires */
    c.mtime = 11;  CHECK(clint_timer_pending(&c));
    c.mtimecmp = 0x100000000ull;                                   /* 64-bit compare, not 32 */
    c.mtime = 0xFFFFFFFFu;  CHECK(!clint_timer_pending(&c));
    c.mtime = 0x100000000ull;  CHECK(clint_timer_pending(&c));
}

/* --- interrupt selection --- */
static void test_pending_interrupt(void)
{
    Csr c = { 0 };
    c.mstatus = MSTATUS_MIE;
    c.mie = MIP_MTIP | MIP_MEIP;
    CHECK_EQ_U32(csr_pending_interrupt(&c, true, false), CAUSE_INT_MTIMER);
    CHECK_EQ_U32(csr_pending_interrupt(&c, false, true), CAUSE_INT_MEXTERNAL);
    CHECK_EQ_U32(csr_pending_interrupt(&c, true, true), CAUSE_INT_MEXTERNAL);   /* priority: external first */
    CHECK_EQ_U32(csr_pending_interrupt(&c, false, false), 0);
    CHECK_EQ_U32(CAUSE_INT_MTIMER, 0x80000007u);
    CHECK_EQ_U32(CAUSE_INT_MEXTERNAL, 0x8000000Bu);

    c.mie = MIP_MTIP;                                              /* external masked in mie */
    CHECK_EQ_U32(csr_pending_interrupt(&c, true, true), CAUSE_INT_MTIMER);
    CHECK_EQ_U32(csr_pending_interrupt(&c, false, true), 0);
    c.mie = MIP_MEIP;
    CHECK_EQ_U32(csr_pending_interrupt(&c, true, false), 0);
    c.mie = 0;
    CHECK_EQ_U32(csr_pending_interrupt(&c, true, true), 0);

    c.mie = MIP_MTIP | MIP_MEIP;
    c.mstatus = 0;                                                 /* global enable off */
    CHECK_EQ_U32(csr_pending_interrupt(&c, true, true), 0);
}

/* --- programs (assembled + linked with riscv64-unknown-elf-as/ld) ---
 * Common layout: arm mtimecmp = 20, enable, spin; the handler (a) counts entries in x24,
 * (b) records mcause/mepc/mstatus in x20..x22 and mtime in x23, (c) keeps the first mepc/mtime
 * in x18/x19, (d) busy-waits 30 loops with the timer still pending, (e) re-arms 40 ticks later
 * on the first entry and disarms on the second, then mret. */
/* timer: spin loop at word index 11 */
static const uint32_t prog_timer[] = {
    0x00000297, /* auipc x5,0x0 */
    0x04828293, /* addi x5,x5,72 # 80000048 <handler> */
    0x30529073, /* csrrw x0,mtvec,x5 */
    0x01400393, /* addi x7,x0,20 */
    0x02004437, /* lui x8,0x2004 */
    0x00742023, /* sw x7,0(x8) # 2004000 <spin-0x7dffc02c> */
    0x00042223, /* sw x0,4(x8) */
    0x00200813, /* addi x16,x0,2 */
    0x08000493, /* addi x9,x0,128 */
    0x30449073, /* csrrw x0,mie,x9 */
    0x30046073, /* csrrsi x0,mstatus,8 */
    0x00150513, /* addi x10,x10,1 */
    0xff0c4ee3, /* blt x24,x16,8000002c <spin> */
    0x30002d73, /* csrrs x26,mstatus,x0 */
    0x00100f37, /* lui x30,0x100 */
    0x00005fb7, /* lui x31,0x5 */
    0x555f8f93, /* addi x31,x31,1365 # 5555 <spin-0x7fffaad7> */
    0x01ff2023, /* sw x31,0(x30) # 100000 <spin-0x7ff0002c> */
    0x001c0c13, /* addi x24,x24,1 */
    0x34202a73, /* csrrs x20,mcause,x0 */
    0x34102af3, /* csrrs x21,mepc,x0 */
    0x30002b73, /* csrrs x22,mstatus,x0 */
    0x0200c5b7, /* lui x11,0x200c */
    0xff85ab83, /* lw x23,-8(x11) # 200bff8 <spin-0x7dff4034> */
    0x00091663, /* bne x18,x0,8000006c <handler+0x24> */
    0x000a8913, /* addi x18,x21,0 */
    0x000b8993, /* addi x19,x23,0 */
    0x01e00793, /* addi x15,x0,30 */
    0xfff78793, /* addi x15,x15,-1 */
    0xfe079ee3, /* bne x15,x0,80000070 <handler+0x28> */
    0x02004637, /* lui x12,0x2004 */
    0x00200693, /* addi x13,x0,2 */
    0x00dc5a63, /* bge x24,x13,80000094 <handler+0x4c> */
    0xff85a703, /* lw x14,-8(x11) */
    0x02870713, /* addi x14,x14,40 */
    0x00e62023, /* sw x14,0(x12) # 2004000 <spin-0x7dffc02c> */
    0x30200073, /* mret */
    0xfff00693, /* addi x13,x0,-1 */
    0x00d62223, /* sw x13,4(x12) */
    0x30200073, /* mret */
};
/* mie_clear: spin loop at word index 11 */
static const uint32_t prog_mie_clear[] = {
    0x06400893, /* addi x17,x0,100 */
    0x00000297, /* auipc x5,0x0 */
    0x04828293, /* addi x5,x5,72 # 8000004c <handler> */
    0x30529073, /* csrrw x0,mtvec,x5 */
    0x01400393, /* addi x7,x0,20 */
    0x02004437, /* lui x8,0x2004 */
    0x00742023, /* sw x7,0(x8) # 2004000 <spin-0x7dffc02c> */
    0x00042223, /* sw x0,4(x8) */
    0x00200813, /* addi x16,x0,2 */
    0x08000493, /* addi x9,x0,128 */
    0x30449073, /* csrrw x0,mie,x9 */
    0x00150513, /* addi x10,x10,1 */
    0xff154ee3, /* blt x10,x17,8000002c <spin> */
    0x0200c5b7, /* lui x11,0x200c */
    0xff85ab83, /* lw x23,-8(x11) # 200bff8 <spin-0x7dff4034> */
    0x00100f37, /* lui x30,0x100 */
    0x00005fb7, /* lui x31,0x5 */
    0x555f8f93, /* addi x31,x31,1365 # 5555 <spin-0x7fffaad7> */
    0x01ff2023, /* sw x31,0(x30) # 100000 <spin-0x7ff0002c> */
    0x001c0c13, /* addi x24,x24,1 */
    0x34202a73, /* csrrs x20,mcause,x0 */
    0x34102af3, /* csrrs x21,mepc,x0 */
    0x30002b73, /* csrrs x22,mstatus,x0 */
    0x0200c5b7, /* lui x11,0x200c */
    0xff85ab83, /* lw x23,-8(x11) # 200bff8 <spin-0x7dff4034> */
    0x00091663, /* bne x18,x0,80000070 <handler+0x24> */
    0x000a8913, /* addi x18,x21,0 */
    0x000b8993, /* addi x19,x23,0 */
    0x01e00793, /* addi x15,x0,30 */
    0xfff78793, /* addi x15,x15,-1 */
    0xfe079ee3, /* bne x15,x0,80000074 <handler+0x28> */
    0x02004637, /* lui x12,0x2004 */
    0x00200693, /* addi x13,x0,2 */
    0x00dc5a63, /* bge x24,x13,80000098 <handler+0x4c> */
    0xff85a703, /* lw x14,-8(x11) */
    0x02870713, /* addi x14,x14,40 */
    0x00e62023, /* sw x14,0(x12) # 2004000 <spin-0x7dffc02c> */
    0x30200073, /* mret */
    0xfff00693, /* addi x13,x0,-1 */
    0x00d62223, /* sw x13,4(x12) */
    0x30200073, /* mret */
};
/* mtie_clear: spin loop at word index 10 */
static const uint32_t prog_mtie_clear[] = {
    0x06400893, /* addi x17,x0,100 */
    0x00000297, /* auipc x5,0x0 */
    0x04428293, /* addi x5,x5,68 # 80000048 <handler> */
    0x30529073, /* csrrw x0,mtvec,x5 */
    0x01400393, /* addi x7,x0,20 */
    0x02004437, /* lui x8,0x2004 */
    0x00742023, /* sw x7,0(x8) # 2004000 <spin-0x7dffc028> */
    0x00042223, /* sw x0,4(x8) */
    0x00200813, /* addi x16,x0,2 */
    0x30046073, /* csrrsi x0,mstatus,8 */
    0x00150513, /* addi x10,x10,1 */
    0xff154ee3, /* blt x10,x17,80000028 <spin> */
    0x0200c5b7, /* lui x11,0x200c */
    0xff85ab83, /* lw x23,-8(x11) # 200bff8 <spin-0x7dff4030> */
    0x00100f37, /* lui x30,0x100 */
    0x00005fb7, /* lui x31,0x5 */
    0x555f8f93, /* addi x31,x31,1365 # 5555 <spin-0x7fffaad3> */
    0x01ff2023, /* sw x31,0(x30) # 100000 <spin-0x7ff00028> */
    0x001c0c13, /* addi x24,x24,1 */
    0x34202a73, /* csrrs x20,mcause,x0 */
    0x34102af3, /* csrrs x21,mepc,x0 */
    0x30002b73, /* csrrs x22,mstatus,x0 */
    0x0200c5b7, /* lui x11,0x200c */
    0xff85ab83, /* lw x23,-8(x11) # 200bff8 <spin-0x7dff4030> */
    0x00091663, /* bne x18,x0,8000006c <handler+0x24> */
    0x000a8913, /* addi x18,x21,0 */
    0x000b8993, /* addi x19,x23,0 */
    0x01e00793, /* addi x15,x0,30 */
    0xfff78793, /* addi x15,x15,-1 */
    0xfe079ee3, /* bne x15,x0,80000070 <handler+0x28> */
    0x02004637, /* lui x12,0x2004 */
    0x00200693, /* addi x13,x0,2 */
    0x00dc5a63, /* bge x24,x13,80000094 <handler+0x4c> */
    0xff85a703, /* lw x14,-8(x11) */
    0x02870713, /* addi x14,x14,40 */
    0x00e62023, /* sw x14,0(x12) # 2004000 <spin-0x7dffc028> */
    0x30200073, /* mret */
    0xfff00693, /* addi x13,x0,-1 */
    0x00d62223, /* sw x13,4(x12) */
    0x30200073, /* mret */
};

static Machine m;
static bool have_machine;

static RunResult run(const uint32_t *prog, size_t n)
{
    if (have_machine)
        machine_free(&m);
    CHECK(machine_init(&m));
    have_machine = true;
    for (size_t i = 0; i < n; i++)
        CHECK(bus_write(&m.bus, BASE + 4 * (uint32_t)i, 4, prog[i]));
    m.cpu.pc = BASE;
    return machine_run(&m, 5000);
}

#define SPIN_IDX 11     /* word index of `spin` in prog_timer (11 setup instructions before it) */

static void test_timer_interrupt(void)
{
    CHECK(run(prog_timer, LEN(prog_timer)) == RUN_PASS);
    X(24, 2);                              /* taken twice: once, then again after being re-armed */
    X(20, 0x80000007u);                    /* interrupt bit + machine timer */
    X(22, 0x1880u);                        /* in the handler MIE=0, MPIE=1 */
    X(26, 0x1888u);                        /* after mret MIE is back */

    /* mtimecmp = 20 and mtime counts one per iteration: iteration 20 (0-based) sees mtime == 20
     * and takes the interrupt instead of executing, so exactly 20 instructions ran: 11 setup +
     * 9 of the spin loop (addi, blt, ... ending on an addi). The one NOT executed is the blt. */
    X(18, BASE + 4 * (SPIN_IDX + 1));
    /* mtime read by the 6th handler instruction: 21 after the interrupt iteration, +5 */
    X(19, 26);
    /* handler stayed uninterrupted while the timer was still pending (masked), and the
     * second interrupt only came after the re-arm 40 ticks after the first handler's wait */
    CHECK(m.cpu.x[23] >= m.cpu.x[19] + 100);
}

static void test_no_interrupt_when_masked(void)
{
    CHECK(run(prog_mie_clear, LEN(prog_mie_clear)) == RUN_PASS);   /* MTIE set, MIE clear */
    X(24, 0);  X(20, 0);  X(10, 100);
    CHECK(m.cpu.x[23] >= 100);             /* mtime well past the compare value 20 */
    CHECK(clint_timer_pending(&m.clint));  /* the line is up, the CPU just ignored it */

    CHECK(run(prog_mtie_clear, LEN(prog_mtie_clear)) == RUN_PASS); /* MIE set, MTIE clear */
    X(24, 0);  X(20, 0);  X(10, 100);
    CHECK(clint_timer_pending(&m.clint));
}

int main(void)
{
    test_clint_registers();
    test_timer_pending_compare();
    test_pending_interrupt();
    test_timer_interrupt();
    test_no_interrupt_when_masked();
    TEST_MAIN_RETURN();
}
