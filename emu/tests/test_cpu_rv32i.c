#include <stdio.h>
#include <string.h>

#include "cpu.h"
#include "machine.h"
#include "memory_map.h"
#include "test_util.h"

#define BASE MAIN_RAM_BASE
#define LEN(p) (sizeof(p) / sizeof((p)[0]))

/* Instruction words below were produced by the GNU assembler (riscv64-unknown-elf-as,
 * -march=rv32im_zicsr), not by this emulator, so the tests do not share its bugs.
 * The assembly text is kept next to each word. */
static const uint32_t prog_alu[] = {
    0x800000b7, /* lui x1,0x80000 */
    0xfff08093, /* addi x1,x1,-1 # 7fffffff <.text+0x7fffffff> */
    0x00100113, /* addi x2,x0,1 */
    0x002081b3, /* add x3,x1,x2 */
    0x40200233, /* sub x4,x0,x2 */
    0x401102b3, /* sub x5,x2,x1 */
    0x80000337, /* lui x6,0x80000 */
    0x002323b3, /* slt x7,x6,x2 */
    0x00233433, /* sltu x8,x6,x2 */
    0x006134b3, /* sltu x9,x2,x6 */
    0x0060c533, /* xor x10,x1,x6 */
    0x0060e5b3, /* or x11,x1,x6 */
    0x0060f633, /* and x12,x1,x6 */
    0x01f00693, /* addi x13,x0,31 */
    0x00d11733, /* sll x14,x2,x13 */
    0x00d357b3, /* srl x15,x6,x13 */
    0x40d35833, /* sra x16,x6,x13 */
    0x02000893, /* addi x17,x0,32 */
    0x01111933, /* sll x18,x2,x17 */
    0x411359b3, /* sra x19,x6,x17 */
};
static const uint32_t prog_alu_imm[] = {
    0xfff00093, /* addi x1,x0,-1 */
    0x00108113, /* addi x2,x1,1 */
    0x80008193, /* addi x3,x1,-2048 */
    0x0000a213, /* slti x4,x1,0 */
    0xfff0b293, /* sltiu x5,x1,-1 */
    0xfff03313, /* sltiu x6,x0,-1 */
    0x5550c393, /* xori x7,x1,1365 */
    0x7ff06413, /* ori x8,x0,2047 */
    0xff00f493, /* andi x9,x1,-16 */
    0x80000537, /* lui x10,0x80000 */
    0x01f09593, /* slli x11,x1,0x1f */
    0x01f55613, /* srli x12,x10,0x1f */
    0x41f55693, /* srai x13,x10,0x1f */
    0x40055713, /* srai x14,x10,0x0 */
    0x0040d793, /* srli x15,x1,0x4 */
    0x4040d813, /* srai x16,x1,0x4 */
};
static const uint32_t prog_upper[] = {
    0x123450b7, /* lui x1,0x12345 */
    0xfffff137, /* lui x2,0xfffff */
    0x00001197, /* auipc x3,0x1 */
    0xfffff217, /* auipc x4,0xfffff */
    0x00001037, /* lui x0,0x1 */
    0x00500013, /* addi x0,x0,5 */
    0x000002b3, /* add x5,x0,x0 */
};
static const uint32_t prog_mem[] = {
    0x80001537, /* lui x10,0x80001 */
    0x8081f0b7, /* lui x1,0x8081f */
    0x2f308093, /* addi x1,x1,755 # 8081f2f3 <.text+0x8081f2f3> */
    0x00152023, /* sw x1,0(x10) # 80001000 <.text+0x80001000> */
    0x00050103, /* lb x2,0(x10) */
    0x00054183, /* lbu x3,0(x10) */
    0x00051203, /* lh x4,0(x10) */
    0x00055283, /* lhu x5,0(x10) */
    0x00052303, /* lw x6,0(x10) */
    0x00350383, /* lb x7,3(x10) */
    0x00255403, /* lhu x8,2(x10) */
    0x00251483, /* lh x9,2(x10) */
    0x00150423, /* sb x1,8(x10) */
    0x00852583, /* lw x11,8(x10) */
    0x00151623, /* sh x1,12(x10) */
    0x00c52603, /* lw x12,12(x10) */
    0x00152823, /* sw x1,16(x10) */
    0x01052683, /* lw x13,16(x10) */
    0x01050713, /* addi x14,x10,16 */
    0xff072783, /* lw x15,-16(x14) */
    0x00152aa3, /* sw x1,21(x10) */
    0x01552803, /* lw x16,21(x10) */
    0x01452883, /* lw x17,20(x10) */
    0x01551903, /* lh x18,21(x10) */
    0x00052003, /* lw x0,0(x10) */
};
static const uint32_t prog_branch[] = {
    0xfff00093, /* addi x1,x0,-1 */
    0x00100113, /* addi x2,x0,1 */
    0x00108463, /* beq x1,x1,10 <.text+0x10> */
    0x00150513, /* addi x10,x10,1 */
    0x00109463, /* bne x1,x1,18 <.text+0x18> */
    0x00158593, /* addi x11,x11,1 */
    0x0020c463, /* blt x1,x2,20 <.text+0x20> */
    0x00160613, /* addi x12,x12,1 */
    0x0020e463, /* bltu x1,x2,28 <.text+0x28> */
    0x00168693, /* addi x13,x13,1 */
    0x00115463, /* bge x2,x1,30 <.text+0x30> */
    0x00170713, /* addi x14,x14,1 */
    0x00117463, /* bgeu x2,x1,38 <.text+0x38> */
    0x00178793, /* addi x15,x15,1 */
    0x0010d463, /* bge x1,x1,40 <.text+0x40> */
    0x00180813, /* addi x16,x16,1 */
    0x0010c463, /* blt x1,x1,48 <.text+0x48> */
    0x00188893, /* addi x17,x17,1 */
};
static const uint32_t prog_jump[] = {
    0x008000ef, /* jal x1,8 <.text+0x8> */
    0x06300293, /* addi x5,x0,99 */
    0x80000537, /* lui x10,0x80000 */
    0x01d50513, /* addi x10,x10,29 # 8000001d <.text+0x8000001d> */
    0x00050167, /* jalr x2,0(x10) */
    0x06300313, /* addi x6,x0,99 */
    0x06300313, /* addi x6,x0,99 */
    0x00700393, /* addi x7,x0,7 */
};
static const uint32_t prog_jal_misaligned[] = {
    0x006000ef, /* jal x1,6 <.text+0x6> */
};
static const uint32_t prog_beq_misaligned[] = {
    0x00000363, /* beq x0,x0,6 <.text+0x6> */
};
static const uint32_t prog_bne_not_taken[] = {
    0x00001363, /* bne x0,x0,6 <.text+0x6> */
};
static const uint32_t prog_jalr_misaligned[] = {
    0x80000537, /* lui x10,0x80000 */
    0x00650513, /* addi x10,x10,6 # 80000006 <.text+0x80000006> */
    0x000500e7, /* jalr x1,0(x10) */
};
static const uint32_t prog_load_unmapped[] = {
    0x00000513, /* addi x10,x0,0 */
    0x00052083, /* lw x1,0(x10) */
};
static const uint32_t prog_store_unmapped[] = {
    0x00000513, /* addi x10,x0,0 */
    0x00700093, /* addi x1,x0,7 */
    0x00152223, /* sw x1,4(x10) */
};
static const uint32_t prog_ecall[] = {
    0x00000073, /* ecall */
};
static const uint32_t prog_mul[] = {
    0x023100b3, /* mul x1,x2,x3 */
};
static const uint32_t prog_csr[] = {
    0xf14020f3, /* csrrs x1,mhartid,x0 */
};
static const uint32_t prog_pass[] = {
    0x00100537, /* lui x10,0x100 */
    0x000055b7, /* lui x11,0x5 */
    0x55558593, /* addi x11,x11,1365 # 5555 <.text+0x5555> */
    0x00b52023, /* sw x11,0(x10) # 100000 <.text+0x100000> */
};
static const uint32_t prog_fail7[] = {
    0x00100537, /* lui x10,0x100 */
    0x000735b7, /* lui x11,0x73 */
    0x33358593, /* addi x11,x11,819 # 73333 <.text+0x73333> */
    0x00b52023, /* sw x11,0(x10) # 100000 <.text+0x100000> */
};
static const uint32_t prog_tohost1[] = {
    0x80001537, /* lui x10,0x80001 */
    0x00100593, /* addi x11,x0,1 */
    0x00b52023, /* sw x11,0(x10) # 80001000 <.text+0x80001000> */
};
static const uint32_t prog_tohost_fail[] = {
    0x80001537, /* lui x10,0x80001 */
    0x00500593, /* addi x11,x0,5 */
    0x00b52023, /* sw x11,0(x10) # 80001000 <.text+0x80001000> */
};
static const uint32_t prog_spin[] = {
    0x00108093, /* addi x1,x1,1 */
    0xffdff06f, /* jal x0,0 <.text> */
};

static Machine m;
static bool have_machine;

/* Fresh machine with the program loaded at the start of main RAM and pc pointing at it. */
static void setup(const uint32_t *prog, size_t nwords)
{
    if (have_machine)
        machine_free(&m);
    CHECK(machine_init(&m));
    have_machine = true;
    for (size_t i = 0; i < nwords; i++)
        CHECK(bus_write(&m.bus, BASE + 4 * (uint32_t)i, 4, prog[i]));
    m.cpu.pc = BASE;
}

/* Run exactly n instructions (machine_run reports RUN_TIMEOUT after n; that is expected here). */
static void run_n(const uint32_t *prog, size_t nwords, uint64_t n)
{
    setup(prog, nwords);
    CHECK(machine_run(&m, n) == RUN_TIMEOUT);
}

#define RUN(p)      run_n(p, LEN(p), LEN(p))
#define X(n, want)  CHECK_EQ_U32(m.cpu.x[n], want)

static void test_alu(void)
{
    RUN(prog_alu);
    X(3, 0x80000000u);  X(4, 0xFFFFFFFFu);  X(5, 0x80000002u);
    X(7, 1);  X(8, 0);  X(9, 1);
    X(10, 0xFFFFFFFFu); X(11, 0xFFFFFFFFu); X(12, 0);
    X(14, 0x80000000u); X(15, 1);  X(16, 0xFFFFFFFFu);
    X(18, 1);                        /* shift by 32 uses shamt 0 */
    X(19, 0x80000000u);
}

static void test_alu_imm(void)
{
    RUN(prog_alu_imm);
    X(2, 0);  X(3, 0xFFFFF7FFu);  X(4, 1);  X(5, 0);  X(6, 1);
    X(7, 0xFFFFFAAAu);  X(8, 0x7FF);  X(9, 0xFFFFFFF0u);
    X(11, 0x80000000u); X(12, 1);  X(13, 0xFFFFFFFFu);  X(14, 0x80000000u);
    X(15, 0x0FFFFFFFu); X(16, 0xFFFFFFFFu);
}

static void test_upper_and_x0(void)
{
    RUN(prog_upper);
    X(1, 0x12345000u);  X(2, 0xFFFFF000u);
    X(3, BASE + 8 + 0x1000u);
    X(4, BASE + 12 - 0x1000u);
    X(0, 0);  X(5, 0);
}

static void test_memory(void)
{
    RUN(prog_mem);
    X(2, 0xFFFFFFF3u);  X(3, 0xF3);  X(4, 0xFFFFF2F3u);  X(5, 0xF2F3);  X(6, 0x8081F2F3u);
    X(7, 0xFFFFFF80u);  X(8, 0x8081);  X(9, 0xFFFF8081u);
    X(11, 0xF3);  X(12, 0xF2F3);  X(13, 0x8081F2F3u);  X(15, 0x8081F2F3u);
    X(16, 0x8081F2F3u);                  /* misaligned lw reads back a misaligned sw */
    X(17, 0x81F2F300u);                  /* ...and straddles into its neighbour bytes */
    X(18, 0xFFFFF2F3u);                  /* misaligned lh, sign-extended */
}

static void test_branches(void)
{
    run_n(prog_branch, LEN(prog_branch), 14);
    X(10, 0);  X(11, 1);  X(12, 0);  X(13, 1);  X(14, 0);  X(15, 1);  X(16, 0);  X(17, 1);
}

static void test_jumps(void)
{
    run_n(prog_jump, LEN(prog_jump), 5);
    X(1, BASE + 4);
    X(2, BASE + 20);
    X(5, 0);  X(6, 0);  X(7, 7);
    CHECK_EQ_U32(m.cpu.pc, BASE + 32);
}

/* Execute one instruction at the start of a fresh program and return its Trap. */
static Trap step_first(const uint32_t *prog, size_t nwords)
{
    StepInfo info;
    setup(prog, nwords);
    return cpu_step(&m.cpu, &m.bus, &info);
}

static void check_trap(Trap t, uint32_t cause, uint32_t tval)
{
    CHECK(t.raised);
    CHECK_EQ_U32(t.cause, cause);
    CHECK_EQ_U32(t.tval, tval);
}

static void test_traps(void)
{
    static const uint32_t zero[] = { 0x00000000u }, ones[] = { 0xFFFFFFFFu };
    StepInfo info;
    Trap t;

    check_trap(step_first(zero, 1), CAUSE_ILLEGAL_INSN, 0);
    check_trap(step_first(ones, 1), CAUSE_ILLEGAL_INSN, 0xFFFFFFFFu);
    check_trap(step_first(prog_ecall, 1), CAUSE_ILLEGAL_INSN, prog_ecall[0]);   /* until Milestone 7 */
    check_trap(step_first(prog_csr, 1), CAUSE_ILLEGAL_INSN, prog_csr[0]);
    check_trap(step_first(prog_mul, 1), CAUSE_ILLEGAL_INSN, prog_mul[0]);       /* until Milestone 6 */

    t = step_first(prog_jal_misaligned, 1);
    check_trap(t, CAUSE_FETCH_MISALIGNED, BASE + 6);
    X(1, 0);                                  /* link register not written on a trap */
    CHECK_EQ_U32(m.cpu.pc, BASE);             /* pc not advanced */

    check_trap(step_first(prog_beq_misaligned, 1), CAUSE_FETCH_MISALIGNED, BASE + 6);
    CHECK(!step_first(prog_bne_not_taken, 1).raised);   /* not taken: target is never checked */
    CHECK_EQ_U32(m.cpu.pc, BASE + 4);

    setup(prog_jalr_misaligned, LEN(prog_jalr_misaligned));
    CHECK(!cpu_step(&m.cpu, &m.bus, &info).raised);
    CHECK(!cpu_step(&m.cpu, &m.bus, &info).raised);
    check_trap(cpu_step(&m.cpu, &m.bus, &info), CAUSE_FETCH_MISALIGNED, BASE + 6);

    setup(prog_load_unmapped, LEN(prog_load_unmapped));
    CHECK(!cpu_step(&m.cpu, &m.bus, &info).raised);
    check_trap(cpu_step(&m.cpu, &m.bus, &info), CAUSE_LOAD_ACCESS, 0);
    X(1, 0);

    setup(prog_store_unmapped, LEN(prog_store_unmapped));
    CHECK(!cpu_step(&m.cpu, &m.bus, &info).raised);
    CHECK(!cpu_step(&m.cpu, &m.bus, &info).raised);
    check_trap(cpu_step(&m.cpu, &m.bus, &info), CAUSE_STORE_ACCESS, 4);

    setup(zero, 1);
    m.cpu.pc = 0;                             /* nothing is mapped at 0 */
    check_trap(cpu_step(&m.cpu, &m.bus, &info), CAUSE_FETCH_ACCESS, 0);
    m.cpu.pc = BASE + 2;
    check_trap(cpu_step(&m.cpu, &m.bus, &info), CAUSE_FETCH_MISALIGNED, BASE + 2);
}

static void test_machine_exits(void)
{
    setup(prog_pass, LEN(prog_pass));
    CHECK(machine_run(&m, LEN(prog_pass)) == RUN_PASS);   /* exit on the very last allowed instruction */
    CHECK(strcmp(m.exit_req.source, "syscon") == 0);

    setup(prog_fail7, LEN(prog_fail7));
    CHECK(machine_run(&m, 100) == RUN_FAIL);
    CHECK_EQ_U32(m.exit_req.code, 7);
    CHECK(strcmp(m.exit_req.source, "syscon") == 0);

    setup(prog_tohost1, LEN(prog_tohost1));
    m.bus.tohost_enabled = true;
    m.bus.tohost_addr = BASE + 0x1000;
    CHECK(machine_run(&m, 100) == RUN_PASS);
    CHECK(strcmp(m.exit_req.source, "tohost") == 0);

    setup(prog_tohost_fail, LEN(prog_tohost_fail));
    m.bus.tohost_enabled = true;
    m.bus.tohost_addr = BASE + 0x1000;
    CHECK(machine_run(&m, 100) == RUN_FAIL);
    CHECK_EQ_U32(m.exit_req.code, 2);          /* 5 >> 1 */
    CHECK(strcmp(m.exit_req.source, "tohost") == 0);

    /* tohost not announced by the ELF: the store is just a store, so the run falls off the
     * end of the program into a zero word (illegal instruction) instead of exiting */
    setup(prog_tohost1, LEN(prog_tohost1));
    CHECK(machine_run(&m, 50) == RUN_ERROR);
    CHECK(m.exit_req.kind == EXIT_NONE);
}

static void test_machine_timeout_and_error(void)
{
    setup(prog_spin, LEN(prog_spin));
    CHECK(machine_run(&m, 1001) == RUN_TIMEOUT);
    X(1, 501);                                 /* exactly 1001 instructions: 501 addi + 500 jal */

    setup(prog_spin, LEN(prog_spin));
    CHECK(machine_run(&m, 0) == RUN_TIMEOUT);
    X(1, 0);

    static const uint32_t zero[] = { 0 };
    setup(zero, 1);
    CHECK(machine_run(&m, 100) == RUN_ERROR);
    CHECK(strncmp(m.error, "unhandled trap cause=0x2", 24) == 0);
}

int main(void)
{
    test_alu();
    test_alu_imm();
    test_upper_and_x0();
    test_memory();
    test_branches();
    test_jumps();
    test_traps();
    test_machine_exits();
    test_machine_timeout_and_error();
    TEST_MAIN_RETURN();
}
