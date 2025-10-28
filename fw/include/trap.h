#ifndef FW_TRAP_H
#define FW_TRAP_H

#include <stdint.h>

#define MSTATUS_MIE (1u << 3)
#define MIE_MTIE    (1u << 7)
#define MIE_MEIE    (1u << 11)

#define csr_read(csr) ({ uint32_t v_; __asm__ volatile ("csrr %0, " #csr : "=r"(v_)); v_; })
#define csr_write(csr, val) __asm__ volatile ("csrw " #csr ", %0" :: "r"((uint32_t)(val)))
#define csr_set(csr, val)   __asm__ volatile ("csrs " #csr ", %0" :: "r"((uint32_t)(val)))
#define csr_clear(csr, val) __asm__ volatile ("csrc " #csr ", %0" :: "r"((uint32_t)(val)))

typedef void (*trap_handler_t)(uint32_t cause, uint32_t epc);

/* Points mtvec at trap_entry (crt0.S) and routes every trap to handler. */
void trap_install(trap_handler_t handler);

static inline void irq_enable_timer(void)    { csr_set(mie, MIE_MTIE); }
static inline void irq_disable_timer(void)   { csr_clear(mie, MIE_MTIE); }
static inline void irq_enable_external(void) { csr_set(mie, MIE_MEIE); }
static inline void irq_disable_external(void){ csr_clear(mie, MIE_MEIE); }
static inline void irq_global_enable(void)   { csr_set(mstatus, MSTATUS_MIE); }
static inline void irq_global_disable(void)  { csr_clear(mstatus, MSTATUS_MIE); }

#endif
