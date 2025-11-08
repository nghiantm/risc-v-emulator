#ifndef FW_RAM_TESTS_H
#define FW_RAM_TESTS_H

#include "diag.h"

/* All three touch DUT RAM only, never main RAM or the stack. */
DiagResult ram_test_data_bus(void);        /* walking 1s and 0s at one word */
DiagResult ram_test_addr_bus(void);        /* aliasing between power-of-two word offsets */
DiagResult ram_test_march_c_minus(void);   /* March C- over the whole 64 KiB */

#endif
