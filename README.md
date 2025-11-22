# RV32IM Emulator + Hardware Diagnostics

An RV32IM instruction-set emulator in C (no third-party libraries) that models a small SoC (UART, CLINT timer,
SYSCON exit device, a temperature sensor, a fault-injectable "device-under-test" RAM), bare-metal firmware that runs
on it, and a Python harness that shows the firmware's diagnostics catch ten injected hardware faults.

```
  fw/ (C + asm, riscv gcc)            scripts/ (Python, stdlib only)
   hello, timer_test, diag  ──ELF──▶  run_isa_tests.py   (riscv-tests)
                                      fault_campaign.py  (10 faults)
                                           │ runs
                                           ▼
                         build/emu  (cpu ─ csr/trap ─ bus)
                                          │
        ┌──────────┬──────────┬───────────┼───────────┬──────────┐
      main RAM   DUT RAM    SYSCON      CLINT       UART       sensor
     0x80000000 0x90000000 0x00100000 0x02000000 0x10000000 0x10001000
                (faults)                                      (faults)
```

## Build and run (fresh clone)

```bash
git clone --recursive <repo> && cd <repo>
sudo apt install gcc-riscv64-unknown-elf     # or set RISCV_PREFIX
make riscv-tests                             # builds rv32ui/rv32um test ELFs into build/isa/
make emu fw
make test                                    # unit + isa + campaign
```

Individual pieces:

```bash
make unit                                    # C unit tests
python3 scripts/run_isa_tests.py             # all ISA tests; optional substring filter: ... add
python3 scripts/fault_campaign.py            # fault campaign
./build/emu build/fw/hello.elf               # run firmware
./build/emu --fault sensor_stuck build/fw/diag.elf
./build/emu --trace --max-insts 1000 prog.elf
```

Emulator exit codes: 0 pass, 1 fail, 2 timeout, 3 error. The last stderr line is `RESULT ...`.

## Results (real output of `make test`)

ISA tests: `Total: 50  Passed: 50  Failed: 0  Timeout: 0  Error: 0` (rv32ui-p: 42, rv32um-p: 8)

```
#  fault                                expected test      caught  collateral failures
 1  ram_data_stuck0:bit=5                data_bus           yes     addr_bus, march_c_minus
 2  ram_data_stuck1:bit=9                data_bus           yes     march_c_minus
 3  ram_addr_stuck:line=10               addr_bus           yes     march_c_minus
 4  ram_bitflip:addr=0x90003A40,bit=3    march_c_minus      yes     -
 5  ram_drop_writes:every=1000           march_c_minus      yes     -
 6  sensor_bad_id                        sensor_id          yes     -
 7  sensor_stuck                         sensor_variation   yes     -
 8  sensor_out_of_range                  sensor_range       yes     sensor_variation
 9  sensor_never_ready                   sensor_ready       yes     sensor_irq, sensor_range, sensor_variation
10  sensor_no_irq                        sensor_irq         yes     -
Caught 10/10, baseline false positives: 0
```

## Docs
- [Memory map](docs/MEMORY_MAP.md)
- [Sensor datasheet](docs/SENSOR_DATASHEET.md)
- [Fault catalog](docs/FAULT_CATALOG.md)
- [Design decisions and limitations](docs/DESIGN_DECISIONS.md)
