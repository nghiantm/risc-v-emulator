#include "memory_map.h"
#include "sensor.h"

#define BAD_ID_VALUE         0xBAD0BAD0u
#define STUCK_VALUE_CC       2500
#define OUT_OF_RANGE_VALUE_CC 20000

static bool fault_on(const Sensor *s, FaultKind k)
{
    return s->faults && fault_set_has(s->faults, k);
}

static uint32_t prng_next(Sensor *s)       /* xorshift32 */
{
    uint32_t x = s->prng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return s->prng = x;
}

/* Sample n (0-based): a sawtooth of 100 steps of 50 centi-degrees plus noise in -20..20.
 * Consecutive samples therefore differ by at least 50 - 40 = 10 inside one period. */
static int32_t next_sample(Sensor *s, uint32_t n)
{
    int32_t noise = (int32_t)(prng_next(s) % 41u) - 20;
    return 2500 + 50 * (int32_t)(n % 100u) + noise;
}

static void complete_conversion(Sensor *s)
{
    /* The PRNG advances even for faulted samples so a fault never shifts later healthy ones. */
    int32_t sample = next_sample(s, s->sample_count);
    if (fault_on(s, FAULT_SENSOR_NEVER_READY))
        return;                                 /* conversion "finishes" but READY never rises */
    if (fault_on(s, FAULT_SENSOR_STUCK))
        sample = STUCK_VALUE_CC;
    else if (fault_on(s, FAULT_SENSOR_OUT_OF_RANGE))
        sample = OUT_OF_RANGE_VALUE_CC;
    s->data = sample;
    s->sample_count++;
    s->ready = true;
}

static bool sensor_read(Device *d, uint32_t offset, unsigned size, uint32_t *out)
{
    Sensor *s = d->state;
    if (size != 4)
        return false;
    switch (offset) {
    case SENSOR_ID_REG:
        *out = fault_on(s, FAULT_SENSOR_BAD_ID) ? BAD_ID_VALUE : SENSOR_ID_VALUE;
        return true;
    case SENSOR_CTRL_REG:
        *out = s->ctrl;
        return true;
    case SENSOR_STATUS_REG:
        *out = s->ready ? SENSOR_STATUS_READY : 0;    /* ERROR bit is reserved: always 0 */
        return true;
    case SENSOR_DATA_REG:
        *out = (uint32_t)s->data;
        s->ready = false;                              /* consuming the sample clears READY (and the IRQ) */
        return true;
    case SENSOR_COUNT_REG:
        *out = s->sample_count;
        return true;
    default:
        return false;
    }
}

static bool sensor_write(Device *d, uint32_t offset, unsigned size, uint32_t value)
{
    Sensor *s = d->state;
    if (size != 4 || offset != SENSOR_CTRL_REG)
        return false;
    s->ctrl = value & SENSOR_CTRL_IRQ_EN;
    if ((value & SENSOR_CTRL_START) && s->countdown == 0)   /* START during a conversion is ignored */
        s->countdown = SENSOR_LATENCY_INSTS;
    return true;
}

static void sensor_tick(Device *d)
{
    Sensor *s = d->state;
    if (s->countdown && --s->countdown == 0)
        complete_conversion(s);
}

void sensor_init(Sensor *s, const FaultSet *faults)
{
    *s = (Sensor){ .prng = SENSOR_PRNG_SEED, .faults = faults };
    s->dev = (Device){ .name = "sensor", .base = SENSOR_BASE, .size = SENSOR_SIZE,
                       .read = sensor_read, .write = sensor_write, .tick = sensor_tick, .state = s };
}

bool sensor_irq_pending(const Sensor *s)
{
    return s->ready && (s->ctrl & SENSOR_CTRL_IRQ_EN) && !fault_on(s, FAULT_SENSOR_NO_IRQ);
}
