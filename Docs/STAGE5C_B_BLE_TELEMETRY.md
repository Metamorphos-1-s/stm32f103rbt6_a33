# Stage 5C-B Read-only Realtime Weight Telemetry

Software implementation is complete on branch `stage5c-ble-b`, based on
Stage 5C-A closeout main merge `2eac9f0bf9b137f031bf91c1791d434436ca6020`.
The Stage 5C-A annotated tag is `stage5c-a-hw-tested`.

## Scope and boundary

The firmware sends only read-only telemetry MCU -> USART1 -> W02 -> FFE0/FFE1.
No BLE TARE, ZERO, CLEAR TARE, SAVE, configuration, calibration, OTA, AT, or
FF12 behavior is implemented. FFE2 remains a lower-level transparent test path.

## Implementation

`Protocol/ble/ble_frame_codec.c` performs explicit byte serialization and
`Services/ble_telemetry_service` captures a single public-domain snapshot after
the 20 ms metrology update. FAST_WEIGHT is 56 bytes at 5 Hz and SLOW_STATUS is
73 bytes at 1 Hz. The exact field map is in [BLE_PROTOCOL_V1.md](BLE_PROTOCOL_V1.md).
The scheduler uses latest-data-wins atomic `BleTransport_Write`: a full queue or
non-ready transport drops the whole frame and increments counters.

The MCU never infers BLE connection from UART activity and continues to use the
existing `BLE_LINK_UNKNOWN` semantics. A client reconnects by synchronizing at
the next complete `A5 5A` frame.

## Verification

- C host tests: 10/10 passed, including the new codec/scheduler test.
- Python telemetry parser tests: 5/5 test groups passed.
- Debug: Flash 115804 B, RAM 12568 B.
- Release: Flash 63000 B, RAM 12576 B, application below `0x0801F000`.
- BoardDiagnostics: Flash 114492 B, RAM 12576 B.
- Schema V2 remains 344 B; configuration A/B remain `0x0801F000` and
  `0x0801F800` (2 KiB each).
- No `.ioc`, W02 9600 baud setting, Modbus map, weighing math, display
  conditioner, Runtime Drift algorithm, or Flash layout was changed.

Hardware telemetry validation (empty, 500 g, +1 g, TARE/SAVE, 600 s soak and
BLE+RS485) is still pending on this branch. Stage status is therefore
`STAGE 5C-B SOFTWARE COMPLETE / NOT TESTED ON HARDWARE`.

## Next gate

Flash a Debug or BoardDiagnostics image without mass erase, verify the existing
configuration slots, then run the 60 s empty/500 g/+1 g/TARE checks followed by
the 600 s Release telemetry soak and BLE+RS485 regression. Record MCU telemetry
drop counters beside PC parser counters; do not claim the known RS485 physical
transient issue is fixed.
