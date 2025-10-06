#ifndef CPU_H
#define CPU_H

#include <stdbool.h>
#include <stdint.h>

#include "bus.h"
#include "csr.h"

typedef struct {
    uint32_t x[32];       /* x[0] always reads 0; writes to x0 are discarded */
    uint32_t pc;
    Csr      csr;
    bool     mtip, meip;  /* device interrupt lines, refreshed by the machine loop before each step */
} Cpu;

/* A synchronous exception raised while executing one instruction. */
typedef struct {
    bool     raised;
    uint32_t cause;       /* mcause value, no interrupt bit */
    uint32_t tval;        /* value for mtval */
} Trap;

/* Exception cause codes */
enum {
    CAUSE_FETCH_MISALIGNED = 0,  CAUSE_FETCH_ACCESS = 1,   CAUSE_ILLEGAL_INSN = 2,
    CAUSE_BREAKPOINT = 3,        CAUSE_LOAD_ACCESS = 5,    CAUSE_STORE_ACCESS = 7,
    CAUSE_ECALL_M = 11
};
/* Load-misaligned (4) and store-misaligned (6) are intentionally unused: misaligned RAM access works. */

/* What one executed instruction did; the trace printer reads it. */
typedef struct {
    uint32_t pc;          /* address the instruction was fetched from */
    uint32_t insn;
    bool     wrote;       /* a register other than x0 was written */
    uint32_t rd;
    uint32_t value;
} StepInfo;

/* Fetch, decode and execute one instruction. On a Trap pc and registers are unchanged. */
Trap cpu_step(Cpu *cpu, Bus *bus, StepInfo *info);

#endif /* CPU_H */
