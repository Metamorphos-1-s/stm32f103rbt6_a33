# Stage 5C-D BLE Calibration Workflow

Status: **SOFTWARE COMPLETE - NOT TESTED ON HARDWARE**

Stage 5C-C closed on `main` commit
`04d498527e9aaa7434f20f72ef9dfd4fea3a3dd7`, tagged
`stage5c-c-hw-tested`. Stage 5C-D is developed on `stage5c-ble-d`.

## Scope

The goal is a safe BLE controller for the existing two-point calibration:
state query, begin, known-mass staging, stable zero/load capture, candidate
review, explicit RAM apply, cancel and the existing explicit SAVE. It does not
add multi-point calibration, weight recognition/attraction, coefficient
correction, linearization, temperature compensation, Runtime Drift changes,
factory reset, OTA, pairing, W02 AT/FF12, baud changes or metrology claims.

## Existing architecture and integration

`CalibrationModel_BuildMass` and `CalibrationModel_Validate` remain the only
calibration mathematics. Samples come from `MetrologyManager_GetSnapshot()` and
the same `filtered_raw` source used by the local panel. `CommandService` owns
the transport-neutral session. `ConfigApplication` validates and atomically
applies the complete candidate through MetrologyManager/WeightEngine.
PersistenceManager and the existing A/B ConfigStore remain the only Flash path.

The local panel still presents its established prompts and editable mass, but
its actual zero/load capture and apply now use the same CommandService session
as Modbus and BLE. The local detector is presentation timing only; the shared
session independently requires and captures the authoritative stable window.

## Session model

Owners are NONE, LOCAL_UI, MODBUS and BLE. BEGIN snapshots the active unit,
decimal places, division, capacity, sample rate and gain, assigns a nonzero
16-bit session ID, and starts in WAIT_ZERO_STABLE. States are IDLE,
WAIT_ZERO_STABLE, ZERO_READY, ZERO_CAPTURED, WAIT_LOAD_STABLE, LOAD_READY,
RESULT_READY, APPLIED and FAILED.

The session owner protects calibration mutations and configuration changes that
could invalidate locked fields. A different local/Modbus/BLE owner receives
BUSY. Normal weighing, panel refresh, telemetry, GET DEVICE INFO, GET CONFIG and
Modbus FC03 remain available. The existing transactional config editor cannot
start during calibration; direct unit/profile changes are also BUSY.

Only a valid mutating calibration request refreshes `last_activity_ms`. At
120,000 ms the session is cancelled with wrap-safe unsigned time arithmetic,
the owner is released, and active calibration and dirty state remain unchanged.
GET state and telemetry do not keep an abandoned session alive.

## Mass, capture and validation

Known mass is canonical signed `MassValueUg` in micrograms, never float. It must
be positive, at or below locked capacity, divisible by the locked display
division, and exactly round-trip through the locked unit/decimal display.

Each capture uses an 8-sample `filtered_raw` window. Raw spread must be no more
than 50 counts for 500 ms. A premature capture returns NOT_STABLE and takes no
sample. Zero stores the rounded window average. Load requires a prior zero and
known mass, stores another average, and calls `CalibrationModel_BuildMass`.
The existing model rejects zero span, raw magnitude at or below 1000 counts,
invalid mass and overflow; either sensor direction is valid. No new sensitivity
threshold is invented.

Load capture is the validation stage, so no duplicate VALIDATE command exists.
The existing model has only safe whole-session cancellation, so no shadow
DISCARD state machine is added. RESULT_READY must be visible before APPLY.

## Apply, cancel and persistence

APPLY revalidates the candidate and locked configuration, copies the complete
calibration into a DeviceConfig candidate, then uses ConfigApplication. A
failure preserves the previous active calibration. Success updates RAM,
resets the WeightEngine calibration/runtime-drift reference through the existing
path, increments config revision, reports `persistent_dirty=1`, marks APPLIED
and releases the owner.

CANCEL before APPLY discards only the volatile session and never changes the
active calibration or dirty state. BEGIN/capture without APPLY is also lost on
reset. APPLY without SAVE is lost on power cycle under existing persistence
semantics. Ordinary `SAVE_CONFIG (0x27)` writes the inactive A/B slot and clears
dirty only on completion; there is no calibration-specific Flash command.

## BLE protocol and idempotency

REQUEST `0x80` and RESPONSE `0x81` are unchanged. Operations are GET STATE
`0x30`, BEGIN `0x31`, SET MASS `0x32`, CAPTURE ZERO `0x33`, CAPTURE LOAD `0x34`,
APPLY `0x35`, and CANCEL `0x36`. Mutations after BEGIN carry the session ID so a
delayed request from an earlier session cannot affect a new one. Every response
contains the MCU's 44-byte state snapshot; the PC never guesses state.

The existing four-entry transaction cache remains unchanged. Same transaction
ID/operation/flags/payload replays the byte-identical response. A conflicting
reuse returns TRANSACTION_CONFLICT. Therefore retries never resample zero/load,
reapply a candidate, recancel a new session or rewrite Flash.

Maximum calibration request and response frames are 24 and 66 bytes. Global V1
limits remain 142 and 110 bytes. Responses use the existing priority queue;
FAST 5 Hz, SLOW 1 Hz and the scheduled 353 B/s telemetry remain unchanged.

## PC workflow

`Tools/stage5c_hw/stage5c_ble.py` adds `cal-status`, `cal-begin`,
`cal-set-mass`, `cal-zero`, `cal-load`, `cal-apply`, `cal-cancel` and the
interactive `calibrate` wizard. State-changing commands require
`--allow-write --allow-calibration`. APPLY requires an explicit typed `APPLY`;
the wizard never applies after capture automatically. Flash SAVE additionally
requires `--allow-flash` and an explicit typed `SAVE`.

## Verification

Host coverage includes state serialization/dispatch, exact transaction replay,
owner conflicts, stale session IDs, invalid order, NOT_STABLE, valid/invalid
mass, load candidate creation, APPLY/dirty semantics, explicit CANCEL rollback,
config read/write locks, timeout boundary and timer wraparound, plus the existing
local UI calibration directions and cancellation. The clean host gate passes
12/12 CTest targets. Stage 5B Python regression passes 28/28 and Stage 5C Python
regression passes 9/9, including BLE state decoding and frame recovery.

Clean ARM builds pass with no new warnings:

- Debug: 125,532 B Flash / 14,464 B RAM, delta +3,808 B / +200 B.
- Release: 68,248 B Flash / 14,472 B RAM, delta +1,952 B / +192 B.
- BoardDiagnostics: 124,044 B Flash / 14,472 B RAM, delta +3,800 B / +200 B.

The Release load image ends at exclusive address `0x08010A98`, below the first
config slot at `0x0801F000`. The transport-neutral calibration object is 256 B,
up from the 64 B Stage 5C-C command calibration state, so its direct RAM cost is
192 B. Owner and workflow state are contained in that object. The four-entry
transaction cache is unchanged (968 B total), the response data buffer limit is
still 88 B, and the global maximum response frame remains 110 B. PC code is not
part of MCU resource use.

Schema stays V2 at 344 bytes; slots remain A
`0x0801F000-0x0801F7FF` and B `0x0801F800-0x0801FFFF`; Modbus map remains
`0x0102`; BLE Protocol V1 and REQUEST `0x80`/RESPONSE `0x81` remain unchanged.
The `.ioc`, USART1 9600, USART2 framing and Flash layout are unchanged. The
changed MCU code contains no dynamic allocation, `HAL_Delay`, floating-point
calibration arithmetic or FreeRTOS dependency.

## Hardware validation pending

No Stage 5C-D code has yet been programmed in this software-complete state.
Required board work is: preserve/hash both config slots, program Release without
mass erase, record the current calibration baseline, test GET/BEGIN/owner/SET/
ZERO/LOAD/retry/error/CANCEL rollback, then request explicit human confirmation
before APPLY and again before SAVE. Power-cycle persistence, unsaved behavior,
local/Modbus interoperability, timeout and a 600 s BLE/RS485/telemetry run must
also pass before declaring Stage 5C-D COMPLETE.

Known limitations: the MCU has no reliable BLE disconnect event, so owner
recovery is timeout-based; calibration remains two-point; the raw-span minimum
is the existing 1000-count rule rather than a load-cell sensitivity model; and
completion is engineering workflow validation, not legal-metrology approval.
