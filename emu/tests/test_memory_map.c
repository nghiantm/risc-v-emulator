#include <stdint.h>

#include "memory_map.h"
#include "test_util.h"

typedef struct { const char *name; uint64_t start, end; } Region;

#define REGION(n) { #n, n##_BASE, (uint64_t)n##_BASE + n##_SIZE }

int main(void)
{
    CHECK_EQ_U32(MAIN_RAM_BASE, 0x80000000u);
    CHECK_EQ_U32(DUT_RAM_BASE, 0x90000000u);
    CHECK_EQ_U32(SENSOR_BASE, 0x10001000u);
    CHECK_EQ_U32(CLINT_BASE + CLINT_MTIME_LO, 0x0200BFF8u);
    CHECK_EQ_U32(SENSOR_ID_VALUE, 0x5E450001u);

    const Region regions[] = {
        REGION(SYSCON), REGION(CLINT), REGION(UART),
        REGION(SENSOR), REGION(MAIN_RAM), REGION(DUT_RAM),
    };
    const int n = (int)(sizeof regions / sizeof regions[0]);
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            int overlap = regions[i].start < regions[j].end && regions[j].start < regions[i].end;
            if (overlap)
                fprintf(stderr, "regions overlap: %s / %s\n", regions[i].name, regions[j].name);
            CHECK(!overlap);
        }
    }

    CHECK(SENSOR_TEMP_MIN_CC < 0);
    TEST_MAIN_RETURN();
}
