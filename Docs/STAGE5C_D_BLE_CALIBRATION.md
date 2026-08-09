# Stage 5C-D BLE Calibration Workflow

Status: **COMPLETE - SOFTWARE AND HARDWARE VALIDATED**

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
contains the MCU's 44-byte state snapshot; the PC never guesses state. While
IDLE, the locked unit/display/capacity fields report the current active
configuration and flags bit 7 reports whether the active calibration is valid.

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

Final clean ARM builds pass with no new warnings:

- Debug: 126,748 B Flash / 14,640 B RAM.
- Release: 68,928 B Flash / 14,648 B RAM.
- BoardDiagnostics: 125,268 B Flash / 14,640 B RAM.

The Release load image ends at exclusive address `0x08010D40`, below the first
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

## Hardware validation

Validation used the physical STM32F103RBT6 instrument, W02
`C8:46:82:00:83:24`, COM7 RS485 at address 1 and 115200 8N1, and a 500 g
reference mass. Runtime Drift remained disabled throughout.

Before APPLY, both 2 KiB config slots were dumped and hashed. Slot B sequence
10 was active, dirty was zero, and the valid negative-direction calibration was
zero raw `-43523`, load raw `-486173`, span `-442650`, 500,000,000 ug, about
`-1129.5606 ug/count`. The old calibration read about 0.06--0.09 g empty and
500.06 g with the reference mass. Slot A SHA-256 was
`B98273993DC01502DDE6BA54A0B2D30B73F236E7D6E91A2878E610CC77DFC9A3`; slot B
was `AE8CBF676DC379CCD4B0C4F66E557C49CCD13740E0B084EBEA3574F55F04F75C`.

GET STATE, BEGIN, valid and invalid mass, owner exclusion, invalid order,
stable ZERO/LOAD, exact duplicate ZERO/LOAD, CANCEL rollback, and the 120 s
timeout all passed. BLE, local UI and Modbus ownership were tested in both
directions where applicable; ordinary Modbus FC03 continued while BLE owned a
session. Hardware interaction exposed one local UI issue: an asynchronous BUSY
result left the CAL menu inactive. Commit `6e3792f` keeps the menu responsive
until ownership succeeds, and the repaired local enter/exit path passed on the
board. Commit `8d2b458` corrected host-only UART DMA boundary test setup and
expectations; it did not change product transport code.

The formal candidate captured zero raw `-43532`, 500 g load raw `-486139`, and
span `-442607`. Exact duplicate APPLY replayed the cached response. APPLY made
the candidate active in RAM and set dirty to one without changing slot B or
Flash sequence 10. The RAM result read 500.03 g. Ordinary SAVE and its exact
duplicate both returned OK; only one Flash transition occurred, to slot A
sequence 11, and dirty cleared. After a power cycle, state was IDLE/NONE,
calibration remained valid, slot A sequence 11 loaded with dirty zero, and the
reference mass read 500.07 g. Post-cycle empty read 0.01 g and the approximately
1 g check read 1.04 g (authoritative mass about 1.030 g), both stable and
locked. This is workflow validation, not legal-metrology certification.

The destructive APPLY-without-SAVE power-cycle case was deliberately not
repeated on hardware after the new calibration was saved. It is **NOT HARDWARE
TESTED**; existing Host tests cover the required reboot semantics. A
BEGIN/capture-only session was independently discarded by an actual power
cycle without changing Flash.

The final post-calibration 600 s run passed. BLE received 3612 frames (3010
FAST, 602 SLOW), completed 28/28 command responses and four BEGIN/CANCEL
sessions, with zero disconnect, timeout, retry, result error, transaction
mismatch, CRC error, sequence gap, duplicate, resync or partial byte. RS485
completed 1049/1049 read-only polling cycles; every cycle contained all five
expected FC03 reads, with zero exception, mean realtime-block latency 57.10 ms
and P99 58.99 ms. The requested custom RS485 CSV directory was absent at final
export, so the tool returned a reporting-path error after the full run; the
default report retained all 5246 TX/RX frame pairs and was used to reconstruct
the cycle and latency statistics. This was not a communication failure.

The final SWD snapshot recorded 5259 valid/addressed FC03 requests and 5259 TX
responses. IDLE queue overflow, DMA/UART errors, short frames,
inter-character errors, RTU overflow/transport errors, CRC/length errors, TX
errors and protocol violations were all zero; TX, framer and server states were
idle. Final calibration state was IDLE/NONE, slot A sequence 11, dirty zero.
The frozen communication architecture from `cabd6eb` therefore needs no
further USART2 or Modbus-framer changes.

Final gates: MSVC 19.43.34808 Host CTest 12/12, Stage 5B Python 28/28, Stage 5C
Python 9/9, and all three clean ARM builds passed. Schema V2 remains 344 bytes,
the A/B slot layout, `.ioc`, Modbus map `0x0102`, BLE V1 and UART settings are
unchanged.

Known limitations: the MCU has no reliable BLE disconnect event, so owner
recovery is timeout-based; calibration remains two-point; the raw-span minimum
is the existing 1000-count rule rather than a load-cell sensitivity model;
long-term zero drift, multi-point linearity and temperature behavior belong to
Stage 6; and completion is engineering workflow validation, not
legal-metrology approval.
