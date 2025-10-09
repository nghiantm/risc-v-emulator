CC     ?= gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror -O2 -I common -I emu/src

BUILD := build

.PHONY: all emu unit riscv-tests isa clean

all: emu

# Emulator: every emu/src/*.c compiles into build/emu.
EMU_SRCS := $(wildcard emu/src/*.c)
EMU_HDRS := $(wildcard emu/src/*.h) $(wildcard common/*.h)

emu: $(BUILD)/emu

$(BUILD)/emu: $(EMU_SRCS) $(EMU_HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(EMU_SRCS) -o $@

# Unit tests: one executable per emu/tests/test_*.c, run as part of the build.
# Tests link every emulator module except main.c.
LIB_SRCS := $(filter-out emu/src/main.c,$(EMU_SRCS))
UNIT_SRCS := $(wildcard emu/tests/test_*.c)
UNIT_BINS := $(patsubst emu/tests/%.c,$(BUILD)/unit/%,$(UNIT_SRCS))
UNIT_HDRS := $(wildcard emu/tests/*.h) $(EMU_HDRS)

unit: $(UNIT_BINS)
	@set -e; for t in $(UNIT_BINS); do echo "run $$t"; $$t; done

$(BUILD)/unit/%: emu/tests/%.c $(LIB_SRCS) $(UNIT_HDRS)
	@mkdir -p $(BUILD)/unit
	$(CC) $(CFLAGS) $< $(LIB_SRCS) -o $@

riscv-tests:
	scripts/build_riscv_tests.sh

# Runs every built rv32ui/rv32um test under the emulator (needs `make riscv-tests` first).
isa: emu
	python3 scripts/run_isa_tests.py

clean:
	rm -rf $(BUILD)
