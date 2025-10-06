#include "trap.h"

void trap_enter(Cpu *cpu, uint32_t cause, uint32_t tval, uint32_t pc)
{
    Csr *c = &cpu->csr;
    c->mepc = pc;
    c->mcause = cause;
    c->mtval = tval;
    if (c->mstatus & MSTATUS_MIE)
        c->mstatus |= MSTATUS_MPIE;
    else
        c->mstatus &= ~MSTATUS_MPIE;
    c->mstatus &= ~MSTATUS_MIE;
    cpu->pc = c->mtvec;
}

uint32_t trap_return(Csr *c)
{
    if (c->mstatus & MSTATUS_MPIE)
        c->mstatus |= MSTATUS_MIE;
    else
        c->mstatus &= ~MSTATUS_MIE;
    c->mstatus |= MSTATUS_MPIE;
    return c->mepc;
}
