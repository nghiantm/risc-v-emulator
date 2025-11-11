#ifndef FW_SENSOR_TESTS_H
#define FW_SENSOR_TESTS_H

#include "diag.h"

/* Each test starts its own conversions and drains any stale READY first. */
DiagResult sensor_test_id(void);
DiagResult sensor_test_ready(void);
DiagResult sensor_test_range(void);
DiagResult sensor_test_variation(void);
DiagResult sensor_test_irq(void);

#endif
