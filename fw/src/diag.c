#include "diag.h"
#include "syscon.h"
#include "memory_map.h"
#include "uart.h"

static const char *const NAMES[DIAG_COUNT] = {
    "data_bus", "addr_bus", "march_c_minus",
    "sensor_id", "sensor_ready", "sensor_range", "sensor_variation", "sensor_irq",
};

static uint32_t passed, failed, fail_mask;

void diag_report(DiagId id, DiagResult r)
{
    uart_puts("DIAG ");
    uart_puts(NAMES[id]);
    if (r.pass) {
        uart_puts(" PASS\n");
        passed++;
        return;
    }
    uart_puts(" FAIL ");
    uart_puts(r.detail);
    uart_putc('\n');
    failed++;
    fail_mask |= 1u << id;
}

/* The summary writes the mask without leading zeros ("0x0", "0x6"); uart_put_hex32 always pads to 8. */
static void put_hex_short(uint32_t v)
{
    uart_puts("0x");
    int shift = 28;
    while (shift > 0 && ((v >> shift) & 0xFu) == 0)
        shift -= 4;
    for (; shift >= 0; shift -= 4)
        uart_putc("0123456789abcdef"[(v >> shift) & 0xFu]);
}

void diag_finish(void)
{
    uart_puts("DIAG SUMMARY passed=");
    uart_put_dec(passed);
    uart_puts(" failed=");
    uart_put_dec(failed);
    uart_puts(" mask=");
    put_hex_short(fail_mask);
    uart_putc('\n');
    syscon_write(fail_mask ? (fail_mask << 16) | SYSCON_FAIL_TAG : SYSCON_PASS);
}

static char detail[96];
static unsigned detail_len;

void diag_detail_begin(void)
{
    detail_len = 0;
    detail[0] = '\0';
}

static void detail_putc(char c)
{
    if (detail_len + 1 < sizeof detail) {
        detail[detail_len++] = c;
        detail[detail_len] = '\0';
    }
}

void diag_detail_str(const char *s)
{
    while (*s)
        detail_putc(*s++);
}

void diag_detail_hex(uint32_t v)
{
    diag_detail_str("0x");
    for (int shift = 28; shift >= 0; shift -= 4)
        detail_putc("0123456789abcdef"[(v >> shift) & 0xFu]);
}

void diag_detail_dec(uint32_t v)
{
    char digits[10];
    int n = 0;
    do {
        digits[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
        detail_putc(digits[--n]);
}

const char *diag_detail_end(void)
{
    return detail;
}
