/* Runs every registered diagnostic, reports each, and exits through SYSCON with the failure mask. */
#include "diag.h"
#include "ram_tests.h"
#include "sensor_tests.h"

typedef struct { DiagId id; DiagResult (*run)(void); } DiagTest;

static const DiagTest TESTS[] = {
    { DIAG_DATA_BUS,       ram_test_data_bus },
    { DIAG_ADDR_BUS,       ram_test_addr_bus },
    { DIAG_MARCH_C_MINUS,  ram_test_march_c_minus },
    { DIAG_SENSOR_ID,        sensor_test_id },
    { DIAG_SENSOR_READY,     sensor_test_ready },
    { DIAG_SENSOR_RANGE,     sensor_test_range },
    { DIAG_SENSOR_VARIATION, sensor_test_variation },
    { DIAG_SENSOR_IRQ,       sensor_test_irq },
};

int main(void)
{
    for (unsigned i = 0; i < sizeof TESTS / sizeof TESTS[0]; i++)
        diag_report(TESTS[i].id, TESTS[i].run());
    diag_finish();
}
