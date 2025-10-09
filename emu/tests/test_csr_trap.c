/* CSRs, Zicsr instructions, trap delivery and mret.
 *
 * riscv-tests `p` environment (env/p/riscv_test.h, reset_vector) touches these CSRs:
 *   implemented here: mtvec, mstatus, mie, mepc, mcause (trap_vector), mhartid
 *   NOT implemented (must trap as illegal, then execution resumes at mtvec, which the code
 *   points at the next instruction before each probe): mnstatus, satp, pmpaddr0, pmpcfg0,
 *   medeleg, mideleg, and stvec/mideleg only when an stvec_handler exists.
 * test_probe below reproduces that pattern. */
#include <stdio.h>
#include <string.h>

#include "cpu.h"
#include "csr.h"
#include "machine.h"
#include "memory_map.h"
#include "test_util.h"

#define BASE MAIN_RAM_BASE
#define LEN(p) (sizeof(p) / sizeof((p)[0]))
#define X(n, want) CHECK_EQ_U32(m.cpu.x[n], want)

/* Words from riscv64-unknown-elf-as + ld (not from this emulator). Every program starts with
 * `la x5,handler; csrw mtvec,x5` (3 words), runs a body from word index 3, then stores
 * SYSCON_PASS. The shared handler leaves mcause/mepc/mtval/mstatus in x20..x23, counts traps
 * in x24 and resumes at mepc + 4. */
static const uint32_t prog_ecall[] = {
    0x00000297, /* auipc x5,0x0 */
    0x02428293, /* addi x5,x5,36 # 80000024 <handler> */
    0x30529073, /* csrrw x0,mtvec,x5 */
    0x00000073, /* ecall */
    0x00100313, /* addi x6,x0,1 */
    0x00100f37, /* lui x30,0x100 */
    0x00005fb7, /* lui x31,0x5 */
    0x555f8f93, /* addi x31,x31,1365 # 5555 <handler-0x7fffaacf> */
    0x01ff2023, /* sw x31,0(x30) # 100000 <handler-0x7ff00024> */
    0x34202a73, /* csrrs x20,mcause,x0 */
    0x34102af3, /* csrrs x21,mepc,x0 */
    0x34302b73, /* csrrs x22,mtval,x0 */
    0x30002bf3, /* csrrs x23,mstatus,x0 */
    0x001c0c13, /* addi x24,x24,1 */
    0x34102cf3, /* csrrs x25,mepc,x0 */
    0x004c8c93, /* addi x25,x25,4 */
    0x341c9073, /* csrrw x0,mepc,x25 */
    0x30200073, /* mret */
};
static const uint32_t prog_mie[] = {
    0x00000297, /* auipc x5,0x0 */
    0x02828293, /* addi x5,x5,40 # 80000028 <handler> */
    0x30529073, /* csrrw x0,mtvec,x5 */
    0x30046073, /* csrrsi x0,mstatus,8 */
    0x00000073, /* ecall */
    0x30002573, /* csrrs x10,mstatus,x0 */
    0x00100f37, /* lui x30,0x100 */
    0x00005fb7, /* lui x31,0x5 */
    0x555f8f93, /* addi x31,x31,1365 # 5555 <handler-0x7fffaad3> */
    0x01ff2023, /* sw x31,0(x30) # 100000 <handler-0x7ff00028> */
    0x34202a73, /* csrrs x20,mcause,x0 */
    0x34102af3, /* csrrs x21,mepc,x0 */
    0x34302b73, /* csrrs x22,mtval,x0 */
    0x30002bf3, /* csrrs x23,mstatus,x0 */
    0x001c0c13, /* addi x24,x24,1 */
    0x34102cf3, /* csrrs x25,mepc,x0 */
    0x004c8c93, /* addi x25,x25,4 */
    0x341c9073, /* csrrw x0,mepc,x25 */
    0x30200073, /* mret */
};
static const uint32_t prog_unknown_csr[] = {
    0x00000297, /* auipc x5,0x0 */
    0x02428293, /* addi x5,x5,36 # 80000024 <handler> */
    0x30529073, /* csrrw x0,mtvec,x5 */
    0x7c0020f3, /* csrrs x1,0x7c0,x0 */
    0x7c10d073, /* csrrwi x0,0x7c1,1 */
    0x00100f37, /* lui x30,0x100 */
    0x00005fb7, /* lui x31,0x5 */
    0x555f8f93, /* addi x31,x31,1365 # 5555 <handler-0x7fffaacf> */
    0x01ff2023, /* sw x31,0(x30) # 100000 <handler-0x7ff00024> */
    0x34202a73, /* csrrs x20,mcause,x0 */
    0x34102af3, /* csrrs x21,mepc,x0 */
    0x34302b73, /* csrrs x22,mtval,x0 */
    0x30002bf3, /* csrrs x23,mstatus,x0 */
    0x001c0c13, /* addi x24,x24,1 */
    0x34102cf3, /* csrrs x25,mepc,x0 */
    0x004c8c93, /* addi x25,x25,4 */
    0x341c9073, /* csrrw x0,mepc,x25 */
    0x30200073, /* mret */
};
static const uint32_t prog_jalr_misaligned[] = {
    0x00000297, /* auipc x5,0x0 */
    0x02828293, /* addi x5,x5,40 # 80000028 <handler> */
    0x30529073, /* csrrw x0,mtvec,x5 */
    0x80000537, /* lui x10,0x80000 */
    0x00650513, /* addi x10,x10,6 # 80000006 <handler-0x22> */
    0x000500e7, /* jalr x1,0(x10) */
    0x00100f37, /* lui x30,0x100 */
    0x00005fb7, /* lui x31,0x5 */
    0x555f8f93, /* addi x31,x31,1365 # 5555 <handler-0x7fffaad3> */
    0x01ff2023, /* sw x31,0(x30) # 100000 <handler-0x7ff00028> */
    0x34202a73, /* csrrs x20,mcause,x0 */
    0x34102af3, /* csrrs x21,mepc,x0 */
    0x34302b73, /* csrrs x22,mtval,x0 */
    0x30002bf3, /* csrrs x23,mstatus,x0 */
    0x001c0c13, /* addi x24,x24,1 */
    0x34102cf3, /* csrrs x25,mepc,x0 */
    0x004c8c93, /* addi x25,x25,4 */
    0x341c9073, /* csrrw x0,mepc,x25 */
    0x30200073, /* mret */
};
static const uint32_t prog_load_fault[] = {
    0x00000297, /* auipc x5,0x0 */
    0x02028293, /* addi x5,x5,32 # 80000020 <handler> */
    0x30529073, /* csrrw x0,mtvec,x5 */
    0x00002083, /* lw x1,0(x0) # 0 <handler-0x80000020> */
    0x00100f37, /* lui x30,0x100 */
    0x00005fb7, /* lui x31,0x5 */
    0x555f8f93, /* addi x31,x31,1365 # 5555 <handler-0x7fffaacb> */
    0x01ff2023, /* sw x31,0(x30) # 100000 <handler-0x7ff00020> */
    0x34202a73, /* csrrs x20,mcause,x0 */
    0x34102af3, /* csrrs x21,mepc,x0 */
    0x34302b73, /* csrrs x22,mtval,x0 */
    0x30002bf3, /* csrrs x23,mstatus,x0 */
    0x001c0c13, /* addi x24,x24,1 */
    0x34102cf3, /* csrrs x25,mepc,x0 */
    0x004c8c93, /* addi x25,x25,4 */
    0x341c9073, /* csrrw x0,mepc,x25 */
    0x30200073, /* mret */
};
static const uint32_t prog_store_fault[] = {
    0x00000297, /* auipc x5,0x0 */
    0x02028293, /* addi x5,x5,32 # 80000020 <handler> */
    0x30529073, /* csrrw x0,mtvec,x5 */
    0x00002423, /* sw x0,8(x0) # 8 <handler-0x80000018> */
    0x00100f37, /* lui x30,0x100 */
    0x00005fb7, /* lui x31,0x5 */
    0x555f8f93, /* addi x31,x31,1365 # 5555 <handler-0x7fffaacb> */
    0x01ff2023, /* sw x31,0(x30) # 100000 <handler-0x7ff00020> */
    0x34202a73, /* csrrs x20,mcause,x0 */
    0x34102af3, /* csrrs x21,mepc,x0 */
    0x34302b73, /* csrrs x22,mtval,x0 */
    0x30002bf3, /* csrrs x23,mstatus,x0 */
    0x001c0c13, /* addi x24,x24,1 */
    0x34102cf3, /* csrrs x25,mepc,x0 */
    0x004c8c93, /* addi x25,x25,4 */
    0x341c9073, /* csrrw x0,mepc,x25 */
    0x30200073, /* mret */
};
static const uint32_t prog_ebreak[] = {
    0x00000297, /* auipc x5,0x0 */
    0x02028293, /* addi x5,x5,32 # 80000020 <handler> */
    0x30529073, /* csrrw x0,mtvec,x5 */
    0x00100073, /* ebreak */
    0x00100f37, /* lui x30,0x100 */
    0x00005fb7, /* lui x31,0x5 */
    0x555f8f93, /* addi x31,x31,1365 # 5555 <handler-0x7fffaacb> */
    0x01ff2023, /* sw x31,0(x30) # 100000 <handler-0x7ff00020> */
    0x34202a73, /* csrrs x20,mcause,x0 */
    0x34102af3, /* csrrs x21,mepc,x0 */
    0x34302b73, /* csrrs x22,mtval,x0 */
    0x30002bf3, /* csrrs x23,mstatus,x0 */
    0x001c0c13, /* addi x24,x24,1 */
    0x34102cf3, /* csrrs x25,mepc,x0 */
    0x004c8c93, /* addi x25,x25,4 */
    0x341c9073, /* csrrw x0,mepc,x25 */
    0x30200073, /* mret */
};
static const uint32_t prog_csr_ops[] = {
    0x00000297, /* auipc x5,0x0 */
    0x06428293, /* addi x5,x5,100 # 80000064 <handler> */
    0x30529073, /* csrrw x0,mtvec,x5 */
    0x0000f0b7, /* lui x1,0xf */
    0x0f008093, /* addi x1,x1,240 # f0f0 <handler-0x7fff0f74> */
    0x34009073, /* csrrw x0,mscratch,x1 */
    0x34002173, /* csrrs x2,mscratch,x0 */
    0x3407e1f3, /* csrrsi x3,mscratch,15 */
    0x3401f273, /* csrrci x4,mscratch,3 */
    0x340012f3, /* csrrw x5,mscratch,x0 */
    0x34002373, /* csrrs x6,mscratch,x0 */
    0x3400b3f3, /* csrrc x7,mscratch,x1 */
    0x340fd073, /* csrrwi x0,mscratch,31 */
    0x34002473, /* csrrs x8,mscratch,x0 */
    0x301024f3, /* csrrs x9,misa,x0 */
    0xf1402573, /* csrrs x10,mhartid,x0 */
    0xf1409073, /* csrrw x0,mhartid,x1 */
    0xf14025f3, /* csrrs x11,mhartid,x0 */
    0x30002673, /* csrrs x12,mstatus,x0 */
    0x34109073, /* csrrw x0,mepc,x1 */
    0x341026f3, /* csrrs x13,mepc,x0 */
    0x00100f37, /* lui x30,0x100 */
    0x00005fb7, /* lui x31,0x5 */
    0x555f8f93, /* addi x31,x31,1365 # 5555 <handler-0x7fffab0f> */
    0x01ff2023, /* sw x31,0(x30) # 100000 <handler-0x7ff00064> */
    0x34202a73, /* csrrs x20,mcause,x0 */
    0x34102af3, /* csrrs x21,mepc,x0 */
    0x34302b73, /* csrrs x22,mtval,x0 */
    0x30002bf3, /* csrrs x23,mstatus,x0 */
    0x001c0c13, /* addi x24,x24,1 */
    0x34102cf3, /* csrrs x25,mepc,x0 */
    0x004c8c93, /* addi x25,x25,4 */
    0x341c9073, /* csrrw x0,mepc,x25 */
    0x30200073, /* mret */
};
static const uint32_t prog_probe[] = {
    0x00000297, /* auipc x5,0x0 */
    0x01028293, /* addi x5,x5,16 # 80000010 <__BSS_END__-0x101c> */
    0x30529073, /* csrrw x0,mtvec,x5 */
    0x18005073, /* csrrwi x0,satp,0 */
    0x34202a73, /* csrrs x20,mcause,x0 */
    0x34102af3, /* csrrs x21,mepc,x0 */
    0x00500313, /* addi x6,x0,5 */
    0x00100f37, /* lui x30,0x100 */
    0x00005fb7, /* lui x31,0x5 */
    0x555f8f93, /* addi x31,x31,1365 # 5555 <__BSS_END__-0x7fffbad7> */
    0x01ff2023, /* sw x31,0(x30) # 100000 <__BSS_END__-0x7ff0102c> */
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
    return machine_run(&m, 1000);
}

#define RUN_PASSES(p) CHECK(run(p, LEN(p)) == RUN_PASS)

static void test_csr_masks(void)
{
    Csr c = { 0 };
    uint32_t v;

    CHECK(csr_write(&c, CSR_MEPC, 0xFFFFFFFFu));    CHECK(csr_read(&c, CSR_MEPC, 0, 0, &v));    CHECK_EQ_U32(v, 0xFFFFFFFCu);
    CHECK(csr_write(&c, CSR_MTVEC, 0x80000103u));   CHECK(csr_read(&c, CSR_MTVEC, 0, 0, &v));   CHECK_EQ_U32(v, 0x80000100u);
    CHECK(csr_write(&c, CSR_MSTATUS, 0xFFFFFFFFu)); CHECK(csr_read(&c, CSR_MSTATUS, 0, 0, &v)); CHECK_EQ_U32(v, 0x1888u);
    CHECK(csr_write(&c, CSR_MSTATUS, 0));           CHECK(csr_read(&c, CSR_MSTATUS, 0, 0, &v)); CHECK_EQ_U32(v, 0x1800u);   /* MPP reads 3 */
    CHECK(csr_write(&c, CSR_MIE, 0xFFFFFFFFu));     CHECK(csr_read(&c, CSR_MIE, 0, 0, &v));     CHECK_EQ_U32(v, 0x880u);
    CHECK(csr_write(&c, CSR_MSCRATCH, 0xDEADBEEFu));CHECK(csr_read(&c, CSR_MSCRATCH, 0, 0, &v));CHECK_EQ_U32(v, 0xDEADBEEFu);
    CHECK(csr_write(&c, CSR_MCAUSE, 0x80000007u));  CHECK(csr_read(&c, CSR_MCAUSE, 0, 0, &v));  CHECK_EQ_U32(v, 0x80000007u);
    CHECK(csr_write(&c, CSR_MTVAL, 0x12345678u));   CHECK(csr_read(&c, CSR_MTVAL, 0, 0, &v));   CHECK_EQ_U32(v, 0x12345678u);

    CHECK(csr_write(&c, CSR_MISA, 0));              CHECK(csr_read(&c, CSR_MISA, 0, 0, &v));    CHECK_EQ_U32(v, 0x40001100u);
    CHECK(csr_write(&c, CSR_MHARTID, 5));           CHECK(csr_read(&c, CSR_MHARTID, 0, 0, &v)); CHECK_EQ_U32(v, 0);
    CHECK(csr_write(&c, CSR_MIP, 0xFFFFFFFFu));
    CHECK(csr_read(&c, CSR_MIP, 0, 0, &v));         CHECK_EQ_U32(v, 0);                          /* mip is composed, not stored */
    CHECK(csr_read(&c, CSR_MIP, 1, 0, &v));         CHECK_EQ_U32(v, 0x80);
    CHECK(csr_read(&c, CSR_MIP, 0, 1, &v));         CHECK_EQ_U32(v, 0x800);
    CHECK(csr_read(&c, CSR_MIP, 1, 1, &v));         CHECK_EQ_U32(v, 0x880);

    CHECK(!csr_read(&c, 0x7C0, 0, 0, &v));
    CHECK(!csr_write(&c, 0x7C0, 0));
    CHECK(!csr_read(&c, 0x180, 0, 0, &v));                                                       /* satp */
}

static void test_csr_instructions(void)
{
    RUN_PASSES(prog_csr_ops);
    X(2, 0xF0F0u);  X(3, 0xF0F0u);  X(4, 0xF0FFu);  X(5, 0xF0FCu);   /* each returns the OLD value */
    X(6, 0);        X(7, 0);        X(8, 31);
    X(9, 0x40001100u);
    X(10, 0);  X(11, 0);             /* mhartid: write ignored, no trap */
    X(12, 0x1800u);
    X(13, 0xF0F0u);
    X(24, 0);                        /* nothing trapped */
}

static void test_ecall(void)
{
    RUN_PASSES(prog_ecall);
    X(20, 11);  X(21, BASE + 12);  X(22, 0);
    X(23, 0x1800u);                  /* MIE was 0, so MPIE stays 0 */
    X(24, 1);
    X(6, 1);                         /* resumed after the ecall */
}

static void test_mie_mpie_save_restore(void)
{
    RUN_PASSES(prog_mie);
    X(21, BASE + 16);
    X(23, 0x1880u);                  /* in the handler: MIE cleared, MPIE holds the old MIE */
    X(10, 0x1888u);                  /* after mret: MIE restored from MPIE, MPIE set */
}

static void test_unknown_csr_traps(void)
{
    RUN_PASSES(prog_unknown_csr);
    X(24, 2);                        /* both the read and the write trapped */
    X(20, 2);
    X(21, BASE + 16);                /* second trap: word 4 */
    X(22, prog_unknown_csr[4]);      /* mtval = the instruction word */
    X(1, 0);                         /* destination not written by the trapping csrr */
}

static void test_fetch_misaligned_jalr(void)
{
    RUN_PASSES(prog_jalr_misaligned);
    X(24, 1);
    X(20, 0);  X(21, BASE + 20);  X(22, BASE + 6);
    X(1, 0);                         /* link not written */
}

static void test_access_faults(void)
{
    RUN_PASSES(prog_load_fault);
    X(24, 1);  X(20, 5);  X(21, BASE + 12);  X(22, 0);
    RUN_PASSES(prog_store_fault);
    X(24, 1);  X(20, 7);  X(21, BASE + 12);  X(22, 8);
}

static void test_ebreak(void)
{
    RUN_PASSES(prog_ebreak);
    X(24, 1);  X(20, 3);  X(21, BASE + 12);  X(22, 0);
}

static void test_probe(void)
{
    RUN_PASSES(prog_probe);
    X(20, 2);                        /* the probe trapped as illegal-instruction... */
    X(21, BASE + 12);                /* ...at the csrwi satp... */
    X(6, 5);                         /* ...and execution continued at the next instruction */
}

static void test_trap_without_handler(void)
{
    static const uint32_t ecall_only[] = { 0x00000073u };
    CHECK(run(ecall_only, 1) == RUN_ERROR);
    CHECK(strstr(m.error, "no handler") != NULL);
}

int main(void)
{
    test_csr_masks();
    test_csr_instructions();
    test_ecall();
    test_mie_mpie_save_restore();
    test_unknown_csr_traps();
    test_fetch_misaligned_jalr();
    test_access_faults();
    test_ebreak();
    test_probe();
    test_trap_without_handler();
    TEST_MAIN_RETURN();
}
