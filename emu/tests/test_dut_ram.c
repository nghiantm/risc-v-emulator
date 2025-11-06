/* DUT RAM: baseline RAM behaviour, each of the five faults in isolation, combination, main RAM untouched. */
#include "machine.h"
#include "memory_map.h"
#include "test_util.h"

static void setup(Machine *m, const char *const *specs, size_t n)
{
    CHECK(machine_init(m));
    for (size_t i = 0; i < n; i++) {
        char err[128];
        CHECK(fault_parse(specs[i], &m->faults.items[i], err, sizeof err));
    }
    m->faults.count = n;
}

static uint32_t rd(Machine *m, uint32_t addr)
{
    uint32_t v = 0xDEADBEEFu;
    CHECK(bus_read(&m->bus, addr, 4, &v));
    return v;
}

#define WR(m, a, v) CHECK(bus_write(&(m)->bus, (a), 4, (v)))
#define ONE(spec) ((const char *const[]){ spec })

static void test_baseline(void)
{
    Machine m;
    setup(&m, NULL, 0);
    CHECK_EQ_U32(rd(&m, DUT_RAM_BASE), 0);
    WR(&m, DUT_RAM_BASE, 0x11223344u);
    WR(&m, DUT_RAM_BASE + DUT_RAM_SIZE - 4, 0xCAFEBABEu);
    CHECK_EQ_U32(rd(&m, DUT_RAM_BASE), 0x11223344u);
    CHECK_EQ_U32(rd(&m, DUT_RAM_BASE + DUT_RAM_SIZE - 4), 0xCAFEBABEu);
    uint32_t v;
    CHECK(bus_read(&m.bus, DUT_RAM_BASE + 1, 1, &v));  CHECK_EQ_U32(v, 0x33);
    CHECK(bus_read(&m.bus, DUT_RAM_BASE + 2, 2, &v));  CHECK_EQ_U32(v, 0x1122);
    CHECK(!bus_read(&m.bus, DUT_RAM_BASE + DUT_RAM_SIZE, 4, &v));
    machine_free(&m);
}

static void test_data_stuck(void)
{
    Machine m;
    setup(&m, ONE("ram_data_stuck0:bit=5"), 1);
    for (uint32_t a = 0; a < 0x100; a += 4) {                    /* every address, not just one */
        WR(&m, DUT_RAM_BASE + a, 0xFFFFFFFFu);
        CHECK_EQ_U32(rd(&m, DUT_RAM_BASE + a), 0xFFFFFFDFu);
    }
    uint32_t v;
    CHECK(bus_read(&m.bus, DUT_RAM_BASE, 1, &v));  CHECK_EQ_U32(v, 0xDF);   /* sub-word reads see it too */
    CHECK(bus_read(&m.bus, DUT_RAM_BASE + 1, 1, &v));  CHECK_EQ_U32(v, 0xFF);
    machine_free(&m);

    setup(&m, ONE("ram_data_stuck1:bit=9"), 1);
    WR(&m, DUT_RAM_BASE + 0x40, 0);
    CHECK_EQ_U32(rd(&m, DUT_RAM_BASE + 0x40), 0x200u);
    machine_free(&m);
}

static void test_addr_stuck(void)
{
    Machine m;
    setup(&m, ONE("ram_addr_stuck:line=10"), 1);
    WR(&m, DUT_RAM_BASE + 0x400, 0xAAAAAAAAu);                   /* offset with bit 10 set ... */
    WR(&m, DUT_RAM_BASE + 0x000, 0x55555555u);                   /* ... aliases offset with it clear */
    CHECK_EQ_U32(rd(&m, DUT_RAM_BASE + 0x400), 0x55555555u);
    CHECK_EQ_U32(rd(&m, DUT_RAM_BASE + 0x000), 0x55555555u);
    WR(&m, DUT_RAM_BASE + 0x404, 0x12345678u);                   /* other lines unaffected */
    CHECK_EQ_U32(rd(&m, DUT_RAM_BASE + 0x004), 0x12345678u);
    CHECK_EQ_U32(rd(&m, DUT_RAM_BASE + 0x008), 0);
    machine_free(&m);
}

static void test_bitflip(void)
{
    Machine m;
    setup(&m, ONE("ram_bitflip:addr=0x90003A40,bit=3"), 1);
    WR(&m, 0x90003A40u, 0);
    CHECK_EQ_U32(rd(&m, 0x90003A40u), 8);                        /* only that bit flips */
    WR(&m, 0x90003A44u, 0);
    WR(&m, 0x90003A3Cu, 0);
    CHECK_EQ_U32(rd(&m, 0x90003A44u), 0);                        /* neighbours clean */
    CHECK_EQ_U32(rd(&m, 0x90003A3Cu), 0);
    uint32_t v;
    CHECK(bus_read(&m.bus, 0x90003A40u, 1, &v));  CHECK_EQ_U32(v, 8);
    machine_free(&m);
}

static void test_drop_writes(void)
{
    Machine m;
    setup(&m, ONE("ram_drop_writes:every=3"), 1);
    for (uint32_t i = 1; i <= 9; i++)
        WR(&m, DUT_RAM_BASE + 4 * i, i);
    for (uint32_t i = 1; i <= 9; i++)
        CHECK_EQ_U32(rd(&m, DUT_RAM_BASE + 4 * i), i % 3 == 0 ? 0 : i);   /* writes 3, 6, 9 lost */
    machine_free(&m);

    setup(&m, ONE("ram_drop_writes:every=1"), 1);
    WR(&m, DUT_RAM_BASE, 7);
    CHECK_EQ_U32(rd(&m, DUT_RAM_BASE), 0);                       /* every write dropped */
    machine_free(&m);
}

static void test_main_ram_unaffected_and_combination(void)
{
    const char *all[] = { "ram_data_stuck0:bit=0", "ram_data_stuck1:bit=31", "ram_addr_stuck:line=2",
                          "ram_drop_writes:every=1" };
    Machine m;
    setup(&m, all, 4);
    WR(&m, MAIN_RAM_BASE + 0x10, 0xFFFFFFFFu);
    WR(&m, MAIN_RAM_BASE + 0x14, 0);
    CHECK_EQ_U32(rd(&m, MAIN_RAM_BASE + 0x10), 0xFFFFFFFFu);
    CHECK_EQ_U32(rd(&m, MAIN_RAM_BASE + 0x14), 0);
    machine_free(&m);

    const char *two[] = { "ram_data_stuck0:bit=0", "ram_data_stuck1:bit=31" };
    setup(&m, two, 2);                                           /* faults combine */
    WR(&m, DUT_RAM_BASE, 0x00000001u);
    CHECK_EQ_U32(rd(&m, DUT_RAM_BASE), 0x80000000u);
    machine_free(&m);
}

static void test_invalid_parameters(void)
{
    Fault f;
    char err[128];
    CHECK(!fault_parse("ram_bitflip:addr=0x80000000,bit=1", &f, err, sizeof err));
    CHECK(!fault_parse("ram_bitflip:addr=0x90010000,bit=1", &f, err, sizeof err));
    CHECK(!fault_parse("ram_addr_stuck:line=16", &f, err, sizeof err));
    CHECK(!fault_parse("ram_data_stuck0:bit=32", &f, err, sizeof err));
    CHECK(!fault_parse("ram_drop_writes:every=0", &f, err, sizeof err));
}

int main(void)
{
    test_baseline();
    test_data_stuck();
    test_addr_stuck();
    test_bitflip();
    test_drop_writes();
    test_main_ram_unaffected_and_combination();
    test_invalid_parameters();
    TEST_MAIN_RETURN();
}
