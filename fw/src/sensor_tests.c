#include "sensor_tests.h"
#include "memory_map.h"
#include "sensor.h"
#include "trap.h"

#define SAMPLES    8u
#define MAX_POLLS  20000u   /* far above SENSOR_LATENCY_INSTS: one poll is several instructions */
#define IRQ_WAITS  20000u

/* Reading DATA clears READY, so this drops a result left by an earlier test. */
static void drain(void) { (void)sensor_read(); }

static bool take_sample(int32_t *out)
{
    drain();
    sensor_start(false);
    if (!sensor_wait_ready(MAX_POLLS))
        return false;
    *out = sensor_read();
    return true;
}

DiagResult sensor_test_id(void)
{
    uint32_t id = sensor_id();
    if (id == SENSOR_ID_VALUE)
        return diag_pass();
    diag_detail_begin();
    diag_detail_str("read ");
    diag_detail_hex(id);
    diag_detail_str(" expected ");
    diag_detail_hex(SENSOR_ID_VALUE);
    return diag_fail(diag_detail_end());
}

DiagResult sensor_test_ready(void)
{
    drain();
    sensor_start(false);
    if (!sensor_wait_ready(MAX_POLLS))
        return diag_fail("timeout");
    drain();
    return diag_pass();
}

DiagResult sensor_test_range(void)
{
    for (unsigned i = 0; i < SAMPLES; i++) {
        int32_t s;
        if (!take_sample(&s))
            return diag_fail("timeout");
        if (s < SENSOR_TEMP_MIN_CC || s > SENSOR_TEMP_MAX_CC) {
            diag_detail_begin();
            diag_detail_str("sample ");
            if (s < 0) {
                diag_detail_str("-");
                diag_detail_dec((uint32_t)-s);
            } else {
                diag_detail_dec((uint32_t)s);
            }
            diag_detail_str(" out of range");
            return diag_fail(diag_detail_end());
        }
    }
    return diag_pass();
}

DiagResult sensor_test_variation(void)
{
    int32_t first = 0;
    for (unsigned i = 0; i < SAMPLES; i++) {
        int32_t s;
        if (!take_sample(&s))
            return diag_fail("timeout");
        if (i == 0)
            first = s;
        else if (s != first)
            return diag_pass();
    }
    diag_detail_begin();
    diag_detail_str("all ");
    diag_detail_dec(SAMPLES);
    diag_detail_str(" samples identical");
    return diag_fail(diag_detail_end());
}

static volatile uint32_t irq_seen;

static void irq_handler(uint32_t cause, uint32_t epc)
{
    (void)epc;
    if (cause == (0x80000000u | 11u)) {   /* machine external interrupt */
        drain();                          /* clears READY, which deasserts the level-sensitive line */
        irq_seen++;
    }
}

DiagResult sensor_test_irq(void)
{
    irq_seen = 0;
    drain();
    trap_install(irq_handler);
    irq_enable_external();
    irq_global_enable();
    sensor_start(true);
    for (uint32_t i = 0; i < IRQ_WAITS && !irq_seen; i++)
        __asm__ volatile ("" ::: "memory");
    irq_global_disable();
    irq_disable_external();
    drain();
    return irq_seen ? diag_pass() : diag_fail("no interrupt");
}
