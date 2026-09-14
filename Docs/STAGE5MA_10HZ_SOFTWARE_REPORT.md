# Stage 5M-A 10 Hz Software Report

Status: `STAGE 5M-A NO ACCEPTABLE CANDIDATE; REQUIREMENTS OR ALGORITHM REVISION REQUIRED`.

## Delivered

- Current raw/filter/calibration/zero/tare/stability/display/alarm/Modbus/BLE pipeline audit.
- Git-bound inventory of 38 complete runs with development/holdout/regression split and labels.
- Deterministic replay v2 with explicit timestamp wrap, actual input-gap handling and separate host-observation omissions.
- Exact reproduction of the four published hardware baseline metric sets.
- Dual-IIR and robust dual-IIR offline comparison under one metric contract.
- HAL-free fixed-point C reference and Host stream runner.
- Python/C comparison: 900 actual records, zero integer/state mismatch.
- Resource evidence: 256 B state, 1724 B ARM `-Os` object text, 104 B Process stack, no dynamic allocation or module int64 division.

## Failed attempts retained

Replay v1 incorrectly interpreted FC03 observation omissions as device input gaps and kept stable false. G1/G2 proves device processing continued, so v2 reopened holdout once to correct only that input contract; no interpolation or parameter change occurred. V2 then failed real frozen thresholds: stable 5.573 s >3.0 s, slow false-stable 24.56% >10%, and static false-positive 1.53% >1%.

The robust dual-IIR is a research reference, not a selected candidate. No Stage 5M experiment is linked into Debug, Release, BoardDiagnostics or Stage5LDiagnostics. No firmware was flashed and no product behavior, protocol field, map, schema, persistent format, Active configuration or firmware version changed.

## Boundaries

Automatic zero, tare learning, calibration attraction and low-frequency drift subtraction are not implemented. The 40 Hz path remains contained because G3 external timing is blocked. Stage 5M-B hardware validation cannot begin without a newly selected candidate. Stage 5N and Stage 5O were not entered.

## Next revision criteria

A future candidate must improve slow-trend persistence without increasing static false positives, and decouple stable qualification from a long settling tail while preserving holdout noise. It must be tuned only on the frozen development runs or declare a new dataset/version before opening any new holdout. The existing v2 failure remains part of regression evidence.
