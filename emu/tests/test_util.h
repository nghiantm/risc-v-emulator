/* Minimal header-only test helpers: failures are reported and counted, the
 * test keeps running so one run shows every broken check. */
#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdint.h>
#include <stdio.h>

static int test_failure_count;

static inline int test_failures(void) { return test_failure_count; }

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            test_failure_count++; \
        } \
    } while (0)

#define CHECK_EQ_U32(actual, expected) \
    do { \
        uint32_t a_ = (uint32_t)(actual); \
        uint32_t e_ = (uint32_t)(expected); \
        if (a_ != e_) { \
            fprintf(stderr, "%s:%d: CHECK_EQ_U32 failed: %s: got 0x%08x, expected 0x%08x\n", \
                    __FILE__, __LINE__, #actual, (unsigned)a_, (unsigned)e_); \
            test_failure_count++; \
        } \
    } while (0)

#define TEST_MAIN_RETURN() return test_failures() ? 1 : 0

#endif /* TEST_UTIL_H */
