# Temperature Sensor

Base `0x10001000`, size `0x1000`. All registers are 32-bit; other access sizes and unknown offsets are access faults.

| Offset | Name | Access | Description |
|---|---|---|---|
| `0x00` | ID | R | `0x5E450001` |
| `0x04` | CTRL | R/W | bit 0 `START` (write-only, never stored), bit 1 `IRQ_EN` (stored) |
| `0x08` | STATUS | R | bit 0 `READY`, bit 1 `ERROR` (reserved, always 0) |
| `0x0C` | DATA | R | latest sample, signed centi-degrees C |
| `0x10` | COUNT | R | completed conversions |

## Operation
1. Write `CTRL` with `START` (and optionally `IRQ_EN`). `START` while a conversion is running is ignored.
2. After `SENSOR_LATENCY_INSTS` = 2000 machine-loop iterations, DATA is latched, COUNT increments, READY rises.
3. Reading DATA returns the latest value and clears READY. Reading DATA while not ready returns the last latched value (0 before the first conversion) and changes nothing else.
4. `START` does not clear a pending READY.

## Sample model
For 0-based conversion index `n`: `data = 2500 + 50 * (n % 100) + noise`, `noise = (xorshift32() % 41) - 20`, seed `0x1234ABCD` (shifts 13, 17, 5). Within one 100-sample period consecutive samples differ by at least 10, so three consecutive samples are never all equal. Valid range is `-4000..12500`.

## Interrupt
Level-sensitive: asserted while `READY && IRQ_EN`. Appears as `mip.MEIP` (bit 11) and is taken as mcause `0x8000000B` when `mie.MEIE` and `mstatus.MIE` are set. It has priority over the timer. Reading DATA clears READY and so drops the line.
