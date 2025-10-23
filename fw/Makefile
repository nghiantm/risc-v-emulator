# Firmware: freestanding RV32IM programs that run on build/emu. Invoked from the top-level Makefile.
RISCV_PREFIX ?= riscv64-unknown-elf-
CC := $(RISCV_PREFIX)gcc
OUT ?= ../build/fw

CFLAGS  = -march=rv32im_zicsr -mabi=ilp32 -O2 -ffreestanding -nostdlib -mno-relax \
          -Wall -Wextra -Werror -I include -I ../common
LDFLAGS = -T link.ld -nostdlib -Wl,--no-warn-rwx-segments

COMMON_SRCS := crt0.S $(wildcard src/*.c)
COMMON_HDRS := $(wildcard include/*.h) ../common/memory_map.h link.ld

EXAMPLES := hello
ELFS := $(addprefix $(OUT)/,$(addsuffix .elf,$(EXAMPLES)))

all: $(ELFS)

$(OUT)/%.elf: examples/%.c $(COMMON_SRCS) $(COMMON_HDRS)
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) $(LDFLAGS) $(COMMON_SRCS) $< -o $@

.PHONY: all
