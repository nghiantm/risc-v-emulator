#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elf.h"
#include "fault.h"
#include "machine.h"

#define USAGE "usage: emu [--max-insts N] [--trace] [--fault NAME[:key=val,...]]... program.elf"
#define DEFAULT_MAX_INSTS 50000000u

typedef struct {
    uint64_t    max_insts;
    bool        trace;
    FaultSet    faults;
    const char *path;
} Options;

/* Every failure before the run is a single RESULT error line and exit code 3. */
static int die(const char *msg)
{
    fprintf(stderr, "RESULT error %s\n", msg);
    return RUN_ERROR;
}

static bool parse_count(const char *s, uint64_t *out)
{
    if (!isdigit((unsigned char)*s))
        return false;
    char *end;
    *out = strtoull(s, &end, 10);
    return *end == '\0';
}

/* Returns NULL on success, else an error message (in err or a literal). */
static const char *parse_args(int argc, char **argv, Options *o, char *err, size_t errlen)
{
    o->max_insts = DEFAULT_MAX_INSTS;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--trace") == 0) {
            o->trace = true;
        } else if (strcmp(argv[i], "--max-insts") == 0) {
            if (++i >= argc || !parse_count(argv[i], &o->max_insts))
                return "--max-insts needs a non-negative integer";
        } else if (strcmp(argv[i], "--fault") == 0) {
            if (++i >= argc)
                return "--fault needs a spec";
            if (o->faults.count == FAULT_MAX)
                return "too many --fault flags";
            if (!fault_parse(argv[i], &o->faults.items[o->faults.count], err, errlen))
                return err;
            o->faults.count++;
        } else if (argv[i][0] == '-') {
            return "unknown option";
        } else if (o->path) {
            return "more than one program given";
        } else {
            o->path = argv[i];
        }
    }
    return o->path ? NULL : USAGE;
}

static void print_result(const Machine *m, RunResult r, uint64_t max_insts)
{
    fflush(stdout);                    /* UART output first, so stdout/stderr ordering is stable */
    switch (r) {
    case RUN_PASS:
        fprintf(stderr, "RESULT pass source=%s\n", m->exit_req.source);
        break;
    case RUN_FAIL:
        if (m->exit_req.source[0] == 't')
            fprintf(stderr, "RESULT fail source=tohost test=%u\n", (unsigned)m->exit_req.code);
        else
            fprintf(stderr, "RESULT fail source=syscon code=0x%x\n", (unsigned)m->exit_req.code);
        break;
    case RUN_TIMEOUT:
        fprintf(stderr, "RESULT timeout insts=%llu\n", (unsigned long long)max_insts);
        break;
    case RUN_ERROR:
        fprintf(stderr, "RESULT error %s\n", m->error);
        break;
    }
}

int main(int argc, char **argv)
{
    Options opts = { 0 };
    char err[160];
    const char *bad = parse_args(argc, argv, &opts, err, sizeof err);
    if (bad)
        return die(bad);

    size_t len;
    uint8_t *img = elf_read_file(opts.path, &len, err, sizeof err);
    if (!img)
        return die(err);

    Machine m;
    if (!machine_init(&m)) {
        free(img);
        return die("out of memory");
    }
    m.faults = opts.faults;
    m.trace = opts.trace;

    uint32_t tohost;
    if (!elf_load(img, len, &m.bus, &m.entry, err, sizeof err)) {
        free(img);
        machine_free(&m);
        return die(err);
    }
    if (elf_find_symbol(img, len, "tohost", &tohost)) {
        m.bus.tohost_enabled = true;
        m.bus.tohost_addr = tohost;
    }
    free(img);

    RunResult r = machine_run(&m, opts.max_insts);
    print_result(&m, r, opts.max_insts);
    machine_free(&m);
    return (int)r;
}
