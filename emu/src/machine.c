#include <stdio.h>
#include <string.h>

#include "machine.h"
#include "memory_map.h"

bool machine_init(Machine *m)
{
    memset(m, 0, sizeof *m);
    bus_init(&m->bus);
    if (!ram_init(&m->main_ram, "main_ram", MAIN_RAM_BASE, MAIN_RAM_SIZE))
        return false;
    syscon_init(&m->syscon, &m->exit_req);
    uart_init(&m->uart, stdout);
    bus_add(&m->bus, &m->main_ram.dev);
    bus_add(&m->bus, &m->syscon.dev);
    bus_add(&m->bus, &m->uart.dev);
    return true;
}

void machine_free(Machine *m)
{
    ram_free(&m->main_ram);
}

/* Stub until Milestone 5 adds the CPU. */
RunResult machine_run(Machine *m, uint64_t max_insts)
{
    (void)max_insts;
    m->error = "cpu not implemented";
    return RUN_ERROR;
}
