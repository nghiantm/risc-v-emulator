/* Sensor registers, sample model, latency, interrupt line, faults, and interrupt priority end to end. */
#include <string.h>

#include "csr.h"
#include "machine.h"
#include "memory_map.h"
#include "sensor.h"
#include "test_util.h"

#define REG(off) (SENSOR_BASE + (off))

static uint32_t rd(Bus *b, uint32_t off)
{
    uint32_t v = 0xDEADBEEFu;
    CHECK(bus_read(b, REG(off), 4, &v));
    return v;
}

static void wr(Bus *b, uint32_t off, uint32_t v)
{
    CHECK(bus_write(b, REG(off), 4, v));
}

static void ticks(Bus *b, unsigned n)
{
    while (n--)
        bus_tick(b);
}

/* Starts one conversion and waits it out; returns the sample read from DATA. */
static int32_t convert(Bus *b)
{
    wr(b, SENSOR_CTRL_REG, SENSOR_CTRL_START);
    ticks(b, SENSOR_LATENCY_INSTS);
    return (int32_t)rd(b, SENSOR_DATA_REG);
}

static void setup(Machine *m, const char *fault)
{
    CHECK(machine_init(m));
    if (fault) {
        char err[128];
        CHECK(fault_parse(fault, &m->faults.items[0], err, sizeof err));
        m->faults.count = 1;
    }
}

static void test_id_and_idle(void)
{
    Machine m;
    setup(&m, NULL);
    CHECK_EQ_U32(rd(&m.bus, SENSOR_ID_REG), SENSOR_ID_VALUE);
    CHECK_EQ_U32(rd(&m.bus, SENSOR_STATUS_REG), 0);
    CHECK_EQ_U32(rd(&m.bus, SENSOR_COUNT_REG), 0);
    CHECK_EQ_U32(rd(&m.bus, SENSOR_DATA_REG), 0);          /* nothing latched yet */
    ticks(&m.bus, 5000);
    CHECK_EQ_U32(rd(&m.bus, SENSOR_STATUS_REG), 0);        /* no READY without START */
    machine_free(&m);
}

static void test_latency_and_ready(void)
{
    Machine m;
    setup(&m, NULL);
    wr(&m.bus, SENSOR_CTRL_REG, SENSOR_CTRL_START);
    CHECK_EQ_U32(rd(&m.bus, SENSOR_CTRL_REG), 0);          /* START never reads back */
    ticks(&m.bus, SENSOR_LATENCY_INSTS - 1);
    CHECK_EQ_U32(rd(&m.bus, SENSOR_STATUS_REG), 0);
    ticks(&m.bus, 1);
    CHECK_EQ_U32(rd(&m.bus, SENSOR_STATUS_REG), SENSOR_STATUS_READY);
    CHECK_EQ_U32(rd(&m.bus, SENSOR_COUNT_REG), 1);
    rd(&m.bus, SENSOR_DATA_REG);
    CHECK_EQ_U32(rd(&m.bus, SENSOR_STATUS_REG), 0);        /* DATA read clears READY */
    CHECK_EQ_U32(rd(&m.bus, SENSOR_COUNT_REG), 1);

    /* START while busy is ignored: the second START must not restart the countdown */
    wr(&m.bus, SENSOR_CTRL_REG, SENSOR_CTRL_START);
    ticks(&m.bus, 1000);
    wr(&m.bus, SENSOR_CTRL_REG, SENSOR_CTRL_START);
    ticks(&m.bus, SENSOR_LATENCY_INSTS - 1000);
    CHECK_EQ_U32(rd(&m.bus, SENSOR_STATUS_REG), SENSOR_STATUS_READY);
    machine_free(&m);
}

static void test_sample_model(void)
{
    /* xorshift32 from seed 0x1234ABCD gives 1860453643, 787452681, 1297446030, 2606524652, 3732773452.
     * sample n = 2500 + 50*n + (prng % 41) - 20, so: 2485, 2569, 2585, 2630, 2718. */
    static const int32_t want[5] = { 2485, 2569, 2585, 2630, 2718 };
    Machine m;
    setup(&m, NULL);
    int32_t prev = 0;
    for (uint32_t n = 0; n < 250; n++) {                   /* 250 covers two sawtooth wraps */
        int32_t s = convert(&m.bus);
        if (n < 5)
            CHECK_EQ_U32(s, want[n]);
        CHECK(s >= SENSOR_TEMP_MIN_CC && s <= SENSOR_TEMP_MAX_CC);
        if (n > 0 && n % 100 != 0)                         /* a wrap is allowed to drop */
            CHECK(s - prev >= 10);
        prev = s;
        CHECK_EQ_U32(rd(&m.bus, SENSOR_COUNT_REG), n + 1);
    }
    machine_free(&m);
}

static void test_irq_line(void)
{
    Machine m;
    setup(&m, NULL);
    wr(&m.bus, SENSOR_CTRL_REG, SENSOR_CTRL_START);        /* IRQ_EN off */
    ticks(&m.bus, SENSOR_LATENCY_INSTS);
    CHECK(!sensor_irq_pending(&m.sensor));                 /* READY alone is not enough */
    wr(&m.bus, SENSOR_CTRL_REG, SENSOR_CTRL_IRQ_EN);
    CHECK(sensor_irq_pending(&m.sensor));                  /* READY && IRQ_EN */
    CHECK_EQ_U32(rd(&m.bus, SENSOR_CTRL_REG), SENSOR_CTRL_IRQ_EN);
    rd(&m.bus, SENSOR_DATA_REG);
    CHECK(!sensor_irq_pending(&m.sensor));                 /* DATA read drops the level */
    machine_free(&m);
}

static void test_access_rules(void)
{
    Machine m;
    uint32_t v;
    setup(&m, NULL);
    CHECK(!bus_read(&m.bus, REG(SENSOR_ID_REG), 2, &v));
    CHECK(!bus_read(&m.bus, REG(SENSOR_ID_REG), 1, &v));
    CHECK(!bus_write(&m.bus, REG(SENSOR_CTRL_REG), 1, 0));
    CHECK(!bus_read(&m.bus, REG(0x14), 4, &v));            /* unknown offset */
    CHECK(!bus_write(&m.bus, REG(SENSOR_ID_REG), 4, 0));   /* only CTRL is writable */
    CHECK(!bus_write(&m.bus, REG(SENSOR_DATA_REG), 4, 0));
    machine_free(&m);
}

static void test_faults(void)
{
    Machine m;

    setup(&m, "sensor_bad_id");
    CHECK_EQ_U32(rd(&m.bus, SENSOR_ID_REG), 0xBAD0BAD0u);
    machine_free(&m);

    setup(&m, "sensor_stuck");
    for (int i = 0; i < 5; i++)
        CHECK_EQ_U32(convert(&m.bus), 2500);
    machine_free(&m);

    setup(&m, "sensor_out_of_range");
    CHECK_EQ_U32(convert(&m.bus), 20000);
    CHECK(20000 > SENSOR_TEMP_MAX_CC);
    machine_free(&m);

    setup(&m, "sensor_never_ready");
    wr(&m.bus, SENSOR_CTRL_REG, SENSOR_CTRL_START | SENSOR_CTRL_IRQ_EN);
    ticks(&m.bus, 3 * SENSOR_LATENCY_INSTS);
    CHECK_EQ_U32(rd(&m.bus, SENSOR_STATUS_REG), 0);
    CHECK_EQ_U32(rd(&m.bus, SENSOR_COUNT_REG), 0);
    CHECK(!sensor_irq_pending(&m.sensor));
    machine_free(&m);

    setup(&m, "sensor_no_irq");
    wr(&m.bus, SENSOR_CTRL_REG, SENSOR_CTRL_START | SENSOR_CTRL_IRQ_EN);
    ticks(&m.bus, SENSOR_LATENCY_INSTS);
    CHECK_EQ_U32(rd(&m.bus, SENSOR_STATUS_REG), SENSOR_STATUS_READY);   /* conversion still completes */
    CHECK_EQ_U32(rd(&m.bus, SENSOR_COUNT_REG), 1);
    CHECK(!sensor_irq_pending(&m.sensor));
    machine_free(&m);

    /* healthy sensor with a RAM fault set: sensor behaviour is unchanged */
    setup(&m, "ram_data_stuck0:bit=3");
    CHECK_EQ_U32(rd(&m.bus, SENSOR_ID_REG), SENSOR_ID_VALUE);
    CHECK_EQ_U32(convert(&m.bus), 2485);
    machine_free(&m);
}

/* Timer and sensor both pending with MIE on: the CPU must take the external interrupt first.
 * Assembled from the program: arm mtimecmp=0, start a conversion with IRQ_EN, poll READY,
 * enable MIE, spin; the handler stores mcause in x20 and exits via SYSCON. */
static const uint32_t prog_priority[] = {
    0x00000297, /* auipc t0,0x0 */
    0x04428293, /* addi t0,t0,68 # 80000044 <handler> */
    0x30529073, /* csrw mtvec,t0 */
    0x000012b7, /* lui t0,0x1 */
    0x88028293, /* addi t0,t0,-1920 # mie = MTIE | MEIE */
    0x30429073, /* csrw mie,t0 */
    0x02004337, /* lui t1,0x2004 */
    0x00032023, /* sw zero,0(t1) # mtimecmp lo = 0 */
    0x00032223, /* sw zero,4(t1) # mtimecmp hi = 0: timer pending now */
    0x100013b7, /* lui t2,0x10001 */
    0x00300e13, /* li t3,3 */
    0x01c3a223, /* sw t3,4(t2) # START | IRQ_EN */
    0x0083ae83, /* lw t4,8(t2) # wait: poll STATUS */
    0x001efe93, /* andi t4,t4,1 */
    0xfe0e8ce3, /* beqz t4,wait */
    0x30046073, /* csrsi mstatus,8 */
    0x0000006f, /* j . */
    0x34202a73, /* csrr s4,mcause # handler */
    0x00100f37, /* lui t5,0x100 */
    0x00005fb7, /* lui t6,0x5 */
    0x555f8f93, /* addi t6,t6,1365 */
    0x01ff2023, /* sw t6,0(t5) # SYSCON pass */
};

static void test_external_preempts_timer(void)
{
    Machine m;
    setup(&m, NULL);
    for (size_t i = 0; i < sizeof prog_priority / sizeof prog_priority[0]; i++)
        CHECK(bus_write(&m.bus, MAIN_RAM_BASE + 4 * (uint32_t)i, 4, prog_priority[i]));
    m.cpu.pc = MAIN_RAM_BASE;
    CHECK(machine_run(&m, 20000) == RUN_PASS);
    CHECK(clint_timer_pending(&m.clint));                  /* the timer really was pending too */
    CHECK_EQ_U32(m.cpu.x[20], CAUSE_INT_MEXTERNAL);
    machine_free(&m);
}

int main(void)
{
    test_id_and_idle();
    test_latency_and_ready();
    test_sample_model();
    test_irq_line();
    test_access_rules();
    test_faults();
    test_external_preempts_timer();
    TEST_MAIN_RETURN();
}
