#include "memory_map.h"
#include "syscon.h"

static bool syscon_read(Device *d, uint32_t offset, unsigned size, uint32_t *out)
{
    (void)d;
    if (offset != 0 || size != 4)
        return false;
    *out = 0;
    return true;
}

static bool syscon_write(Device *d, uint32_t offset, unsigned size, uint32_t value)
{
    if (offset != 0 || size != 4)
        return false;
    ExitRequest *req = ((Syscon *)d->state)->req;
    if (value == SYSCON_PASS)
        exit_request(req, EXIT_PASS, 0, "syscon");
    else if ((value & 0xFFFFu) == SYSCON_FAIL_TAG)
        exit_request(req, EXIT_FAIL, value >> 16, "syscon");
    /* any other value is ignored */
    return true;
}

void syscon_init(Syscon *s, ExitRequest *req)
{
    s->req = req;
    s->dev = (Device){ .name = "syscon", .base = SYSCON_BASE, .size = SYSCON_SIZE,
                       .read = syscon_read, .write = syscon_write, .state = s };
}
