#include <stdio.h>
#include <string.h>

#include "fault.h"
#include "memory_map.h"
#include "test_util.h"

static bool parse(const char *spec, Fault *f)
{
    char err[128];
    return fault_parse(spec, f, err, sizeof err);
}

static void test_valid(void)
{
    Fault f;
    CHECK(parse("ram_data_stuck0:bit=5", &f));   CHECK_EQ_U32(f.kind, FAULT_RAM_DATA_STUCK0); CHECK_EQ_U32(f.bit, 5);
    CHECK(parse("ram_data_stuck1:bit=9", &f));   CHECK_EQ_U32(f.kind, FAULT_RAM_DATA_STUCK1); CHECK_EQ_U32(f.bit, 9);
    CHECK(parse("ram_addr_stuck:line=10", &f));  CHECK_EQ_U32(f.kind, FAULT_RAM_ADDR_STUCK);  CHECK_EQ_U32(f.line, 10);
    CHECK(parse("ram_bitflip:addr=0x90003A40,bit=3", &f));
    CHECK_EQ_U32(f.kind, FAULT_RAM_BITFLIP); CHECK_EQ_U32(f.addr, 0x90003A40u); CHECK_EQ_U32(f.bit, 3);
    CHECK(parse("ram_bitflip:bit=3,addr=0x90003A40", &f));            /* key order is free */
    CHECK(parse("ram_drop_writes:every=1000", &f)); CHECK_EQ_U32(f.kind, FAULT_RAM_DROP_WRITES); CHECK_EQ_U32(f.every, 1000);
    CHECK(parse("sensor_bad_id", &f));           CHECK_EQ_U32(f.kind, FAULT_SENSOR_BAD_ID);
    CHECK(parse("sensor_stuck", &f));            CHECK_EQ_U32(f.kind, FAULT_SENSOR_STUCK);
    CHECK(parse("sensor_out_of_range", &f));     CHECK_EQ_U32(f.kind, FAULT_SENSOR_OUT_OF_RANGE);
    CHECK(parse("sensor_never_ready", &f));      CHECK_EQ_U32(f.kind, FAULT_SENSOR_NEVER_READY);
    CHECK(parse("sensor_no_irq", &f));           CHECK_EQ_U32(f.kind, FAULT_SENSOR_NO_IRQ);
}

static void test_bounds(void)
{
    Fault f;
    CHECK(parse("ram_data_stuck0:bit=0", &f));
    CHECK(parse("ram_data_stuck0:bit=31", &f));
    CHECK(!parse("ram_data_stuck0:bit=32", &f));
    CHECK(!parse("ram_addr_stuck:line=1", &f));
    CHECK(parse("ram_addr_stuck:line=2", &f));
    CHECK(parse("ram_addr_stuck:line=15", &f));
    CHECK(!parse("ram_addr_stuck:line=16", &f));
    CHECK(!parse("ram_drop_writes:every=0", &f));
    CHECK(parse("ram_drop_writes:every=1", &f));
    CHECK(parse("ram_bitflip:addr=0x90000000,bit=0", &f));
    CHECK(parse("ram_bitflip:addr=0x9000FFFF,bit=0", &f));
    CHECK(!parse("ram_bitflip:addr=0x90010000,bit=0", &f));          /* one past DUT RAM */
    CHECK(!parse("ram_bitflip:addr=0x8FFFFFFF,bit=0", &f));
    CHECK(!parse("ram_bitflip:addr=0x1FFFFFFFF,bit=0", &f));         /* does not fit 32 bits */
}

static void test_malformed(void)
{
    Fault f;
    CHECK(!parse("", &f));
    CHECK(!parse("bogus", &f));
    CHECK(!parse("ram_data_stuck0", &f));                    /* missing key */
    CHECK(!parse("ram_data_stuck0:", &f));
    CHECK(!parse("ram_data_stuck0:bit", &f));                /* no value */
    CHECK(!parse("ram_data_stuck0:bit=", &f));
    CHECK(!parse("ram_data_stuck0:bit=-1", &f));
    CHECK(!parse("ram_data_stuck0:bit=5x", &f));
    CHECK(!parse("ram_data_stuck0:bit=5,bit=6", &f));        /* duplicate */
    CHECK(!parse("ram_data_stuck0:bit=5,line=3", &f));       /* extra key */
    CHECK(!parse("ram_data_stuck0:foo=5", &f));              /* unknown key */
    CHECK(!parse("ram_data_stuck0:bit=5,,", &f));
    CHECK(!parse("ram_bitflip:addr=0x90000000", &f));        /* missing bit */
    CHECK(!parse("sensor_stuck:bit=1", &f));                 /* parameterless fault takes none */
}

static void test_set_has(void)
{
    FaultSet fs = { 0 };
    CHECK(!fault_set_has(&fs, FAULT_SENSOR_STUCK));
    CHECK(parse("sensor_stuck", &fs.items[fs.count]));
    fs.count++;
    CHECK(fault_set_has(&fs, FAULT_SENSOR_STUCK));
    CHECK(!fault_set_has(&fs, FAULT_SENSOR_BAD_ID));
}

int main(void)
{
    test_valid();
    test_bounds();
    test_malformed();
    test_set_has();
    TEST_MAIN_RETURN();
}
