/* Vectors were hand-assembled per the RISC-V spec; the expected value is next to each. */
#include <stdint.h>

#include "decode.h"
#include "test_util.h"

static void test_fields(void)
{
    /* add x3, x1, x2 = 0x002081B3 */
    uint32_t i = 0x002081B3u;
    CHECK_EQ_U32(insn_opcode(i), 0x33);
    CHECK_EQ_U32(insn_rd(i), 3);
    CHECK_EQ_U32(insn_funct3(i), 0);
    CHECK_EQ_U32(insn_rs1(i), 1);
    CHECK_EQ_U32(insn_rs2(i), 2);
    CHECK_EQ_U32(insn_funct7(i), 0);
    /* sub x31, x30, x29 = 0x41DF0FB3 */
    i = 0x41DF0FB3u;
    CHECK_EQ_U32(insn_rd(i), 31);
    CHECK_EQ_U32(insn_rs1(i), 30);
    CHECK_EQ_U32(insn_rs2(i), 29);
    CHECK_EQ_U32(insn_funct7(i), 0x20);
}

static void test_imm_i(void)
{
    CHECK_EQ_U32(imm_i(0x00100093u), (uint32_t)(1));  /* addi x1,x0,1 */
    CHECK_EQ_U32(imm_i(0x7FF00093u), (uint32_t)(2047));  /* addi x1,x0,2047 */
    CHECK_EQ_U32(imm_i(0xFFF00093u), (uint32_t)(-1));  /* addi x1,x0,-1 */
    CHECK_EQ_U32(imm_i(0x80000093u), (uint32_t)(-2048));  /* addi x1,x0,-2048 */
    CHECK_EQ_U32(imm_i(0xFDB00093u), (uint32_t)(-37));  /* addi x1,x0,-37 */
}

static void test_imm_s(void)
{
    CHECK_EQ_U32(imm_s(0x0020A223u), (uint32_t)(4));  /* sw x2,4(x1) */
    CHECK_EQ_U32(imm_s(0x7E20AFA3u), (uint32_t)(2047));  /* sw x2,2047(x1) */
    CHECK_EQ_U32(imm_s(0xFE20AFA3u), (uint32_t)(-1));  /* sw x2,-1(x1) */
    CHECK_EQ_U32(imm_s(0x8020A023u), (uint32_t)(-2048));  /* sw x2,-2048(x1) */
    CHECK_EQ_U32(imm_s(0xF820AE23u), (uint32_t)(-100));  /* sw x2,-100(x1) */
}

static void test_imm_b(void)
{
    CHECK_EQ_U32(imm_b(0x00208463u), (uint32_t)(8));  /* beq x1,x2,8 */
    CHECK_EQ_U32(imm_b(0x7E208FE3u), (uint32_t)(4094));  /* beq x1,x2,4094 */
    CHECK_EQ_U32(imm_b(0xFE208FE3u), (uint32_t)(-2));  /* beq x1,x2,-2 */
    CHECK_EQ_U32(imm_b(0x80208063u), (uint32_t)(-4096));  /* beq x1,x2,-4096 */
    CHECK_EQ_U32(imm_b(0xFE208CE3u), (uint32_t)(-8));  /* beq x1,x2,-8 */
    CHECK_EQ_U32(imm_b(0x002080E3u), (uint32_t)(2048));  /* beq x1,x2,2048 */
    CHECK_EQ_U32(imm_b(0x802080E3u), (uint32_t)(-2048));  /* beq x1,x2,-2048 */
}

static void test_imm_u(void)
{
    CHECK_EQ_U32(imm_u(0x123450B7u), (uint32_t)(0x12345000));  /* lui x1,0x12345 */
    CHECK_EQ_U32(imm_u(0xFFFFF0B7u), (uint32_t)(0xFFFFF000));  /* lui x1,0xFFFFF */
    CHECK_EQ_U32(imm_u(0x800000B7u), (uint32_t)(0x80000000));  /* lui x1,0x80000 (min) */
    CHECK_EQ_U32(imm_u(0x000000B7u), (uint32_t)(0));  /* lui x1,0 */
    CHECK_EQ_U32(imm_u(0x7FFFF097u), (uint32_t)(0x7FFFF000));  /* auipc x1,0x7FFFF (max positive) */
}

static void test_imm_j(void)
{
    CHECK_EQ_U32(imm_j(0x008000EFu), (uint32_t)(8));  /* jal x1,8 */
    CHECK_EQ_U32(imm_j(0x7FFFF0EFu), (uint32_t)(1048574));  /* jal x1,1048574 */
    CHECK_EQ_U32(imm_j(0xFFFFF0EFu), (uint32_t)(-2));  /* jal x1,-2 */
    CHECK_EQ_U32(imm_j(0x800000EFu), (uint32_t)(-1048576));  /* jal x1,-1048576 */
    CHECK_EQ_U32(imm_j(0x800FF0EFu), (uint32_t)(-4096));  /* jal x1,-4096 */
    CHECK_EQ_U32(imm_j(0x001000EFu), (uint32_t)(2048));  /* jal x1,2048 */
    CHECK_EQ_U32(imm_j(0x801FF0EFu), (uint32_t)(-2048));  /* jal x1,-2048 */
    CHECK_EQ_U32(imm_j(0x0007F0EFu), (uint32_t)(520192));  /* jal x1,520192 */
}


static void test_even(void)
{
    /* imm_b / imm_j never produce bit 0, whatever the (ignored) insn[8]/[21] ... */
    for (uint32_t i = 0; i < 64; i++) {
        uint32_t insn = i * 0x9E3779B1u;
        CHECK((imm_b(insn) & 1) == 0);
        CHECK((imm_j(insn) & 1) == 0);
    }
}

int main(void)
{
    test_fields();
    test_imm_i();
    test_imm_s();
    test_imm_b();
    test_imm_u();
    test_imm_j();
    test_even();
    TEST_MAIN_RETURN();
}
