# Project stage status

## Stage 5C-A Closeout

Stage 5C-A BLE transport hardware and BLE/RS485 concurrency validation is
merged to `main` at `2eac9f0bf9b137f031bf91c1791d434436ca6020` and frozen by
annotated tag `stage5c-a-hw-tested`.

## Stage 5C-B

Branch: `stage5c-ble-b`.

Read-only realtime telemetry software is implemented: version 1 FAST_WEIGHT
and SLOW_STATUS frames, explicit little-endian serialization, shared CRC16,
stream parser, and non-blocking latest-data-wins scheduling. Final closeout
HEAD is `a454555bbf7ed0a20be5213360fafd022f97d72f`. The final Release image is
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
