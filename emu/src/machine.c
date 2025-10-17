#include <stdio.h>
#include <string.h>

#include "machine.h"
#include "memory_map.h"
#include "trace.h"
#include "trap.h"

bool machine_init(Machine *m)
{
    memset(m, 0, sizeof *m);
    bus_init(&m->bus);
    if (!ram_init(&m->main_ram, "main_ram", MAIN_RAM_BASE, MAIN_RAM_SIZE))
        return false;
    clint_init(&m->clint);
    syscon_init(&m->syscon, &m->exit_req);
    uart_init(&m->uart, stdout);
    bus_add(&m->bus, &m->main_ram.dev);
    bus_add(&m->bus, &m->syscon.dev);
    bus_add(&m->bus, &m->clint.dev);
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

/* Take a trap or interrupt. false (with m->error set) if no handler is installed. */
static bool deliver(Machine *m, uint32_t cause, uint32_t tval)
{
    if (m->cpu.csr.mtvec == 0) {
        snprintf(m->error_buf, sizeof m->error_buf,
                 "trap with no handler installed (cause=0x%x tval=0x%08x pc=0x%08x)",
                 (unsigned)cause, (unsigned)tval, (unsigned)m->cpu.pc);
        m->error = m->error_buf;
        return false;
    }
    trap_enter(&m->cpu, cause, tval, m->cpu.pc);
    if (m->trace)
        trace_trap(stderr, cause, tval, m->cpu.pc);
    return true;
}

RunResult machine_run(Machine *m, uint64_t max_insts)
{
    for (uint64_t n = 0; ; n++) {
        latch_tohost(m);
        if (m->exit_req.kind != EXIT_NONE)
            return m->exit_req.kind == EXIT_PASS ? RUN_PASS : RUN_FAIL;
        if (n == max_insts)
            return RUN_TIMEOUT;

        /* Interrupts are taken between instructions; the interrupted instruction has not run. */
        m->cpu.mtip = clint_timer_pending(&m->clint);
        m->cpu.meip = false;                          /* sensor line arrives in Milestone 12 */
        uint32_t irq = csr_pending_interrupt(&m->cpu.csr, m->cpu.mtip, m->cpu.meip);
        if (irq) {
            if (!deliver(m, irq, 0))
                return RUN_ERROR;
        } else {
            StepInfo info;
            Trap t = cpu_step(&m->cpu, &m->bus, &info);
            if (t.raised) {
                if (!deliver(m, t.cause, t.tval))
                    return RUN_ERROR;
            } else if (m->trace) {
                trace_step(stderr, &info);
            }
        }
        bus_tick(&m->bus);
    }
}
