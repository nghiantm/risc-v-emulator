#ifndef SENSOR_H
#define SENSOR_H

#include "bus.h"
#include "fault.h"

typedef struct {
    Device dev;
    uint32_t ctrl;            /* bit1 IRQ_EN stored; bit0 START never stored */
    bool     ready;
    uint32_t sample_count;
    int32_t  data;            /* last latched sample, centi-degrees C */
    uint32_t countdown;       /* instructions until conversion completes; 0 = idle */
    uint32_t prng;            /* xorshift32 state, seeded SENSOR_PRNG_SEED */
    const FaultSet *faults;
} Sensor;

#define SENSOR_PRNG_SEED 0x1234ABCDu

/* faults may be NULL (no faults); it is read on every access, so it can be filled in later. */
void sensor_init(Sensor *s, const FaultSet *faults);
bool sensor_irq_pending(const Sensor *s);   /* ready && (ctrl & IRQ_EN) && !fault no_irq */

#endif /* SENSOR_H */
