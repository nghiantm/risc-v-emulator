# Memory Map

Source of truth: `common/memory_map.h`. All addresses are physical; there is no MMU.

| Region | Base | Size | Notes |
|---|---|---|---|
| SYSCON | `0x00100000` | `0x1000` | exit device |
| CLINT | `0x02000000` | `0x10000` | `mtime` / `mtimecmp` |
| UART | `0x10000000` | `0x1000` | transmit only |
| Sensor | `0x10001000` | `0x1000` | see `SENSOR_DATASHEET.md` |
| Main RAM | `0x80000000` | `0x400000` (4 MiB) | code, data, stack. Never faulted. |
| DUT RAM | `0x90000000` | `0x10000` (64 KiB) | fault-injectable test target |

Everything else (including address 0) is unmapped and faults.

## SYSCON (`0x00100000`)
- Reads return 0.
- A 32-bit write ends the run: `0x5555` (`SYSCON_PASS`) is a pass; `(code << 16) | 0x3333` (`SYSCON_FAIL_TAG`) is a fail with `code`. Any other value is ignored.

## CLINT (`0x02000000`)
| Offset | Register |
|---|---|
| `0x4000` | `mtimecmp` low |
| `0x4004` | `mtimecmp` high |
| `0xBFF8` | `mtime` low |
| `0xBFFC` | `mtime` high |

- Reset: `mtime = 0`, `mtimecmp = 0xFFFFFFFF_FFFFFFFF`.
- `MTIP = (mtime >= mtimecmp)`, 64-bit unsigned compare.
- Other offsets and non-4-byte accesses fault.

## UART (`0x10000000`)
| Offset | Register |
|---|---|
| `0x0` | TX: low byte of any-size write goes to stdout |
| `0x4` | STATUS: always `0x1` (`UART_STATUS_TXRDY`) |

## Access faults
- Unmapped address, an access straddling two regions, or an access a device refuses → load/store access fault (mcause 5/7), `mtval` = address.
- Misaligned access to RAM (main or DUT) works, byte by byte, little-endian. Misaligned or non-32-bit access to CLINT/sensor is an access fault.
- Instruction fetch from outside main RAM → fetch access fault (mcause 1).
