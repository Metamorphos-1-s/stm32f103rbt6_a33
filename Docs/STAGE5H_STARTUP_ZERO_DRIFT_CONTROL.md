# Stage 5H Startup Zero and Runtime Drift Control

## Status

Branch: `stage5h-startup-zero-drift-control`

Development base: provisional Stage 5G tip
`57c9a179c06044ed14a75ffaba62699722e1cc25`.

Status: **CODE COMPLETE; SOFTWARE REGRESSION PASS; HARDWARE VALIDATION
PENDING; FINAL CLOSEOUT BLOCKED BY STAGE 5G**.

Stage 5G remains **OPEN - OLD BOARD VALIDATION PENDING**. This branch must not
be merged to main or tagged as tested until Stage 5G is formally qualified,
merged and tagged, then final main is merged back into Stage 5H.

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

On the new board, verify P-Zr OFF, ON/in-range APPLIED, out-of-range skip,
unstable timeout and restored-tare skip. Verify Drift ENABLE/ARMING,
DISABLE/OFF, enabled RESET/ARMING and power-cycle OFF. Then repeat final Stage
5H qualification on both boards only after Stage 5G has formally closed.
