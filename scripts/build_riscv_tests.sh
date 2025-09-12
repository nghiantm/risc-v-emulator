#!/usr/bin/env bash
# Builds the 32-bit rv32ui-p-* / rv32um-p-* riscv-tests and copies the ELFs
# to build/isa/. The submodule is never written to: the build runs in a
# scratch copy under build/riscv-tests-src/.
set -euo pipefail

cd "$(dirname "$0")/.."
RISCV_PREFIX="${RISCV_PREFIX:-riscv64-unknown-elf-}"
SRC=third_party/riscv-tests
WORK=build/riscv-tests-src
OUT=build/isa

if ! command -v "${RISCV_PREFIX}gcc" >/dev/null 2>&1; then
    echo "error: ${RISCV_PREFIX}gcc not found on PATH" >&2
    echo "install: sudo apt install gcc-riscv64-unknown-elf   (Debian/Ubuntu)" >&2
    exit 1
fi

if [ ! -f "$SRC/env/p/riscv_test.h" ]; then
    echo "error: riscv-tests submodule not initialised; run:" >&2
    echo "  git submodule update --init --recursive" >&2
    exit 1
fi

rm -rf "$WORK" "$OUT"
mkdir -p "$WORK" "$OUT"
cp -r "$SRC/isa" "$SRC/env" "$WORK/"

# isa/Makefile passes -march=rv32g -mabi=ilp32 itself for rv32ui/rv32um.
# Only the physical-mode (-p-) tests are needed; the -v- ones want a libc.
printf 'print-%%:\n\t@echo $($*)\n' > "$WORK/print.mk"
targets=$(make -s -C "$WORK/isa" XLEN=32 -f Makefile -f ../print.mk print-rv32ui_p_tests print-rv32um_p_tests | tr '\n' ' ')
# shellcheck disable=SC2086
make -C "$WORK/isa" XLEN=32 RISCV_PREFIX="$RISCV_PREFIX" $targets >/dev/null

for f in "$WORK"/isa/rv32ui-p-* "$WORK"/isa/rv32um-p-*; do
    [ -f "$f" ] || continue
    # Only real ELF files (skip .dump and anything else).
    [ "$(head -c 4 "$f" | od -An -tx1 | tr -d ' \n')" = "7f454c46" ] || continue
    cp "$f" "$OUT/"
done

ui=$(find "$OUT" -name 'rv32ui-p-*' | wc -l)
um=$(find "$OUT" -name 'rv32um-p-*' | wc -l)
echo "rv32ui-p: $ui"
echo "rv32um-p: $um"
[ "$ui" -gt 0 ] && [ "$um" -gt 0 ] || exit 1
