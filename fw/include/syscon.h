#ifndef FW_SYSCON_H
#define FW_SYSCON_H

#include <stdint.h>

/* Ends the run. 0 reports pass; nonzero reports fail with that code (low 16 bits). */
void syscon_exit(uint32_t code) __attribute__((noreturn));
/* Ends the run with a raw SYSCON word (used by diag, whose code is a bitmask). */
void syscon_write(uint32_t word) __attribute__((noreturn));

#endif
