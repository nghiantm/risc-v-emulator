/* Machine-mode CSR storage with WARL masking. Device interrupt lines arrive as booleans. */
#ifndef CSR_H
#define CSR_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t mstatus;     /* only MIE(3), MPIE(7) stored; MPP(12:11) reads as 0b11 always */
    uint32_t mie;         /* only MTIE(7), MEIE(11) stored */
    uint32_t mtvec;       /* direct mode only: low 2 bits always 0 */
    uint32_t mscratch;
    uint32_t mepc;        /* low 2 bits always 0 (no compressed instructions) */
    uint32_t mcause;
    uint32_t mtval;
    /* misa, mhartid, mip are NOT stored: misa constant, mhartid 0, mip composed from device lines */
} Csr;

/* CSR addresses */
enum {
    CSR_MSTATUS = 0x300, CSR_MISA = 0x301, CSR_MIE = 0x304, CSR_MTVEC = 0x305,
    CSR_MSCRATCH = 0x340, CSR_MEPC = 0x341, CSR_MCAUSE = 0x342, CSR_MTVAL = 0x343,
    CSR_MIP = 0x344, CSR_MHARTID = 0xF14
};

/* mstatus / mie / mip bits */
#define MSTATUS_MIE   (1u << 3)
#define MSTATUS_MPIE  (1u << 7)
#define MSTATUS_MPP   (3u << 11)       /* always reads 3 (machine mode only) */
#define MIP_MTIP      (1u << 7)
#define MIP_MEIP      (1u << 11)
#define MISA_VALUE    0x40001100u      /* MXL=1 (32-bit), extensions I and M */

/* false => no such CSR (the caller raises illegal-instruction). mtip/meip feed the composed mip. */
bool csr_read(const Csr *c, uint32_t addr, bool mtip, bool meip, uint32_t *out);

/* false => no such CSR. Writes to read-only CSRs (misa, mhartid, mip) are accepted and ignored;
 * unimplemented bits of implemented CSRs are dropped. */
bool csr_write(Csr *c, uint32_t addr, uint32_t value);

#endif /* CSR_H */
