#include <stdio.h>
#include <string.h>

#include "machine.h"
#include "memory_map.h"
#include "trace.h"

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

/* The tohost latch becomes an exit request: 1 means pass, anything else fails with value >> 1. */
static void latch_tohost(Machine *m)
{
    if (!m->bus.tohost_written)
        return;
    if (m->bus.tohost_value == 1)
        exit_request(&m->exit_req, EXIT_PASS, 0, "tohost");
    else
        exit_request(&m->exit_req, EXIT_FAIL, m->bus.tohost_value >> 1, "tohost");
}

RunResult machine_run(Machine *m, uint64_t max_insts)
{
    for (uint64_t n = 0; ; n++) {
        latch_tohost(m);
        if (m->exit_req.kind != EXIT_NONE)
            return m->exit_req.kind == EXIT_PASS ? RUN_PASS : RUN_FAIL;
        if (n == max_insts)
            return RUN_TIMEOUT;

        StepInfo info;
        Trap t = cpu_step(&m->cpu, &m->bus, &info);
        if (t.raised) {
            /* Trap delivery arrives with the CSRs in Milestone 7. */
            snprintf(m->error_buf, sizeof m->error_buf, "unhandled trap cause=0x%x tval=0x%08x pc=0x%08x",
                     (unsigned)t.cause, (unsigned)t.tval, (unsigned)m->cpu.pc);
            m->error = m->error_buf;
            return RUN_ERROR;
        }
        if (m->trace)
            trace_step(stderr, &info);
        bus_tick(&m->bus);
    }
}
