#ifndef FW_CLINT_H
#define FW_CLINT_H

#include <stdint.h>

typedef struct { uint32_t lo, hi; } Mtime64;     /* no libgcc: 64-bit values are kept as two words */

Mtime64 clint_read_mtime(void);
/* Arms the timer to fire `delta` ticks from now (delta < 2^32). */
void clint_set_timer_after(uint32_t delta);
/* Pushes mtimecmp to the maximum so MTIP drops and stays low. */
void clint_disable_timer(void);

#endif
