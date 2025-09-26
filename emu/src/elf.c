#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elf.h"
#include "memory_map.h"

enum {
    EHDR_SIZE = 52, PHDR_SIZE = 32, SHDR_SIZE = 40, SYM_SIZE = 16,
    ET_EXEC = 2, EM_RISCV = 243, PT_LOAD = 1, SHT_SYMTAB = 2
};

/* All readers assume the caller already checked that the bytes are inside the image. */
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* true if [off, off + n) lies inside an image of len bytes (no overflow: 64-bit sum). */
static bool in_image(size_t len, uint32_t off, uint64_t n)
{
    return (uint64_t)off + n <= len;
}

uint8_t *elf_read_file(const char *path, size_t *len, char *err, size_t errlen)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(err, errlen, "cannot open %s", path);
        return NULL;
    }
    size_t cap = 1 << 16, n = 0;
    uint8_t *buf = malloc(cap);
    while (buf) {
        n += fread(buf + n, 1, cap - n, f);
        if (n < cap)
            break;
        cap *= 2;
        uint8_t *bigger = realloc(buf, cap);
        if (!bigger)
            free(buf);
        buf = bigger;
    }
    bool bad = ferror(f);
    fclose(f);
    if (!buf || bad) {
        free(buf);
        snprintf(err, errlen, "cannot read %s", path);
        return NULL;
    }
    *len = n;
    return buf;
}

bool elf_load(const uint8_t *img, size_t len, Bus *bus, uint32_t *entry, char *err, size_t errlen)
{
    if (len < EHDR_SIZE) {
        snprintf(err, errlen, "not an ELF file (truncated header)");
        return false;
    }
    if (memcmp(img, "\x7f" "ELF", 4) != 0) {
        snprintf(err, errlen, "not an ELF file (bad magic)");
        return false;
    }
    if (img[4] != 1 || img[5] != 1) {
        snprintf(err, errlen, "not a 32-bit little-endian ELF");
        return false;
    }
    if (rd16(img + 16) != ET_EXEC || rd16(img + 18) != EM_RISCV) {
        snprintf(err, errlen, "not a RISC-V executable ELF");
        return false;
    }

    uint32_t phoff = rd32(img + 28);
    uint32_t phnum = rd16(img + 44);
    if (rd16(img + 42) != PHDR_SIZE || !in_image(len, phoff, (uint64_t)phnum * PHDR_SIZE)) {
        snprintf(err, errlen, "bad program header table");
        return false;
    }

    for (uint32_t i = 0; i < phnum; i++) {
        const uint8_t *ph = img + phoff + i * PHDR_SIZE;
        if (rd32(ph) != PT_LOAD)
            continue;
        uint32_t offset = rd32(ph + 4), vaddr = rd32(ph + 8);
        uint32_t filesz = rd32(ph + 16), memsz = rd32(ph + 20);

        if (filesz > memsz || !in_image(len, offset, filesz)) {
            snprintf(err, errlen, "segment %u: contents outside file", (unsigned)i);
            return false;
        }
        if (vaddr < MAIN_RAM_BASE || (uint64_t)vaddr + memsz > (uint64_t)MAIN_RAM_BASE + MAIN_RAM_SIZE) {
            snprintf(err, errlen, "segment %u: 0x%08x+0x%x outside main RAM", (unsigned)i,
                     (unsigned)vaddr, (unsigned)memsz);
            return false;
        }
        for (uint32_t b = 0; b < memsz; b++) {
            uint32_t byte = b < filesz ? img[offset + b] : 0;
            if (!bus_write(bus, vaddr + b, 1, byte)) {
                snprintf(err, errlen, "segment %u: write fault at 0x%08x", (unsigned)i,
                         (unsigned)(vaddr + b));
                return false;
            }
        }
    }

    *entry = rd32(img + 24);
    return true;
}

bool elf_find_symbol(const uint8_t *img, size_t len, const char *name, uint32_t *value)
{
    if (len < EHDR_SIZE)
        return false;
    uint32_t shoff = rd32(img + 32);
    uint32_t shnum = rd16(img + 48);
    if (rd16(img + 46) != SHDR_SIZE || !in_image(len, shoff, (uint64_t)shnum * SHDR_SIZE))
        return false;

    for (uint32_t s = 0; s < shnum; s++) {
        const uint8_t *sh = img + shoff + s * SHDR_SIZE;
        if (rd32(sh + 4) != SHT_SYMTAB)
            continue;
        uint32_t symoff = rd32(sh + 16), symsize = rd32(sh + 20), strndx = rd32(sh + 24);
        if (strndx >= shnum || !in_image(len, symoff, symsize))
            return false;
        const uint8_t *strsh = img + shoff + strndx * SHDR_SIZE;
        uint32_t stroff = rd32(strsh + 16), strsize = rd32(strsh + 20);
        if (!in_image(len, stroff, strsize))
            return false;

        for (uint32_t i = 0; i + SYM_SIZE <= symsize; i += SYM_SIZE) {
            const uint8_t *sym = img + symoff + i;
            uint32_t nameoff = rd32(sym);
            if (nameoff >= strsize)
                continue;
            /* bounded compare: the string must end inside the string table */
            size_t room = strsize - nameoff;
            const char *str = (const char *)img + stroff + nameoff;
            if (memchr(str, 0, room) != NULL && strcmp(str, name) == 0) {
                *value = rd32(sym + 4);
                return true;
            }
        }
        return false;       /* only one SHT_SYMTAB per file */
    }
    return false;
}
