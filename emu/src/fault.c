#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fault.h"
#include "memory_map.h"

enum { KEY_BIT = 1, KEY_LINE = 2, KEY_ADDR = 4, KEY_EVERY = 8 };

static const struct {
    const char *name;
    FaultKind   kind;
    unsigned    keys;      /* every listed key is required; no others are allowed */
} FAULT_TABLE[] = {
    { "ram_data_stuck0",   FAULT_RAM_DATA_STUCK0,   KEY_BIT },
    { "ram_data_stuck1",   FAULT_RAM_DATA_STUCK1,   KEY_BIT },
    { "ram_addr_stuck",    FAULT_RAM_ADDR_STUCK,    KEY_LINE },
    { "ram_bitflip",       FAULT_RAM_BITFLIP,       KEY_ADDR | KEY_BIT },
    { "ram_drop_writes",   FAULT_RAM_DROP_WRITES,   KEY_EVERY },
    { "sensor_bad_id",     FAULT_SENSOR_BAD_ID,     0 },
    { "sensor_stuck",      FAULT_SENSOR_STUCK,      0 },
    { "sensor_out_of_range", FAULT_SENSOR_OUT_OF_RANGE, 0 },
    { "sensor_never_ready", FAULT_SENSOR_NEVER_READY, 0 },
    { "sensor_no_irq",     FAULT_SENSOR_NO_IRQ,     0 },
};

static bool fail(char *err, size_t errlen, const char *fmt, const char *a)
{
    snprintf(err, errlen, fmt, a);
    return false;
}

/* Decimal or 0x hex, digits only (no sign, no octal, no trailing junk), must fit 32 bits. */
static bool parse_u32(const char *s, uint32_t *out)
{
    if (!isdigit((unsigned char)*s))
        return false;
    char *end;
    int base = (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) ? 16 : 10;
    unsigned long long v = strtoull(s, &end, base);
    if (*end != '\0' || v > UINT32_MAX)
        return false;
    *out = (uint32_t)v;
    return true;
}

bool fault_parse(const char *spec, Fault *out, char *err, size_t errlen)
{
    char buf[128];
    if (strlen(spec) >= sizeof buf)
        return fail(err, errlen, "fault spec too long: %s", spec);
    strcpy(buf, spec);

    char *params = strchr(buf, ':');
    if (params)
        *params++ = '\0';

    size_t i = 0;
    while (i < sizeof FAULT_TABLE / sizeof FAULT_TABLE[0] && strcmp(buf, FAULT_TABLE[i].name) != 0)
        i++;
    if (i == sizeof FAULT_TABLE / sizeof FAULT_TABLE[0])
        return fail(err, errlen, "unknown fault: %s", buf);

    Fault f = { .kind = FAULT_TABLE[i].kind };
    unsigned seen = 0;

    /* strtok would also swallow empty items ("a=1,,b=2"); walk by hand so they are errors. */
    while (params) {
        char *comma = strchr(params, ',');
        if (comma)
            *comma++ = '\0';
        char *eq = strchr(params, '=');
        if (!eq)
            return fail(err, errlen, "bad fault parameter (want key=val): %s", params);
        *eq++ = '\0';

        unsigned key;
        uint32_t *field;
        if      (strcmp(params, "bit")   == 0) { key = KEY_BIT;   field = &f.bit; }
        else if (strcmp(params, "line")  == 0) { key = KEY_LINE;  field = &f.line; }
        else if (strcmp(params, "addr")  == 0) { key = KEY_ADDR;  field = &f.addr; }
        else if (strcmp(params, "every") == 0) { key = KEY_EVERY; field = &f.every; }
        else return fail(err, errlen, "unknown fault parameter: %s", params);

        if (!(FAULT_TABLE[i].keys & key))
            return fail(err, errlen, "parameter not valid for this fault: %s", params);
        if (seen & key)
            return fail(err, errlen, "duplicate fault parameter: %s", params);
        if (!parse_u32(eq, field))
            return fail(err, errlen, "bad number for fault parameter: %s", params);
        seen |= key;
        params = comma;
    }

    if (seen != FAULT_TABLE[i].keys)
        return fail(err, errlen, "missing fault parameter for: %s", FAULT_TABLE[i].name);

    if ((seen & KEY_BIT) && f.bit > 31)
        return fail(err, errlen, "%s: bit must be 0..31", FAULT_TABLE[i].name);
    if ((seen & KEY_LINE) && (f.line < 2 || f.line > 15))
        return fail(err, errlen, "%s: line must be 2..15", FAULT_TABLE[i].name);
    if ((seen & KEY_ADDR) && (f.addr < DUT_RAM_BASE || f.addr - DUT_RAM_BASE >= DUT_RAM_SIZE))
        return fail(err, errlen, "%s: addr must be inside DUT RAM", FAULT_TABLE[i].name);
    if ((seen & KEY_EVERY) && f.every < 1)
        return fail(err, errlen, "%s: every must be >= 1", FAULT_TABLE[i].name);

    *out = f;
    return true;
}

bool fault_set_has(const FaultSet *fs, FaultKind k)
{
    for (size_t i = 0; i < fs->count; i++)
        if (fs->items[i].kind == k)
            return true;
    return false;
}
