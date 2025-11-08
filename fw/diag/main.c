/* Runs every registered diagnostic, reports each, and exits through SYSCON with the failure mask. */
#include "diag.h"
#include "ram_tests.h"

typedef struct { DiagId id; DiagResult (*run)(void); } DiagTest;

/* Milestone 16 appends the sensor tests here. */
static const DiagTest TESTS[] = {
    { DIAG_DATA_BUS,       ram_test_data_bus },
    { DIAG_ADDR_BUS,       ram_test_addr_bus },
    { DIAG_MARCH_C_MINUS,  ram_test_march_c_minus },
};

int main(void)
{
    for (unsigned i = 0; i < sizeof TESTS / sizeof TESTS[0]; i++)
        diag_report(TESTS[i].id, TESTS[i].run());
    diag_finish();
}
