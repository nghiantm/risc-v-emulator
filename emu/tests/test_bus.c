#include <stdio.h>

#include "bus.h"
#include "machine.h"
#include "memory_map.h"
#include "ram.h"
#include "syscon.h"
#include "test_util.h"
#include "uart.h"

#define A_BASE 0x1000u
#define A_SIZE 0x100u
#define B_BASE (A_BASE + A_SIZE)     /* directly adjacent to A */

static Bus bus;
static Ram ram_a, ram_b;

static void test_ram(void)
{
    uint32_t v;
    CHECK(bus_write(&bus, A_BASE, 4, 0xAABBCCDDu));
    CHECK(bus_read(&bus, A_BASE, 4, &v));      CHECK_EQ_U32(v, 0xAABBCCDDu);
    CHECK(bus_read(&bus, A_BASE, 1, &v));      CHECK_EQ_U32(v, 0xDD);           /* little endian */
    CHECK(bus_read(&bus, A_BASE + 3, 1, &v));  CHECK_EQ_U32(v, 0xAA);
    CHECK(bus_read(&bus, A_BASE + 2, 2, &v));  CHECK_EQ_U32(v, 0xAABB);
    CHECK(bus_write(&bus, A_BASE + 1, 1, 0x11));
    CHECK(bus_read(&bus, A_BASE, 4, &v));      CHECK_EQ_U32(v, 0xAABB11DDu);
    CHECK(bus_write(&bus, A_BASE + 2, 2, 0x1234));
    CHECK(bus_read(&bus, A_BASE, 4, &v));      CHECK_EQ_U32(v, 0x123411DDu);
}

static void test_misaligned_ram(void)
{
    uint32_t v;
    CHECK(bus_write(&bus, A_BASE + 0x10, 4, 0x04030201u));
    CHECK(bus_write(&bus, A_BASE + 0x14, 4, 0x08070605u));
    CHECK(bus_read(&bus, A_BASE + 0x12, 4, &v));   CHECK_EQ_U32(v, 0x06050403u);
    CHECK(bus_read(&bus, A_BASE + 0x13, 2, &v));   CHECK_EQ_U32(v, 0x0504);
    CHECK(bus_write(&bus, A_BASE + 0x13, 4, 0xDEADBEEFu));
    CHECK(bus_read(&bus, A_BASE + 0x10, 4, &v));   CHECK_EQ_U32(v, 0xEF030201u);
    CHECK(bus_read(&bus, A_BASE + 0x14, 4, &v));   CHECK_EQ_U32(v, 0x08DEADBEu);
}

static void test_faults(void)
{
    uint32_t v = 0x77;
    CHECK(!bus_read(&bus, 0, 4, &v));
    CHECK(!bus_write(&bus, 0, 4, 1));
    CHECK(!bus_read(&bus, 0x5000, 1, &v));
    CHECK(!bus_read(&bus, A_BASE, 3, &v));            /* bad size */

    /* Straddles the end of the last device: no partial write. */
    uint32_t end = B_BASE + A_SIZE;
    CHECK(bus_write(&bus, end - 4, 4, 0x11223344u));
    CHECK(!bus_write(&bus, end - 2, 4, 0xFFFFFFFFu));
    CHECK(!bus_read(&bus, end - 2, 4, &v));
    CHECK(bus_read(&bus, end - 4, 4, &v));            CHECK_EQ_U32(v, 0x11223344u);
    CHECK(!bus_write(&bus, end - 1, 2, 0));
    CHECK(bus_write(&bus, end - 1, 1, 0x99));         /* last byte itself is fine */

    /* Straddles two adjacent devices: false, neither is written. */
    CHECK(bus_write(&bus, B_BASE - 4, 4, 0x01020304u));
    CHECK(bus_write(&bus, B_BASE, 4, 0x05060708u));
    CHECK(!bus_write(&bus, B_BASE - 2, 4, 0xFFFFFFFFu));
    CHECK(bus_read(&bus, B_BASE - 4, 4, &v));         CHECK_EQ_U32(v, 0x01020304u);
    CHECK(bus_read(&bus, B_BASE, 4, &v));             CHECK_EQ_U32(v, 0x05060708u);
    CHECK(!bus_read(&bus, B_BASE - 2, 4, &v));
}

static void test_syscon(void)
{
    ExitRequest req = {0};
    Syscon sc;
    Bus b;
    uint32_t v = 5;
    syscon_init(&sc, &req);
    bus_init(&b);
    bus_add(&b, &sc.dev);

    CHECK(bus_read(&b, SYSCON_BASE, 4, &v));      CHECK_EQ_U32(v, 0);
    CHECK(bus_write(&b, SYSCON_BASE, 4, 0x1234)); /* ignored */
    CHECK(req.kind == EXIT_NONE);
    CHECK(!bus_write(&b, SYSCON_BASE, 2, SYSCON_PASS));      /* MMIO: 32-bit only */
    CHECK(!bus_write(&b, SYSCON_BASE + 2, 4, SYSCON_PASS));  /* misaligned */
    CHECK(req.kind == EXIT_NONE);
    CHECK(bus_write(&b, SYSCON_BASE, 4, (7u << 16) | SYSCON_FAIL_TAG));
    CHECK(req.kind == EXIT_FAIL);  CHECK_EQ_U32(req.code, 7);
    CHECK(bus_write(&b, SYSCON_BASE, 4, SYSCON_PASS));       /* first request wins */
    CHECK(req.kind == EXIT_FAIL);

    ExitRequest req2 = {0};
    syscon_init(&sc, &req2);
    CHECK(bus_write(&b, SYSCON_BASE, 4, SYSCON_PASS));
    CHECK(req2.kind == EXIT_PASS);  CHECK_EQ_U32(req2.code, 0);
    CHECK(req2.source != NULL && req2.source[0] == 's');
}

static void test_uart(void)
{
    FILE *f = tmpfile();
    Uart u;
    Bus b;
    uint32_t v = 0;
    uart_init(&u, f);
    bus_init(&b);
    bus_add(&b, &u.dev);

    CHECK(bus_read(&b, UART_BASE + UART_STATUS, 4, &v));  CHECK_EQ_U32(v, UART_STATUS_TXRDY);
    CHECK(bus_write(&b, UART_BASE + UART_TX, 4, 'h' | 0x100));   /* low byte only */
    CHECK(bus_write(&b, UART_BASE + UART_TX, 1, 'i'));
    CHECK(!bus_write(&b, UART_BASE + UART_STATUS, 4, 0));
    fflush(f);
    rewind(f);
    CHECK(fgetc(f) == 'h');
    CHECK(fgetc(f) == 'i');
    CHECK(fgetc(f) == EOF);
    fclose(f);
}

static void test_tohost(void)
{
    Bus *b = &bus;
    uint32_t v;
    b->tohost_enabled = true;
    b->tohost_addr = A_BASE + 0x40;

    CHECK(bus_write(b, A_BASE + 0x40, 1, 1));          /* wrong size: no latch */
    CHECK(!b->tohost_written);
    CHECK(bus_write(b, A_BASE + 0x44, 4, 1));          /* wrong address */
    CHECK(!b->tohost_written);
    CHECK(bus_write(b, A_BASE + 0x40, 4, 0x25));
    CHECK(b->tohost_written);  CHECK_EQ_U32(b->tohost_value, 0x25);
    CHECK(bus_write(b, A_BASE + 0x40, 4, 1));          /* later stores don't re-latch */
    CHECK_EQ_U32(b->tohost_value, 0x25);
    CHECK(bus_read(b, A_BASE + 0x40, 4, &v));          /* store still reached RAM */
    CHECK_EQ_U32(v, 1);
}

int main(void)
{
    bus_init(&bus);
    CHECK(ram_init(&ram_a, "a", A_BASE, A_SIZE));
    CHECK(ram_init(&ram_b, "b", B_BASE, A_SIZE));
    CHECK(bus_add(&bus, &ram_a.dev));
    CHECK(bus_add(&bus, &ram_b.dev));

    test_ram();
    test_misaligned_ram();
    test_faults();
    test_syscon();
    test_uart();
    test_tohost();

    ram_free(&ram_a);
    ram_free(&ram_b);
    TEST_MAIN_RETURN();
}
