# Project stage status

## Stage 5F

Branch `stage5f-six-digit-edit` adds six-position ZERO selection and a
selected-digit cursor blink to the existing menu and calibration numeric
editors. STAR/HASH direction and repeat, KeyService, ConfigEdit, metrology,
calibration math, alarm/checkweigh, protocols, Schema/Flash, UART settings, and
the `.ioc` file remain frozen.

Host 15/15, Stage 5B Python 28/28, Stage 5C Python 12/12, and Debug, Release,
and BoardDiagnostics clean builds passed. Release is 73,840 B Flash / 14,816 B
RAM; BoardDiagnostics ends at `0x0801DE6C`, leaving 4,500 B before the config
region.

CAP all-six-position cycling, 10000/100000-position edits, blink timing, DP,
leading blank, FUNCTION/TARE/invalid behavior, OL/Hi/HyS/Stability regressions,
and the independent calibration mass editor passed on hardware. The clean
60-second BLE/RS485 run received FAST 310, SLOW 62, CHECKWEIGH 62, completed
3/3 BLE commands and 222/222 FC03 requests, and had zero CRC, gap, duplicate,
disconnect, timeout, retry, exception, CS1237 overrun, event drop, or transport
error.

Stage 5F status: **SOFTWARE COMPLETE; HARDWARE COMPLETE; SIX-DIGIT EDIT AND
CURSOR BLINK TESTED; CONCURRENCY REGRESSION TESTED; COMPLETE**.

Full evidence is in `Docs/STAGE5F_SIX_DIGIT_EDIT_BLINK.md`. Stage 6 may begin
after merge to `main`, final software rebuild, and annotated tag
`stage5f-ui-tested`.

## Stage 5E

Branch `stage5e-checkweigh-alarm` completed final hardware qualification from
the frozen product baseline `b27df9af2923697490c382500f7069862a7c16aa`.
LOW/OK/HIGH, exact canonical boundaries, both hysteresis directions, direct
transitions, fast-load gating, a 120.079 s slow ramp, NET/GROSS/TARE/ZERO,
RGY/buzzer controls, safe OVERLOAD and recovery, calibration priority, local
configuration, Modbus map `0x0103`, BLE Protocol V1 and CHECKWEIGH_STATUS,
cross-transport ownership, persistence, power-cycle restore, and Flash A/B all
passed.

The final 606.047 s Release concurrency run received 4,214 BLE frames (FAST
3,010, SLOW 602, CHECKWEIGH 602), completed 5/5 BLE commands and 2,724/2,724
RS485 polling cycles, and had zero disconnect, timeout, retry, CRC, gap,
duplicate, resync, exception, or partial frame. SWD showed zero UART/DMA/RTU,
BLE drop/overflow, CS1237 overrun, event queue drop, and fault counters.

The slow ramp remained STABLE under the current short-window detector while
classification and outputs stayed correct; this is a P2 characterization for
Stage 6. Physical destructive FAULT injection was not performed because no
safe production mechanism exists; Host classification and the output chain
passed. Selected-digit blink and higher-position numeric editing are P2 UI
items deferred to the next version.

Stage 5E status: **SOFTWARE COMPLETE; HARDWARE COMPLETE; PERSISTENCE TESTED;
LOCAL/MODBUS/BLE TESTED; CONCURRENCY REGRESSION TESTED; COMPLETE**.

Full evidence is in `Docs/STAGE5E_FINAL_HARDWARE_VALIDATION.md`. Stage 6 may
begin only after the Stage 5E branch is merged, rebuilt on `main`, and frozen
by annotated tag `stage5e-hw-tested`.

## Stage 5C-A Closeout

Stage 5C-A BLE transport hardware and BLE/RS485 concurrency validation is
merged to `main` at `2eac9f0bf9b137f031bf91c1791d434436ca6020` and frozen by
annotated tag `stage5c-a-hw-tested`.

## Stage 5C-B

Branch: `stage5c-ble-b`.

Read-only realtime telemetry software is implemented: version 1 FAST_WEIGHT
and SLOW_STATUS frames, explicit little-endian serialization, shared CRC16,
stream parser, and non-blocking latest-data-wins scheduling. Final closeout
HEAD is `a5751eb536af2c16a2b5381da4e2332653e94de0`. The final Release image is
63,056 B Flash / 12,600 B RAM and remains below `0x0801F000`; firmware is
`0x050A`, register map is `0x0102`, and Schema V2 is 344 B.

Stage 5C-B hardware closeout is complete. The 600 s BLE + RS485 concurrency
run produced 3602 BLE frames (FAST 3001, SLOW 601) with no CRC error, sequence
gap, duplicate, disconnect, parser resynchronization, or partial frame. RS485
completed 2711/2711 read-only FC03 requests with no timeout, CRC error, or
Modbus exception. SWD recorded 15632 complete Modbus frames and matching IDLE,
T1.5, and T3.5 counts; Framer races and timer-start failures were 0. Two DMA
wrap-race recoveries caused no loss or timeout. The earlier RS485 timeout was
not reproduced and remains classified as a physical/link transient observation.

Stage 5C-B status: SOFTWARE COMPLETE; BLE TELEMETRY HARDWARE TESTED; BLE
TELEMETRY SOAK TESTED; CONCURRENCY REGRESSION TESTED; COMPLETE.

The next stage is Stage 5C-C BLE Configuration & Safe Commands. Calibration,
factory reset, OTA, AT, FF12, and Runtime Drift control remain excluded until
Stage 5C-D.

Stage 5C-C is implemented on branch `stage5c-ble-c` from main merge
`dc7c17bba828d0cdda6635545b482fd4914519ee`. It adds V1 `0x80`/`0x81`
request/response frames, bounded parser processing, transaction duplicate
protection, response priority, shared CommandService routing, and a shared
BLE/Modbus config-edit owner.

Stage 5C-C hardware validation is complete at branch HEAD
`3a06bbf8d51fe75dbb89e95b6162b2f1f03d90a6`. Device/config reads,
TARE/CLEAR TARE, ZERO/RESET ZERO, staging/validation/APPLY/discard, invalid
CAP/OL rejection, SAVE completion, duplicate protection, and power-cycle
restore passed. BLE and Modbus rejected competing config owners in both
directions, and runtime/config state changes were observed across transports.

The final 600 s concurrent run received 3613 BLE frames (FAST 3011, SLOW 602)
and 24/24 command responses with no CRC error, sequence gap, duplicate,
timestamp anomaly, parser resync, partial byte, retry, timeout, transaction
mismatch, or disconnect. RS485 completed 1419/1419 FC03 requests with no
timeout, CRC error, exception, or retry failure. MCU telemetry counters showed
4596/4596 cumulative generated/sent frames and zero queue, readiness, encode,
FAST, or SLOW drop. An earlier isolated missing-sync-byte event is documented
in `STAGE5C_C_BLE_COMMANDS.md`; it did not reproduce in the strict rerun.

Stage 5C-C status: SOFTWARE COMPLETE; BLE COMMAND HARDWARE TESTED; TRANSPORT
INTEROPERABILITY TESTED; COMPLETE. The frozen Modbus map still has no direct OL
active-config register, so OL cross-checking uses the BLE read plus the shared
validator. Calibration remains reserved for Stage 5C-D.

## Stage 5C-D

Branch: `stage5c-ble-d`, based on the Stage 5C-C main merge
`04d498527e9aaa7434f20f72ef9dfd4fea3a3dd7` and tag
`stage5c-c-hw-tested`.

The BLE two-point calibration workflow is software complete. A single
transport-neutral CommandService session now owns local UI, Modbus, or BLE
calibration, locks the conversion-critical configuration, requires the existing
8-sample/50-count/500-ms stable window, and uses the existing
CalibrationModel, ConfigApplication and PersistenceManager paths. BLE V1 adds
GET STATE `0x30`, BEGIN `0x31`, SET MASS `0x32`, CAPTURE ZERO `0x33`, CAPTURE
LOAD `0x34`, APPLY `0x35`, and CANCEL `0x36`; mutating requests carry a 16-bit
session ID and retain the existing transaction replay protection. APPLY and
SAVE both require explicit operator confirmation in the PC workflow.

Stage 5C-D hardware validation is complete. The original slot B sequence 10
calibration and both slot hashes were preserved before testing. GET STATE,
BEGIN, BLE/local/Modbus ownership, valid/invalid mass, invalid order, stable
ZERO/LOAD, exact duplicate capture/APPLY/SAVE, CANCEL rollback, 120 s timeout,
explicit APPLY, dirty semantics, SAVE and power-cycle restore all passed. The
saved calibration uses zero raw `-43532`, 500 g load raw `-486139`, and span
`-442607`; SAVE switched once to slot A sequence 11. After reboot, empty read
0.01 g, the 500 g reference read 500.07 g, and an approximately 1 g check read
1.04 g. Runtime Drift remained disabled. APPLY-without-SAVE power-cycle is
explicitly NOT HARDWARE TESTED; Host coverage verifies that reboot behavior.

The final post-calibration 600 s concurrency run passed: BLE received 3612
frames (3010 FAST, 602 SLOW), returned 28/28 commands including four successful
BEGIN/CANCEL sessions, and had zero disconnect, timeout, retry, CRC error,
sequence gap, duplicate, resync or partial byte. RS485 completed 1049/1049
read-only polling cycles with zero exception, mean 57.10 ms and P99 58.99 ms.
The final SWD snapshot showed 5259/5259 valid FC03 responses and zero IDLE queue
overflow, DMA/UART, RTU, CRC, TX or protocol errors; all transport states were
idle. Final calibration state was IDLE/NONE, slot A sequence 11, dirty zero.

Final clean gates pass: MSVC 19.43.34808 CTest 12/12, Stage 5B Python 28/28,
Stage 5C Python 9/9, Debug 126,748 B Flash / 14,640 B RAM, Release 68,928 B /
14,648 B, and BoardDiagnostics 125,268 B / 14,640 B. Release ends at
`0x08010D40`, below `0x0801F000`. Schema V2 remains 344 B; slots A/B, `.ioc`,
Modbus map `0x0102`, BLE V1 and UART settings are unchanged.

Stage 5C-D status: **SOFTWARE COMPLETE; CALIBRATION HARDWARE TESTED;
PERSISTENCE TESTED; OWNERSHIP TESTED; CONCURRENCY REGRESSION TESTED;
COMPLETE**. The next recommended phase is Stage 6 system qualification and
metrology validation: long-term zero/500 g drift, multiple load points,
repeatability, return-to-zero, eccentric loading and temperature behavior.

## Stage 5A

Stage 5A HF2-R1 hardware regression is merged to `main` at
`167f4f43290dcfceccd7e510965b5e0b9445ef9` and tagged
`stage5a-hf2-r1-hw-tested` (`9d698a79d7cae1c92c74cfdaf6556b9e93ed8f6b`).

The persistent-dirty audit is recorded in
`Docs/STAGE5A_CLOSEOUT_CONFIG_DIRTY.md`: Schema V2 persists tare mass, TARE
and CLEAR TARE intentionally dirty the revision, and ZERO/RESET ZERO are
currently RAM-only zero-offset operations.

## Stage 5C-A

Branch: `stage5c-ble-a`.

This stage adds the nonblocking W02 UART transport and observable connection
manager on the existing USART1 resource. USART3 is not present in the current
CubeMX project, so no USART3 claim or `.ioc` change was made. W02 module model,
AT protocol, BLE service/characteristic UUIDs, and true phone-link reporting
remain unknown and explicitly unimplemented.

Historical branch note: the transport implementation was host verified before
the H2 hardware closeout. The current closeout results are recorded in
`Docs/STAGE5C_A_BLE_TRANSPORT.md` and frozen on `main`.

H2 update: phone writes through FFE2 have been received by USART1 and the
transport ring (`ABC123`, six bytes, no overflow). A BoardDiagnostics-only
one-shot `Hello W02` TX trigger was used for FFE1 Notify validation.
Reverse-direction, bidirectional, link-observation and BLE/RS485 concurrency
results are now recorded in the Stage 5C-A closeout document; the known RS485
transient is a physical-layer limitation.

H2 build snapshot: Debug 112,692 B / 12,312 B, Release 61,184 B / 12,320 B,
and BoardDiagnostics 110,812 B / 12,312 B (Flash / RAM). Release load ends at
`0x0800EEFF`; Schema V2 and config slot addresses are unchanged.

Post-change ARM resource snapshot:

- Debug: Flash 112,412 B, RAM 12,256 B.
- Release: Flash 61,000 B, RAM 12,264 B.
- BoardDiagnostics: Flash 110,172 B, RAM 12,256 B.
- Release load end is `0x0800EE47`, below `0x0801F000`; Schema V2 is 344 B and the existing
  config slots remain A `0x0801F000-0x0801F7FF`, B `0x0801F800-0x0801FFFF`.
