# Fault Catalog

Syntax: `--fault NAME[:key=val,...]`, repeatable. Values decimal or `0x` hex. All keys are required; unknown keys are an error (exit 3). Faults only affect DUT RAM and the sensor.

| Fault | Models | Parameters | Emulator semantics | Detected by |
|---|---|---|---|---|
| `ram_data_stuck0` | data line shorted to ground | `bit` 0..31 | bit cleared in every **read** of the containing word (all access sizes) | `data_bus`: walking-1 pattern at that bit reads back 0 |
| `ram_data_stuck1` | data line shorted to supply | `bit` 0..31 | bit set in every read | `data_bus`: walking-0 pattern reads back with bit set |
| `ram_addr_stuck` | address line stuck low | `line` 2..15 | bit `line` of the byte offset cleared on reads **and** writes, so two addresses alias | `addr_bus`: write to the single-bit offset also lands on offset 0, disturbing it |
| `ram_bitflip` | failing memory cell | `addr` absolute address in DUT RAM, `bit` 0..31 | bit XORed on reads of the word containing `addr`; neighbours clean | `march_c_minus`: every cell is read as both 0 and 1, so the flipped cell mismatches |
| `ram_drop_writes` | write enable glitch | `every` >= 1 | every Nth DUT write access (all sizes, counted from 1) is discarded | `march_c_minus`: a lost write leaves the old value, caught by the next read pass |
| `sensor_bad_id` | wrong/absent device | none | ID reads `0xBAD0BAD0` | `sensor_id` |
| `sensor_stuck` | frozen ADC | none | DATA always 2500 | `sensor_variation`: 8 identical samples |
| `sensor_out_of_range` | broken front end | none | DATA always 20000 | `sensor_range`: above 12500 |
| `sensor_never_ready` | dead conversion logic | none | READY never rises, COUNT stays 0 | `sensor_ready`: poll timeout |
| `sensor_no_irq` | broken interrupt line | none | conversions complete normally; interrupt line never asserted | `sensor_irq`: bounded wait expires |

Counters (drop_writes count, sensor PRNG) start at reset, so every run is reproducible. The sensor PRNG advances on every conversion, faulted or not.

## Collateral failures (from a real campaign run)
Stuck/aliasing faults also break other RAM tests (e.g. `ram_data_stuck0` also fails `addr_bus` and `march_c_minus`); `sensor_out_of_range` also fails `sensor_variation`; `sensor_never_ready` also fails `sensor_range`, `sensor_variation`, `sensor_irq` (timeouts). A fault counts as caught only if its expected test fails; collateral failures are only reported.
