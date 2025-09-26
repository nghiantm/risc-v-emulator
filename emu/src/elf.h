#ifndef ELF_H
#define ELF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bus.h"

/* Reads a whole file into a malloc'd buffer (caller frees). NULL + message in err on failure. */
uint8_t *elf_read_file(const char *path, size_t *len, char *err, size_t errlen);

/* Validates an ELF32 little-endian RISC-V ET_EXEC image and copies every PT_LOAD
 * segment (p_filesz bytes, then zeros up to p_memsz) into main RAM through the bus.
 * Every header and segment is bounds-checked against the image and against main RAM.
 * false + message in err on any problem; the bus may be partly written. */
bool elf_load(const uint8_t *img, size_t len, Bus *bus, uint32_t *entry, char *err, size_t errlen);

/* Looks `name` up in the SHT_SYMTAB symbol table. false if there is none or no match. */
bool elf_find_symbol(const uint8_t *img, size_t len, const char *name, uint32_t *value);

#endif /* ELF_H */
