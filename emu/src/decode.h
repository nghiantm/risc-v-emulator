/* Instruction field extraction and immediate decoding. Pure functions of a
 * 32-bit instruction word: no CPU state, no bus. */
#ifndef DECODE_H
#define DECODE_H

#include <stdint.h>

static inline uint32_t insn_opcode(uint32_t i) { return i & 0x7Fu; }
static inline uint32_t insn_rd    (uint32_t i) { return (i >> 7) & 0x1Fu; }
static inline uint32_t insn_funct3(uint32_t i) { return (i >> 12) & 0x7u; }
static inline uint32_t insn_rs1   (uint32_t i) { return (i >> 15) & 0x1Fu; }
static inline uint32_t insn_rs2   (uint32_t i) { return (i >> 20) & 0x1Fu; }
static inline uint32_t insn_funct7(uint32_t i) { return (i >> 25) & 0x7Fu; }

int32_t imm_i(uint32_t insn);   /* sign-extended */
int32_t imm_s(uint32_t insn);
int32_t imm_b(uint32_t insn);   /* bit 0 is 0 */
int32_t imm_u(uint32_t insn);   /* already shifted left 12 */
int32_t imm_j(uint32_t insn);   /* bit 0 is 0 */

#endif /* DECODE_H */
