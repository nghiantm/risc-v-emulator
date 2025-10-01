/* RV32M arithmetic. Pure functions of 32-bit register values: no CPU state, no bus. */
#ifndef ALU_H
#define ALU_H

#include <stdint.h>

/* funct3 order of the M extension: mul mulh mulhsu mulhu div divu rem remu.
 * Operands and results are raw register bit patterns (two's complement). */
uint32_t alu_m(uint32_t funct3, uint32_t a, uint32_t b);

#endif /* ALU_H */
