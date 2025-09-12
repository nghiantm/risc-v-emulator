#include "decode.h"

/* Sign-extend the low `bits` bits of v. Done with an xor/subtract trick so no
 * implementation-defined shift of a negative value is needed. */
static int32_t sign_extend(uint32_t v, unsigned bits)
{
    uint32_t sign = 1u << (bits - 1);
    return (int32_t)((v ^ sign) - sign);
}

/* Every format keeps its sign bit at insn[31] and rs1/rs2/rd at fixed
 * positions. That lets the register file be read before the format is known;
 * the price is that the other immediate bits are scattered. */

int32_t imm_i(uint32_t insn)
{
    return sign_extend(insn >> 20, 12);                 /* imm[11:0] = insn[31:20] */
}

int32_t imm_s(uint32_t insn)
{
    /* rd's slot (insn[11:7]) is reused for imm[4:0] because stores have no rd. */
    uint32_t v = ((insn >> 25) << 5)                    /* imm[11:5] = insn[31:25] */
               | ((insn >> 7) & 0x1Fu);                 /* imm[4:0]  = insn[11:7]  */
    return sign_extend(v, 12);
}

int32_t imm_b(uint32_t insn)
{
    /* Same slots as S, but the immediate is a multiple of 2, so imm[0] is not
     * stored; imm[11] takes insn[7] (the slot S uses for imm[0]) to keep
     * every other bit in the S position. */
    uint32_t v = (((insn >> 31) & 1u) << 12)            /* imm[12]   = insn[31]    */
               | (((insn >> 7)  & 1u) << 11)            /* imm[11]   = insn[7]     */
               | (((insn >> 25) & 0x3Fu) << 5)          /* imm[10:5] = insn[30:25] */
               | (((insn >> 8)  & 0xFu) << 1);          /* imm[4:1]  = insn[11:8]  */
    return sign_extend(v, 13);
}

int32_t imm_u(uint32_t insn)
{
    return (int32_t)(insn & 0xFFFFF000u);               /* imm[31:12] = insn[31:12], low 12 zero */
}

int32_t imm_j(uint32_t insn)
{
    /* U-type layout reshuffled so imm[10:1] and the sign bit line up with B. */
    uint32_t v = (((insn >> 31) & 1u) << 20)            /* imm[20]    = insn[31]    */
               | (((insn >> 12) & 0xFFu) << 12)         /* imm[19:12] = insn[19:12] (as in U) */
               | (((insn >> 20) & 1u) << 11)            /* imm[11]    = insn[20]    */
               | (((insn >> 21) & 0x3FFu) << 1);        /* imm[10:1]  = insn[30:21] */
    return sign_extend(v, 21);
}
