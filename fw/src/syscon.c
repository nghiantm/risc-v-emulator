#include "memory_map.h"
#include "syscon.h"

void syscon_write(uint32_t word)
{
    *(volatile uint32_t *)SYSCON_BASE = word;
    for (;;)                         /* the emulator stops on the write; spin if it somehow does not */
        ;
}

void syscon_exit(uint32_t code)
{
    syscon_write(code == 0 ? SYSCON_PASS : (code << 16) | SYSCON_FAIL_TAG);
}
