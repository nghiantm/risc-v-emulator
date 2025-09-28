#include "cpu.h"
#include "decode.h"

enum {
    OP_LOAD = 0x03, OP_MISC_MEM = 0x0F, OP_IMM = 0x13, OP_AUIPC = 0x17, OP_STORE = 0x23,
    OP_REG = 0x33, OP_LUI = 0x37, OP_BRANCH = 0x63, OP_JALR = 0x67, OP_JAL = 0x6F
};

#define SIGN_BIT 0x80000000u

static Trap trap(uint32_t cause, uint32_t tval)
{
    return (Trap){ .raised = true, .cause = cause, .tval = tval };
}

static Trap illegal(uint32_t insn) { return trap(CAUSE_ILLEGAL_INSN, insn); }

/* Signed less-than without casting out-of-range values to int32_t:
 * flipping the sign bit turns signed order into unsigned order. */
static bool slt(uint32_t a, uint32_t b) { return (a ^ SIGN_BIT) < (b ^ SIGN_BIT); }

/* Arithmetic right shift built from a logical one: copy the sign bit into the vacated top bits. */
static uint32_t sra(uint32_t a, uint32_t sh)
{
    uint32_t r = a >> sh;
    if (a & SIGN_BIT)
        r |= ~(0xFFFFFFFFu >> sh);
    return r;
}

/* Shared by OP (register) and OP-IMM. `alt` selects sub (funct3 0) or sra (funct3 5). */
static uint32_t alu(uint32_t funct3, bool alt, uint32_t a, uint32_t b)
{
    switch (funct3) {
    case 0:  return alt ? a - b : a + b;
    case 1:  return a << (b & 0x1F);
    case 2:  return slt(a, b);
    case 3:  return a < b;
    case 4:  return a ^ b;
    case 5:  return alt ? sra(a, b & 0x1F) : a >> (b & 0x1F);
    case 6:  return a | b;
    default: return a & b;
    }
}

static bool branch_taken(uint32_t funct3, uint32_t a, uint32_t b, bool *legal)
{
    *legal = true;
    switch (funct3) {
    case 0: return a == b;
    case 1: return a != b;
    case 4: return slt(a, b);
    case 5: return !slt(a, b);
    case 6: return a < b;
    case 7: return a >= b;
    default: *legal = false; return false;
    }
}

Trap cpu_step(Cpu *cpu, Bus *bus, StepInfo *info)
{
    uint32_t pc = cpu->pc, insn;

    info->pc = pc;
    info->insn = 0;
    info->wrote = false;

    if (pc & 3u)
        return trap(CAUSE_FETCH_MISALIGNED, pc);
    if (!bus_read(bus, pc, 4, &insn))
        return trap(CAUSE_FETCH_ACCESS, pc);
    info->insn = insn;

    uint32_t opcode = insn_opcode(insn), rd = insn_rd(insn), funct3 = insn_funct3(insn);
    uint32_t funct7 = insn_funct7(insn);
    uint32_t a = cpu->x[insn_rs1(insn)], b = cpu->x[insn_rs2(insn)];
    uint32_t next = pc + 4;
    uint32_t result = 0;
    bool write = false;

    switch (opcode) {
    case OP_LUI:
        result = (uint32_t)imm_u(insn);
        write = true;
        break;
    case OP_AUIPC:
        result = pc + (uint32_t)imm_u(insn);
        write = true;
        break;
    case OP_JAL:
    case OP_JALR: {
        uint32_t target;
        if (opcode == OP_JAL) {
            target = pc + (uint32_t)imm_j(insn);
        } else {
            if (funct3 != 0)
                return illegal(insn);
            target = (a + (uint32_t)imm_i(insn)) & ~1u;    /* jalr clears bit 0 */
        }
        if (target & 3u)
            return trap(CAUSE_FETCH_MISALIGNED, target);   /* before the link is written */
        result = next;
        write = true;
        next = target;
        break;
    }
    case OP_BRANCH: {
        bool legal;
        bool taken = branch_taken(funct3, a, b, &legal);
        if (!legal)
            return illegal(insn);
        if (taken) {
            uint32_t target = pc + (uint32_t)imm_b(insn);
            if (target & 3u)
                return trap(CAUSE_FETCH_MISALIGNED, target);
            next = target;
        }
        break;
    }
    case OP_LOAD: {
        static const unsigned size[8] = { 1, 2, 4, 0, 1, 2, 0, 0 };   /* lb lh lw - lbu lhu - - */
        if (size[funct3] == 0)
            return illegal(insn);
        uint32_t addr = a + (uint32_t)imm_i(insn), v;
        if (!bus_read(bus, addr, size[funct3], &v))
            return trap(CAUSE_LOAD_ACCESS, addr);
        if (funct3 == 0)                    /* lb: sign-extend from bit 7 */
            v = (v ^ 0x80u) - 0x80u;
        else if (funct3 == 1)               /* lh: sign-extend from bit 15 */
            v = (v ^ 0x8000u) - 0x8000u;
        result = v;
        write = true;
        break;
    }
    case OP_STORE: {
        if (funct3 > 2)
            return illegal(insn);
        uint32_t addr = a + (uint32_t)imm_s(insn);
        if (!bus_write(bus, addr, 1u << funct3, b))
            return trap(CAUSE_STORE_ACCESS, addr);
        break;
    }
    case OP_IMM:
        /* shifts: funct7 picks logical vs arithmetic and must otherwise be 0 */
        if (funct3 == 1 && funct7 != 0x00)
            return illegal(insn);
        if (funct3 == 5 && funct7 != 0x00 && funct7 != 0x20)
            return illegal(insn);
        result = alu(funct3, funct3 == 5 && funct7 == 0x20, a,
                     funct3 == 1 || funct3 == 5 ? insn_rs2(insn) : (uint32_t)imm_i(insn));
        write = true;
        break;
    case OP_REG:
        if (funct7 != 0x00 && !(funct7 == 0x20 && (funct3 == 0 || funct3 == 5)))
            return illegal(insn);           /* funct7 0x01 (M extension) arrives in Milestone 6 */
        result = alu(funct3, funct7 == 0x20, a, b);
        write = true;
        break;
    case OP_MISC_MEM:                       /* fence, fence.i: nothing to synchronise */
        if (funct3 > 1)
            return illegal(insn);
        break;
    default:                                /* includes SYSTEM: ecall/ebreak/mret/CSR come later */
        return illegal(insn);
    }

    if (write && rd != 0) {
        cpu->x[rd] = result;
        info->wrote = true;
        info->rd = rd;
        info->value = result;
    }
    cpu->pc = next;
    return (Trap){ .raised = false };
}
