/* Reset code and trap entry. No libc: this is everything that runs before main. */
    .section .text.init
    .globl _start
_start:
    la   sp, __stack_top

    /* Zero .bss one word at a time (the linker aligns both symbols to 4). */
    la   t0, __bss_start
    la   t1, __bss_end
1:  bgeu t0, t1, 2f
    sw   zero, 0(t0)
    addi t0, t0, 4
    j    1b

2:  call main
    call syscon_exit        /* a0 = main's return value; never returns */
3:  j    3b

/* Placeholder until Milestone 11 installs a real handler. */
    .text
    .align 2
    .globl trap_entry
trap_entry:
1:  j    1b
