#ifndef MACHINE_H
#define MACHINE_H

#include <stdbool.h>
#include <stdint.h>

#include "bus.h"
#include "exit_request.h"
#include "fault.h"
#include "ram.h"
#include "syscon.h"
#include "uart.h"

typedef enum {            /* values equal the process exit codes */
    RUN_PASS = 0, RUN_FAIL = 1, RUN_TIMEOUT = 2, RUN_ERROR = 3
} RunResult;

/* Fields are added as the modules that own them appear (cpu, clint, sensor). */
typedef struct {
    Bus bus;
    Ram main_ram;
    Syscon syscon;
    Uart uart;
    ExitRequest exit_req;
    FaultSet faults;      /* parsed from --fault; devices that honour them arrive in later milestones */
    uint32_t entry;       /* ELF entry point; becomes cpu.pc once the CPU exists */
    const char *error;    /* message for RUN_ERROR */
    bool trace;
} Machine;

/* Zeroes *m and registers main RAM, SYSCON and UART on the bus. false on allocation failure. */
bool machine_init(Machine *m);
void machine_free(Machine *m);

RunResult machine_run(Machine *m, uint64_t max_insts);

#endif /* MACHINE_H */
