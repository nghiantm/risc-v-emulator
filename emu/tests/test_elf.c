#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bus.h"
#include "elf.h"
#include "memory_map.h"
#include "ram.h"
#include "test_util.h"

/* ---- tiny ELF image builder (offsets are fixed, so the test reads like a hex dump) ----
 * 0x00 ehdr (52) | 0x34 phdr[0] (32) | 0x54 phdr[1] (32) | 0x74 payload (8 bytes)
 * | 0x7C strtab | 0x90 symtab (3 entries) | 0xC0 shdr[0..2] (null, symtab, strtab) */
#define OFF_PH     0x34
#define OFF_DATA   0x74
#define OFF_STR    0x7C
#define OFF_SYM    0x90
#define OFF_SH     0xC0
#define IMG_LEN    (OFF_SH + 3 * 40)
#define STR_LEN    0x14

static void w16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void w32(uint8_t *p, uint32_t v) { w16(p, (uint16_t)v); w16(p + 2, (uint16_t)(v >> 16)); }

static void build(uint8_t *img, int with_tohost)
{
    memset(img, 0, IMG_LEN);
    memcpy(img, "\x7f" "ELF", 4);
    img[4] = 1; img[5] = 1; img[6] = 1;
    w16(img + 16, 2);  w16(img + 18, 243);  w32(img + 20, 1);
    w32(img + 24, MAIN_RAM_BASE);                    /* entry */
    w32(img + 28, OFF_PH);  w32(img + 32, OFF_SH);
    w16(img + 40, 52); w16(img + 42, 32); w16(img + 44, 2);
    w16(img + 46, 40); w16(img + 48, 3);

    /* phdr[0]: PT_LOAD, 8 file bytes, 16 mem bytes -> 8 bytes must be zero-filled */
    uint8_t *ph = img + OFF_PH;
    w32(ph, 1); w32(ph + 4, OFF_DATA); w32(ph + 8, MAIN_RAM_BASE); w32(ph + 12, MAIN_RAM_BASE);
    w32(ph + 16, 8); w32(ph + 20, 16);
    /* phdr[1]: PT_NOTE, must be ignored even though it points nowhere sensible */
    w32(img + OFF_PH + 32, 4);  w32(img + OFF_PH + 32 + 4, 0xFFFFFFF0u);

    memcpy(img + OFF_DATA, "\x11\x22\x33\x44\x55\x66\x77\x88", 8);

    /* strtab: "\0foo\0tohost\0" */
    memcpy(img + OFF_STR + 1, "foo", 3);
    memcpy(img + OFF_STR + 5, with_tohost ? "tohost" : "tohos_", 6);
    w32(img + OFF_SYM + 16, 1);  w32(img + OFF_SYM + 16 + 4, 0x80000010u);   /* foo    */
    w32(img + OFF_SYM + 32, 5);  w32(img + OFF_SYM + 32 + 4, 0x80001000u);   /* tohost */

    uint8_t *sh = img + OFF_SH + 40;                 /* shdr[1]: SHT_SYMTAB -> strtab is shdr[2] */
    w32(sh + 4, 2); w32(sh + 16, OFF_SYM); w32(sh + 20, 48); w32(sh + 24, 2);
    sh += 40;                                        /* shdr[2]: SHT_STRTAB */
    w32(sh + 4, 3); w32(sh + 16, OFF_STR); w32(sh + 20, STR_LEN);
}

static Bus bus;
static Ram ram;

static void fresh_bus(void)
{
    if (ram.mem)
        ram_free(&ram);
    bus_init(&bus);
    CHECK(ram_init(&ram, "main_ram", MAIN_RAM_BASE, MAIN_RAM_SIZE));
    CHECK(bus_add(&bus, &ram.dev));
}

static bool load(const uint8_t *img, size_t len, uint32_t *entry)
{
    char err[128];
    fresh_bus();
    return elf_load(img, len, &bus, entry, err, sizeof err);
}

static void test_valid(void)
{
    uint8_t img[IMG_LEN];
    uint32_t entry = 0, v;
    build(img, 1);
    CHECK(load(img, sizeof img, &entry));
    CHECK_EQ_U32(entry, MAIN_RAM_BASE);
    CHECK(bus_read(&bus, MAIN_RAM_BASE, 4, &v));      CHECK_EQ_U32(v, 0x44332211u);
    CHECK(bus_read(&bus, MAIN_RAM_BASE + 4, 4, &v));  CHECK_EQ_U32(v, 0x88776655u);
    CHECK(bus_read(&bus, MAIN_RAM_BASE + 8, 4, &v));  CHECK_EQ_U32(v, 0);          /* bss part */
    CHECK(bus_read(&bus, MAIN_RAM_BASE + 12, 4, &v)); CHECK_EQ_U32(v, 0);
}

static void test_zero_fill_overwrites(void)
{
    uint8_t img[IMG_LEN];
    uint32_t entry, v;
    build(img, 1);
    fresh_bus();
    CHECK(bus_write(&bus, MAIN_RAM_BASE + 12, 4, 0xFFFFFFFFu));      /* dirty the bss area first */
    char err[128];
    CHECK(elf_load(img, sizeof img, &bus, &entry, err, sizeof err));
    CHECK(bus_read(&bus, MAIN_RAM_BASE + 12, 4, &v)); CHECK_EQ_U32(v, 0);
}

static void test_symbols(void)
{
    uint8_t img[IMG_LEN];
    uint32_t v = 0;
    build(img, 1);
    CHECK(elf_find_symbol(img, sizeof img, "tohost", &v));  CHECK_EQ_U32(v, 0x80001000u);
    CHECK(elf_find_symbol(img, sizeof img, "foo", &v));     CHECK_EQ_U32(v, 0x80000010u);
    CHECK(!elf_find_symbol(img, sizeof img, "fromhost", &v));
    CHECK(!elf_find_symbol(img, sizeof img, "toho", &v));            /* no prefix matches */
    build(img, 0);
    CHECK(!elf_find_symbol(img, sizeof img, "tohost", &v));
    /* symtab pointing past the end of the file must not read out of bounds */
    build(img, 1);
    w32(img + OFF_SH + 40 + 16, IMG_LEN);
    CHECK(!elf_find_symbol(img, sizeof img, "tohost", &v));
    /* string table without a terminating NUL before its end */
    build(img, 1);
    w32(img + OFF_SH + 80 + 20, 0x0B);                               /* ends in the middle of "tohost" */
    CHECK(!elf_find_symbol(img, sizeof img, "tohost", &v));
    CHECK(!elf_find_symbol(img, 10, "tohost", &v));                  /* truncated header */
}

static void test_rejects(void)
{
    uint8_t img[IMG_LEN];
    uint32_t entry;

    build(img, 1); img[0] = 0;                       CHECK(!load(img, sizeof img, &entry));   /* magic */
    build(img, 1); img[4] = 2;                       CHECK(!load(img, sizeof img, &entry));   /* 64-bit */
    build(img, 1); img[5] = 2;                       CHECK(!load(img, sizeof img, &entry));   /* big endian */
    build(img, 1); w16(img + 18, 62);                CHECK(!load(img, sizeof img, &entry));   /* x86-64 */
    build(img, 1); w16(img + 16, 3);                 CHECK(!load(img, sizeof img, &entry));   /* ET_DYN */
    build(img, 1);                                   CHECK(!load(img, 20, &entry));           /* truncated ehdr */
    build(img, 1);                                   CHECK(!load(img, 0, &entry));
    build(img, 1);                                   CHECK(!load(img, OFF_PH + 40, &entry));  /* truncated phdrs */
    build(img, 1); w16(img + 44, 0xFFFF);            CHECK(!load(img, sizeof img, &entry));   /* phnum huge */
    build(img, 1); w32(img + 28, 0xFFFFFFF0u);       CHECK(!load(img, sizeof img, &entry));   /* phoff wraps */
    build(img, 1); w16(img + 42, 20);                CHECK(!load(img, sizeof img, &entry));   /* phentsize */

    build(img, 1); w32(img + OFF_PH + 8, MAIN_RAM_BASE - 4);   /* below RAM */
    CHECK(!load(img, sizeof img, &entry));
    build(img, 1); w32(img + OFF_PH + 8, MAIN_RAM_BASE + MAIN_RAM_SIZE - 8);   /* memsz 16 crosses the end */
    CHECK(!load(img, sizeof img, &entry));
    build(img, 1); w32(img + OFF_PH + 8, 0xFFFFFFF8u);         /* vaddr + memsz wraps 32 bits */
    CHECK(!load(img, sizeof img, &entry));
    build(img, 1); w32(img + OFF_PH + 8, DUT_RAM_BASE);        /* other RAM is not a legal target */
    CHECK(!load(img, sizeof img, &entry));
    build(img, 1); w32(img + OFF_PH + 4, IMG_LEN - 4);         /* 8 file bytes run past EOF */
    CHECK(!load(img, sizeof img, &entry));
    build(img, 1); w32(img + OFF_PH + 4, 0xFFFFFFFCu);         /* offset + filesz wraps */
    CHECK(!load(img, sizeof img, &entry));
    build(img, 1); w32(img + OFF_PH + 16, 32);                 /* filesz > memsz */
    CHECK(!load(img, sizeof img, &entry));
}

/* Real riscv-tests ELF from `make riscv-tests`; skipped (not failed) if it was never built. */
static void test_real_elf(void)
{
    char err[128];
    size_t len;
    uint8_t *img = elf_read_file("build/isa/rv32ui-p-add", &len, err, sizeof err);
    if (!img) {
        fprintf(stderr, "skip real ELF: %s\n", err);
        return;
    }
    uint32_t entry = 0, tohost = 0;
    CHECK(load(img, len, &entry));
    CHECK_EQ_U32(entry, 0x80000000u);
    CHECK(elf_find_symbol(img, len, "tohost", &tohost));
    CHECK(tohost >= MAIN_RAM_BASE && tohost < MAIN_RAM_BASE + MAIN_RAM_SIZE);
    free(img);
}

int main(void)
{
    test_valid();
    test_zero_fill_overwrites();
    test_symbols();
    test_rejects();
    test_real_elf();
    TEST_MAIN_RETURN();
}
