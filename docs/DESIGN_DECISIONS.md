# Design Decisions and Limitations

## Emulator
- **Misaligned data access is supported**, not trapped, for RAM (byte-by-byte little-endian) so `rv32ui-p-ma_data` passes with no special case. MMIO refuses misaligned access (access fault). Causes 4 and 6 are never raised. Instruction fetch must be 4-aligned: bad branch/jump target → cause 0, `mtval` = target.
- **`mtval`**: illegal instruction → the instruction word; access fault → faulting address; fetch-misaligned → target; ecall/ebreak → 0.
- **CSRs implemented**: `mstatus` (MIE, MPIE; MPP reads 3), `misa` (`0x40001100`, writes ignored), `mhartid` (0, writes ignored), `mie` (MTIE, MEIE), `mtvec` (direct, low 2 bits 0), `mscratch`, `mepc` (low 2 bits 0), `mcause`, `mtval`, `mip` (read-only, composed from device lines). Any other CSR → illegal instruction. Writes to unimplemented bits are ignored. `csrrs/csrrc` with rs1 = x0 (or uimm 0) do not write.
- **Trap with `mtvec == 0`** ends the run with `RESULT error trap with no handler installed`.
- **`mtime`** counts machine-loop iterations (one per instruction or interrupt entry), not cycles.
- **Interrupts** are level-sensitive, checked only between instructions; external beats timer; taken iff `MIE` and `mip & mie`.
- **tohost**: if the ELF has a `tohost` symbol, the first 4-byte store to it stops the run: value 1 pass, otherwise fail with test number `value >> 1`. The store still completes.
- **Exit check precedes timeout check**, so an exit on the last allowed instruction is a pass.
- **Run limits**: `--max-insts` default 50,000,000; timeout exits 2.
- `RESULT` stderr lines and `DIAG` stdout lines are an internal contract between `emu`, `fw/` and `scripts/`.

## Deviations noted during implementation
- The top-level Makefile builds the emulator directly (no `emu/Makefile`).
- `exit_request.h` was split out of `machine.h` to break an include cycle.
- Sub-word reads from DUT RAM apply read-side faults to the containing word; `addr_stuck` applies per byte offset.
- `crt0.S` calls C `syscon_exit` after `main` rather than using `memory_map.h` macros in assembly; `link.ld` hardcodes the RAM origin/length (a linker script cannot include the header).

## Firmware diagnostics
- RAM tests touch DUT RAM only; every wait loop is bounded. Diagnostics are never weakened to hit a score.

## Known limitations
- No C/A/F/D extensions, S/U modes, MMU, PMP, vectored mtvec, `wfi`, software interrupts.
- No cycle accuracy; `fence`/`fence.i` are no-ops.
- Only 4-byte stores to `tohost` are watched.
- `ram_drop_writes` with a large `every` may not trigger in a short program; the campaign uses 1000.
- Address 0 and all unmapped memory fault on purpose.
