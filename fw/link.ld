/* Everything lives in main RAM. Keep ORIGIN/LENGTH in sync with common/memory_map.h
 * (MAIN_RAM_BASE, MAIN_RAM_SIZE); a linker script cannot include a C header. */
ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 0x400000
}

SECTIONS
{
    /* _start must sit at the exact RAM base: the ELF entry point is also there. */
    .text : {
        KEEP(*(.text.init))
        *(.text .text.*)
    } > RAM

    .rodata : { *(.rodata .rodata.* .srodata .srodata.*) } > RAM

    .data : { *(.data .data.* .sdata .sdata.*) } > RAM

    .bss : {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss .bss.* .sbss .sbss.* COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > RAM

    /* Stack grows down from the end of RAM. */
    __stack_top = ORIGIN(RAM) + LENGTH(RAM);

    /DISCARD/ : { *(.comment) *(.riscv.attributes) *(.eh_frame*) }
}
