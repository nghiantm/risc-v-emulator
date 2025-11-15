#!/usr/bin/env python3
"""Fault-injection campaign: runs build/fw/diag.elf on build/emu with no fault, then with each fault alone.

A fault is caught only if its expected test reports FAIL. Other failing tests are listed as collateral.
Only DIAG stdout lines and the exit code are parsed.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EMU = ROOT / "build" / "emu"
DIAG = ROOT / "build" / "fw" / "diag.elf"
TIMEOUT_S = 60

# (fault spec, expected detecting test) -- fixed by ARCHITECTURE.md
FAULTS = [
    ("ram_data_stuck0:bit=5", "data_bus"),
    ("ram_data_stuck1:bit=9", "data_bus"),
    ("ram_addr_stuck:line=10", "addr_bus"),
    ("ram_bitflip:addr=0x90003A40,bit=3", "march_c_minus"),
    ("ram_drop_writes:every=1000", "march_c_minus"),
    ("sensor_bad_id", "sensor_id"),
    ("sensor_stuck", "sensor_variation"),
    ("sensor_out_of_range", "sensor_range"),
    ("sensor_never_ready", "sensor_ready"),
    ("sensor_no_irq", "sensor_irq"),
]

DIAG_LINE = re.compile(r"^DIAG (\w+) (PASS|FAIL)\b")
SUMMARY = re.compile(r"^DIAG SUMMARY passed=\d+ failed=\d+ mask=0x[0-9a-f]+$")


def run(fault=None):
    """Returns (results {test: 'PASS'|'FAIL'}, has_summary, exit_code); results is None on timeout."""
    cmd = [str(EMU)] + (["--fault", fault] if fault else []) + [str(DIAG)]
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=TIMEOUT_S)
    except subprocess.TimeoutExpired:
        return None, False, None
    results, summary = {}, False
    for line in p.stdout.splitlines():
        if SUMMARY.match(line):
            summary = True
            continue
        m = DIAG_LINE.match(line)
        if m:
            results[m.group(1)] = m.group(2)
    return results, summary, p.returncode


def main():
    for path in (EMU, DIAG):
        if not path.exists():
            print(f"missing {path} (run `make emu fw`)")
            return 1

    base, base_summary, base_rc = run()
    false_pos = 0 if base is None else sum(1 for r in base.values() if r == "FAIL")
    if base is None or false_pos or not base_summary or base_rc != 0:
        print("CAMPAIGN INVALID: baseline failed")
        return 1

    print(f"{'#':>2}  {'fault':<36} {'expected test':<18} {'caught':<7} collateral failures")
    caught_count = 0
    for i, (spec, expected) in enumerate(FAULTS, 1):
        results, _, _ = run(spec)
        failed = {t for t, r in (results or {}).items() if r == "FAIL"}
        caught = expected in failed
        caught_count += caught
        collateral = ", ".join(sorted(failed - {expected})) or "-"
        print(f"{i:>2}  {spec:<36} {expected:<18} {'yes' if caught else 'NO':<7} {collateral}")

    print(f"Caught {caught_count}/{len(FAULTS)}, baseline false positives: {false_pos}")
    return 0 if caught_count == len(FAULTS) else 1


if __name__ == "__main__":
    sys.exit(main())
