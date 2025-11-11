#ifndef FW_SENSOR_H
#define FW_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

uint32_t sensor_id(void);
/* Starts one conversion; irq_en also enables the sensor's interrupt line. */
void sensor_start(bool irq_en);
/* Polls STATUS.READY at most max_polls times. */
bool sensor_wait_ready(uint32_t max_polls);
/* Reads the latest sample (centi-degrees C). Reading clears READY. */
int32_t sensor_read(void);
uint32_t sensor_sample_count(void);

#endif
