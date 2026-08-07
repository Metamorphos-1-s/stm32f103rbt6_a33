# Stage 5A-DSP-A-HF2-R1 Review Fixes

Baseline: `f6e025c3ca4abdb013667a813b69d5d6f066bd0a`.

## Review findings and fixes

- `0x0203` previously fell through to the arming-elapsed branch. Subtracting
  `0x0218` from the lower address wrapped before its `uint8_t` conversion, so
  the reserved register leaked an elapsed word. It is now explicitly named
  `MODBUS_RUNTIME_DRIFT_RESERVED`, read-only, and read-as-zero.
- Every 32/64-bit runtime-drift branch now checks both lower and upper bounds
  before subtracting its base address. FC03 tests cover single, crossing,
  whole-block, both word orders, `0x021D`, illegal `0x021E`, and write rejects.
- Map version remains `0x0102`; old addresses did not move. The full
  `0x0200-0x021D` contract and command ID 25 are documented in
  `MODBUS_REGISTER_MAP_V1.md`.
- HF2 permanently blocked learning while tare was active. R1 makes TARE and
  CLEAR TARE preserve offset, discard the old plateau/window, enter FROZEN,
  and re-arm. Learning remains based on uncompensated gross, so tare's net
  transition is never interpreted as physical drift. Long-lived tare can
  return to TRACKING and continue learning common gross drift.
- HF2 cleared offset on every APP_STATE_FAULT. R1 uses concrete fault codes.
  ADC/CS1237 reference/config, calibration, metrology/reference, weight-math
  and inconsistent config-apply faults clear offset. Invalid samples, FIFO
  overrun/gaps and UI/display/transient faults freeze it. Communication faults
  numbered above the local-weighing
  mask do not force APP_STATE_FAULT and preserve it.
- The application currently enters a terminal FAULT state and its common safe
  state disables CS1237. There is no production automatic FAULT recovery path.
  If such recovery is added, any path that powers/reconfigures the ADC must
  explicitly use the reference-invalid reset reason before sampling resumes.
- The online average now uses checked `int64` sum plus `uint32` count and one
  division at phase/window close. At the 3 kg profile, approximately 3001
  arming samples total about `9.0e12 ug`, safely below `INT64_MAX`; abnormal
  additions that would overflow are rejected. Positive, negative, mixed and
  non-divisible windows are tested.

Reset reasons distinguish power-on, manual/restore zero, calibration apply,
ADC/profile/reference changes and explicit control. Freeze reasons distinguish
instability, load change, TARE/CLEAR TARE re-arm, transient sample/fault, and
operation gating. Reasons are volatile diagnostics and do not alter Schema V2.

## Verification

All builds completed with no compiler errors or new warnings:

| Preset | Flash | RAM | Flash delta vs HF2 | RAM delta |
|---|---:|---:|---:|---:|
| Debug | 110,108 B | 10,968 B | +776 B | 0 B |
| Release | 59,576 B | 10,976 B | +300 B | 0 B |
| BoardDiagnostics | 107,892 B | 10,960 B | +760 B | 0 B |

Release load data ends at `0x0800E8B7`, below application limit
`0x0801EFFF`. Slot A remains `0x0801F000-0x0801F7FF`; slot B remains
`0x0801F800-0x0801FFFF`. Schema V2 remains 344 bytes and V1-to-V2 tests pass.

Seven host test executables pass. The repeated 12-hour integer simulation now
ends with offset 229,500 ug, residual 10,500 ug, and maximum hourly change
20,000 ug. Synthetic unload freezes and retains offset; the 1 g load guard and
LIMITED behavior pass. TARE-active integration returns to TRACKING and learns a
later gross drift step without clearing offset.

Status: **STAGE 5A-DSP-A-HF2-R1 SOFTWARE COMPLETE**. **SOFTWARE IMPLEMENTED -
NOT TESTED ON HARDWARE**.

This remains **PROVISIONAL RUNTIME DRIFT COMPENSATION**, **ENGINEERING MODE**,
**DEFAULT DISABLED**, and **NOT METROLOGICALLY VALIDATED**. Long-term real-load,
temperature, slow material-addition discrimination and minimum board regression
remain outstanding.
