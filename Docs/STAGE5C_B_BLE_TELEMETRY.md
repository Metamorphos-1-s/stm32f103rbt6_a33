# Stage 5C-B Read-only Realtime Weight Telemetry

Software implementation is complete on branch `stage5c-ble-b`, at final
closeout commit `a454555bbf7ed0a20be5213360fafd022f97d72f`, based on
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
- Release: Flash 63056 B, RAM 12600 B, application below `0x0801F000`.
- BoardDiagnostics: Flash 114492 B, RAM 12576 B.
- Schema V2 remains 344 B; configuration A/B remain `0x0801F000` and
  `0x0801F800` (2 KiB each).
- No `.ioc`, W02 9600 baud setting, Modbus map, weighing math, display
  conditioner, Runtime Drift algorithm, or Flash layout was changed.

The final Release image reports firmware `0x050A` (1290), register map
`0x0102` (258), active configuration slot B, sequence 8, and
`persistent_dirty=0`.

Hardware closeout results:

- The 600 s BLE + RS485 concurrency run received 3602 BLE frames (FAST 3001,
  SLOW 601), with CRC errors 0, sequence gaps 0, duplicates 0, disconnects 0,
  parser resynchronizations 0, and partial bytes 0.
- The same run completed 2711/2711 read-only FC03 requests, with timeout 0,
  PC CRC errors 0, and Modbus exceptions 0.
- FAST telemetry remained approximately 501.386187 g with stable=1,
  display_locked=1, and overload=0.
- SWD diagnostics recorded 15632 complete frames, 15632 IDLE events, 15632
  T1.5 expiries, and 15632 T3.5 expiries. Framer races and timer-start
  failures were 0. Two DMA wrap-race recoveries occurred without frame loss,
  timeout, or CRC error. UART, DMA, and Modbus error counters were 0.
- An earlier intermittent RS485 timeout was not reproduced after the
  instrumented Release rebuild and long concurrency run. It remains a
  physical/link transient observation; the Modbus Framer algorithm was not
  changed.

Stage status: `STAGE 5C-B SOFTWARE COMPLETE`, `STAGE 5C-B BLE TELEMETRY
HARDWARE TESTED`, `STAGE 5C-B BLE TELEMETRY SOAK TESTED`, `STAGE 5C-B
CONCURRENCY REGRESSION TESTED`, `STAGE 5C-B COMPLETE`.

## Next gate

The next boundary is Stage 5C-C BLE Configuration & Safe Commands. Calibration,
factory reset, OTA, AT, FF12, and Runtime Drift control remain out of scope.
