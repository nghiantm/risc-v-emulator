#include "sensor.h"
#include "memory_map.h"

static inline volatile uint32_t *reg(uint32_t off)
{
    return (volatile uint32_t *)(SENSOR_BASE + off);
}

uint32_t sensor_id(void) { return *reg(SENSOR_ID_REG); }

void sensor_start(bool irq_en)
{
    *reg(SENSOR_CTRL_REG) = SENSOR_CTRL_START | (irq_en ? SENSOR_CTRL_IRQ_EN : 0u);
}

bool sensor_wait_ready(uint32_t max_polls)
{
    while (max_polls--)
        if (*reg(SENSOR_STATUS_REG) & SENSOR_STATUS_READY)
            return true;
    return false;
}

int32_t sensor_read(void) { return (int32_t)*reg(SENSOR_DATA_REG); }

uint32_t sensor_sample_count(void) { return *reg(SENSOR_COUNT_REG); }
