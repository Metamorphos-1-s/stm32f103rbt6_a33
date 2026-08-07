# Project stage status

## Stage 5C-A Closeout

Stage 5C-A BLE transport hardware and BLE/RS485 concurrency validation is
merged to `main` at `2eac9f0bf9b137f031bf91c1791d434436ca6020` and frozen by
annotated tag `stage5c-a-hw-tested`.

## Stage 5C-B

Branch: `stage5c-ble-b`.

Read-only realtime telemetry software is implemented: version 1 FAST_WEIGHT
and SLOW_STATUS frames, explicit little-endian serialization, shared CRC16,
stream parser, and non-blocking latest-data-wins scheduling. Host C tests are
10/10 and Python parser tests are 5/5. Debug, Release, and BoardDiagnostics
images build without new warnings; Release remains below `0x0801F000`.

Hardware telemetry testing has not yet been run on this branch. Do not mark
Stage 5C-B complete or enter Stage 5C-C until empty/500 g/+1 g/TARE/SAVE,
600-second BLE soak, BLE+weighing, and BLE+RS485 checks are recorded.

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
