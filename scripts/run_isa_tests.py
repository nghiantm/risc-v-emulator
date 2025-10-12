#!/usr/bin/env python3
"""Run every built rv32ui-p-* / rv32um-p-* ELF under build/emu and print a result table.

usage: run_isa_tests.py [NAME_SUBSTRING]
Exit 0 only if every selected test passes.
"""
import glob
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EMU = os.path.join(ROOT, "build", "emu")
TIMEOUT_S = 30


def is_elf(path):
    with open(path, "rb") as f:
        return f.read(4) == b"\x7fELF"


def run_one(path):
    """Returns (status, failing_test_number_or_empty)."""
    try:
        p = subprocess.run([EMU, path], capture_output=True, text=True, timeout=TIMEOUT_S)
    except subprocess.TimeoutExpired:
        return "TIMEOUT", ""
    lines = [l for l in p.stderr.splitlines() if l.startswith("RESULT ")]
    if not lines:
        return "ERROR", ""
    result = lines[-1]
    if result.startswith("RESULT pass") and p.returncode == 0:
        return "PASS", ""
    m = re.match(r"RESULT fail source=tohost test=(\d+)", result)
    if m and p.returncode == 1:
        return "FAIL", m.group(1)
    if result.startswith("RESULT timeout") and p.returncode == 2:
        return "TIMEOUT", ""
    return "ERROR", ""


def main():
    pattern = sys.argv[1] if len(sys.argv) > 1 else ""
    if not os.path.exists(EMU):
        print("build/emu not found: run `make emu`", file=sys.stderr)
        return 1
    tests = sorted(
        p for p in glob.glob(os.path.join(ROOT, "build", "isa", "rv32u[im]-p-*"))
        if not p.endswith(".dump") and is_elf(p) and pattern in os.path.basename(p)
    )
    if not tests:
        print("no matching tests in build/isa (run `make riscv-tests`)", file=sys.stderr)
        return 1

    counts = {"PASS": 0, "FAIL": 0, "TIMEOUT": 0, "ERROR": 0}
    print(f"{'name':<28} {'status':<8} failing test #")
    for path in tests:
        status, num = run_one(path)
        counts[status] += 1
        print(f"{os.path.basename(path):<28} {status:<8} {num}")
    total = len(tests)
    print(f"Total: {total}  Passed: {counts['PASS']}  Failed: {counts['FAIL']}  "
          f"Timeout: {counts['TIMEOUT']}  Error: {counts['ERROR']}")
    return 0 if counts["PASS"] == total else 1


if __name__ == "__main__":
    sys.exit(main())
