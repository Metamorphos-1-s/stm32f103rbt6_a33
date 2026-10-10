# Stage 5P Unified-R4 transaction attribution and 40 Hz resource closure

Date: 2026-10-11
Baseline: `222a62310a814dbb708d385ef749255f263245e4`

## R3 revision attribution

The R3 `revision=21` observation was expected source behavior, not an
unexplained write. `MetrologyManager_SetA13Pair()` calls
`SystemContext_SetRequestedR5Pair()` once when the requested pair changes;
that function updates both request fields and calls `SystemContext_MarkConfigChanged()` once.

The R4 host ledger records the following raw successful transactions:

| Operation | Before | After | Saved | Result |
| --- | ---: | ---: | ---: | --- |
| profile 0 -> 1 | 19 | 20 | 19 | `ACCEPTED=1`, async 40 Hz complete |
| OFF+SHADOW -> ACTIVE+STATIC | 20 | 21 | 19 | pair result OK, readback ACTIVE/STATIC |
| ACTIVE+STATIC -> ACTIVE+DOSING | 21 | 22 | 19 | pair result OK, readback ACTIVE/DOSING |
| repeated return STATIC | 22 | 23 | 19 | pair result OK, readback ACTIVE/STATIC |

No SAVE occurred. The ledger rejects any revision not associated with a
successful transaction and keeps the saved revision fixed at the pre-test
value. Host synthetic revision tests pass.

## Software/diagnostic gate

R4 reuses the statically qualified 0x0526/0x010E diagnostic image without MCU
changes:

- SHA-256: `A482E1912CB34725553FD04A0E2EAA07BF7709E21D465D75DB579565A7C7001C`
- RAM 19,384 B; conservative stack 1,448 B; collision margin 672 B.
- Host cumulative contract tests: 11/11.
- Host Debug/Release regression: 47/47 each.

## Target resource evidence

Fresh preflight was `0x0520/0x0109`, OFF+SHADOW, 10 Hz/filt1/strength3,
calibrated, revision/saved 19/19, dirty/fault/overrun zero. Application and
V3 configuration were backed up before installation. The diagnostic image was
installed app-only and later restored byte-for-byte.

The corrected 40 Hz epoch run recorded:

- diagnostic flags `1`, valid DWT and stack watermark;
- physical produced `9610`, consumed `9610`, FIFO `0`, invalid/read errors/
  overrun `0`;
- engine count `9600` within the epoch, with legal 40 Hz admission separate
  from physical production;
- maximum Feed `82,166` cycles at 72 MHz, approximately `1.141 ms`;
- maximum App_Run loop `475,028` cycles, approximately `6.598 ms`;
- maximum loop interval `475,080` cycles, approximately `6.598 ms`;
- no host gap was treated as MCU loss. Three host gaps were recorded and
  retained as polling gaps.

Coverage mask was `0x12F` (OFF, HOLDOFF, REFERENCE_FILL, OBSERVATION_FILL,
FAST_TRACKING and DOSING). TRACKING, STEP_PENDING and STEP_SETTLING were not
covered during this run. Therefore the complete nine-path resource gate is
**INCOMPLETE**, despite the timing and conservation sub-gates passing.

The raw resource capture and all rollback evidence are retained under
`Results/stage5p_unified/hardware/stage5pr4_20261011/`.

## Final state and deferred work

Restored final device:

- `0x0520 / 0x0109`, OFF+SHADOW, offset 0;
- revision/saved 19/19, dirty/fault/overrun 0/0/0;
- Checkweigh OFF, calibration valid;
- application SHA equals the fresh pre-test backup;
- configuration SHA equals `856BD8F5C14760561FC4BFEC4274FE0C5480C19BC4422046617C2450F8439733`.

The exact 0x0524 image (`CBAB6D5E...35FFBF1`) was not burned. Its function,
SAVE/migration, power-cycle, menu and battery UI gates are NOT RUN. A13B
long-duration drift efficacy remains INCONCLUSIVE, with no change to formal
metrology or cross-sensor qualification status.

**Conclusion:** R4 transaction attribution PASS; target timing/conservation
PASS; complete 40 Hz path resource qualification INCOMPLETE; 0x0524 delivery
validation NOT RUN.
