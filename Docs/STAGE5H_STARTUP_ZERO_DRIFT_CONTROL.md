# Stage 5H Startup Zero and Runtime Drift Control

## Status

Branch: `stage5h-startup-zero-drift-control`

Development base: formal Stage 5G main merge `9615751` (tag
`stage5g-cs1237-od-tested`).

Status: **SOFTWARE REGRESSION PASS; OLD- AND NEW-BOARD HARDWARE PASS; READY
FOR FORMAL MERGE AND TESTED TAG**.

## Startup Auto Zero (P-Zr)

- Default is OFF and the saved enable uses one explicit SystemConfig reserved
  byte. Schema remains V2 and the encoded payload remains exactly 344 bytes.
- Old records contain zero in that byte and therefore migrate as OFF.
- Advanced menu order is `StAb -> ZrnG -> P-Zr -> OL`; STAR/HASH toggles,
  FUNCTION applies, TARE cancels and ZERO has no digit-selection effect.
- Modbus active/staging registers are `013C` / `017C`; BLE GET_CONFIG appends
  one byte at absolute payload offset 84 and SET_CONFIG_FIELD uses ID 20.
- Apply changes configuration only. The enable is sampled once by `App_Init`,
  so it cannot zero the current boot.
- The nonblocking boot state machine starts its 10 s engineering timeout at
  first RUN entry, uses authoritative gross mass and the existing `ZrnG`
  `zero_range_ug`, and calls the existing MetrologyManager/WeightEngine ZERO
  path. Runtime zero offset is not persisted.
- Restored retained tare produces SKIPPED_TARE. Invalid calibration, weight
  fault, timeout and out-of-range are terminal; no terminal state retries in
  the same boot.
- Read-only result telemetry is `0260-0269` in Map `0x0104`.

## Runtime Drift Engineering Control

The existing compensator remains the only implementation. It is still an
experimental engineering feature, defaults OFF at every power-up, is not part
of SAVE, the local menu or ordinary BLE configuration, and retains its existing
read-only `0200-021D` telemetry.

Mailbox ID 25 remains compatible (`arg0=0/1`). New argument-free commands are
26 ENABLE, 27 DISABLE and 28 RESET. ENABLE and DISABLE clear correction,
reference, window and stable count, ending in ARMING and DISABLED respectively.
RESET clears the same state while preserving enabled state. Duplicate tokens
retain the existing one-execution/cached-response behavior.

The correction remains signed int64 micrograms and is subtracted after raw
calibration/business zero and before tare. Manual ZERO and calibration clear
it; TARE/CLEAR TARE rearm it; profile/filter reconfiguration resets it.

## Software Evidence

- Host CTest: 16/16 PASS (the prior 15 targets plus startup-auto-zero tests).
- Stage 5B Python: 29/29 PASS (prior 28 plus explicit Drift command checks).
- Stage 5C Python: 12/12 PASS.
- Debug: 87,212 B Flash / 14,864 B RAM.
- Release: 74,840 B Flash / 14,848 B RAM.
- BoardDiagnostics: 124,324 B Flash / 14,760 B RAM. At 97.91% of its
  124 KiB application region, this image has little remaining Flash margin.

## Required Hardware Validation

### Old board (ST-LINK + COM7, 2026-08-17)

P-Zr OFF/terminal-disabled, ON/in-range/APPLIED, out-of-range/SKIPPED_RANGE,
10 s unstable/TIMEOUT, and restored-tare/SKIPPED_TARE all passed. Runtime Drift
ENABLE/ARMING, RESET while enabled, DISABLE/OFF, and power-cycle OFF passed.
BLE V1 GET_CONFIG returned 85 bytes with P-Zr enabled. The final concurrent
120 s run passed BLE 854 telemetry frames and 6/6 command responses with zero
disconnect, timeout, CRC, sequence-gap, duplicate, or retry errors; COM7
passed 557/557 FC03 requests with zero timeout/CRC/exception/retry errors.
The first retention/TARE setup attempt returned an invalid validation result;
the corrected configuration and subsequent restored-tare test passed.

The old board is currently empty, P-Zr enabled, tare cleared, and
`config_dirty=0`; the operator restored the original ZrnG locally.

### Remaining gate

### New board (W02_00832C + COM7, 2026-08-17)

The Stage 5H Release image was programmed and verified while preserving the
configuration region. P-Zr OFF/terminal-disabled, ON/in-range/APPLIED,
SKIPPED_RANGE, 10 s TIMEOUT, and restored-tare/SKIPPED_TARE all passed. Runtime
Drift ENABLE/ARMING, RESET, DISABLE/OFF, and power-cycle OFF passed. BLE
GET_CONFIG returned 85 bytes with P-Zr enabled. After clearing TARE and saving,
the final empty-board state was stable with `config_dirty=0`.

The final concurrent 120 s run passed BLE 854 telemetry frames and 6/6 command
responses with zero disconnect, timeout, CRC, sequence-gap, duplicate, or retry
errors; COM7 passed 556/556 FC03 requests with zero timeout/CRC/exception/retry
errors. The first invalid-report-directory collision was a host-side naming
collision before APPLY and did not affect the board; the command was rerun
successfully.

SWD-only counters are not Modbus-mapped and were not captured in either final
concurrency run. The 10 s timeout is an engineering default pending Stage 6
qualification. 640 Hz remains Deferred P1 and is not production-qualified.
