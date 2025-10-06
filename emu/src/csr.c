#include "csr.h"

#define MIE_MASK     (MIP_MTIP | MIP_MEIP)
#define MSTATUS_MASK (MSTATUS_MIE | MSTATUS_MPIE)

bool csr_read(const Csr *c, uint32_t addr, bool mtip, bool meip, uint32_t *out)
{
    switch (addr) {
    case CSR_MSTATUS:  *out = c->mstatus | MSTATUS_MPP; return true;
    case CSR_MISA:     *out = MISA_VALUE;               return true;
    case CSR_MIE:      *out = c->mie;                   return true;
    case CSR_MTVEC:    *out = c->mtvec;                 return true;
    case CSR_MSCRATCH: *out = c->mscratch;              return true;
    case CSR_MEPC:     *out = c->mepc;                  return true;
    case CSR_MCAUSE:   *out = c->mcause;                return true;
    case CSR_MTVAL:    *out = c->mtval;                 return true;
    case CSR_MIP:      *out = (mtip ? MIP_MTIP : 0) | (meip ? MIP_MEIP : 0); return true;
    case CSR_MHARTID:  *out = 0;                        return true;
    default:           return false;
    }
}

bool csr_write(Csr *c, uint32_t addr, uint32_t value)
{
    switch (addr) {
    case CSR_MSTATUS:  c->mstatus = value & MSTATUS_MASK; return true;
    case CSR_MIE:      c->mie = value & MIE_MASK;         return true;
    case CSR_MTVEC:    c->mtvec = value & ~3u;            return true;
    case CSR_MSCRATCH: c->mscratch = value;               return true;
    case CSR_MEPC:     c->mepc = value & ~3u;             return true;
    case CSR_MCAUSE:   c->mcause = value;                 return true;
    case CSR_MTVAL:    c->mtval = value;                  return true;
    case CSR_MISA:
    case CSR_MIP:
    case CSR_MHARTID:  return true;                       /* read-only: write ignored, no trap */
    default:           return false;
    }
}
