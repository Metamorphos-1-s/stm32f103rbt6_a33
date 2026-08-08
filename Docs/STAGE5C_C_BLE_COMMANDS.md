# Stage 5C-C BLE Configuration and Safe Commands

Stage 5C-C is implemented on branch `stage5c-ble-c`, based on the Stage 5C-B
hardware-tested main merge `dc7c17bba828d0cdda6635545b482fd4914519ee` and tag
`stage5c-b-hw-tested`.

## Scope

This stage adds a reliable V1 command request/response path on the existing
FFE2 byte stream. It supports read-only device/config queries and safe runtime
commands through the existing `CommandService`. It does not change the frozen
FAST/SLOW telemetry payloads, `.ioc`, USART1 9600 8N1, Modbus map `0x0102`,
CS1237 behavior, display conditioning or Flash A/B layout.

Supported operations are `GET_DEVICE_INFO`, `GET_ACTIVE_CONFIG`, `TARE`,
`CLEAR_TARE`, `ZERO`, `RESET_ZERO`, `SET_WEIGHT_VIEW`, `SET_DISPLAY_UNIT`,
`BEGIN_CONFIG_EDIT`, `SET_CONFIG_MASS`, `SET_UNIT_DISPLAY`,
`SET_PROFILE_FIELD` (filter/stability fields only), `VALIDATE_CONFIG`,
`APPLY_CONFIG`, `DISCARD_CONFIG`, and `SAVE_CONFIG`. Calibration, factory
reset, communication apply, OTA, AT, FF12 and Runtime Drift control are
explicitly unsupported.

## Protocol

The common V1 envelope is unchanged. New message types are `0x80` REQUEST and
`0x81` RESPONSE. Request payload is
`transaction_id:u16, operation:u8, flags:u8, data_length:u16, data[]`; response
payload is `transaction_id:u16, operation:u8, result:u8, detail_code:u16,
data_length:u16, data[]`. All fields are explicitly serialized little-endian.
Request payload is at most 128 bytes; response payload is at most 96 bytes.
CRC, malformed frame handling and resynchronization follow
[BLE_PROTOCOL_V1.md](BLE_PROTOCOL_V1.md).

## Transactions and queues

`transaction_id` is a business identifier and is not replaced by the transport
`frame_sequence`. Four fixed cache entries retain the request key and complete
response. Exact retries replay the cached frame; the same ID with a different
operation/payload returns `TRANSACTION_CONFLICT`. A pending SAVE occupies its
cache entry and is completed by the persistence event, so a retry cannot start
another Flash write.

Responses use a fixed-depth four-entry transport priority queue ahead of the
normal telemetry FIFO. The application parser consumes at most 64 bytes and
dispatches at most two frames per `App_Run()`. Queue exhaustion drops no
command state and is retried by the client using the same transaction ID;
telemetry remains non-blocking and may drop a newest frame when its normal FIFO
is full.

At 9600 8N1, the existing telemetry load remains 353 bytes/s. The largest
request is 142 bytes on the wire and the largest response is 110 bytes; command
traffic is low-frequency. A burst of four maximum responses is bounded to 440
bytes of dedicated static storage.

## Business and persistence semantics

All side effects call `CommandService_Execute` with `COMMAND_SOURCE_BLE`.
TARE/CLEAR TARE and ZERO/RESET ZERO therefore retain the same stability,
calibration, tare-active, zero-range, overload and zero-disabled results as
local keys and Modbus. TARE/CLEAR TARE keep the existing dirty-state behavior;
ZERO/RESET ZERO remain RAM-only.

Configuration edits use `ConfigEdit` staging and `ConfigApplication` validation.
There is one volatile staging owner shared with Modbus. An active edit is never
modified by a competing transport. Failed validation leaves active config
unchanged; APPLY updates active RAM and sets `persistent_dirty`; SAVE delegates
to `PersistenceManager` and returns the final event result. No BLE path accesses
HAL Flash directly.

## Verification

Host CTest covers the V1 codec/parser, split and merged frames, garbage and CRC
recovery, response queue priority/boundedness, duplicate command protection,
transaction conflict, SAVE completion and queue-full retry. Firmware Debug,
Release and BoardDiagnostics builds are required before hardware validation.

## Hardware validation

Release firmware from branch HEAD `3a06bbf8d51fe75dbb89e95b6162b2f1f03d90a6`
was flashed and verified with ST-Link. The device reported protocol V1, firmware
`0x050A`, register map `0x0102`, Schema V2, and all expected capability bits.
`GET_ACTIVE_CONFIG` agreed with the frozen Modbus view for every field exposed
by both transports: unit, decimal places, division, capacity, zero range,
profile, filter, stability, sample rate, and gain.

The final clean builds used 121724 B Flash / 14264 B RAM for Debug, 66296 B /
14280 B for Release, and 120244 B / 14272 B for BoardDiagnostics. The Release
load ends at `0x080102F8`, below the config-slot boundary `0x0801F000`. Host
CTest passed 12/12 and the Stage 5C Python protocol/parser suite passed 8/8.

The following command paths passed on the physical instrument:

- TARE at approximately 500 g, exact duplicate replay, CLEAR TARE, and the
  tare-active ZERO rejection path.
- Empty-platform ZERO, exact duplicate replay, and RESET ZERO. ZERO/RESET ZERO
  remained RAM-only and did not advance the persistent revision.
- Decimal-place staging, read-before-apply isolation, validation, APPLY, and
  restoration to the original value. Invalid OL values above CAP and below CAP
  were rejected without changing active configuration.
- SAVE completion after the Flash event, exact duplicate SAVE without a second
  slot write, `persistent_dirty` transition to zero, and power-cycle restore
  from slot A sequence 9.
- Shared config ownership in both directions. Modbus returned `BUSY` while BLE
  owned staging; BLE returned `BUSY` while Modbus owned staging. The owning
  transport could discard/cancel and the state returned to IDLE unchanged.
- Cross-transport state in both directions. BLE TARE/config changes were visible
  through Modbus. Modbus ZERO produced 25 consecutive BLE FAST frames at exactly
  zero; Modbus RESET ZERO restored approximately 0.325 g in another 25 FAST
  frames. Both captures had zero CRC error, gap, resync, or disconnect.

The final 600 s command/telemetry/RS485 concurrency rerun passed strictly:

- BLE: 3613 telemetry frames (FAST 3011, SLOW 602), 24/24 command responses,
  12 device-info and 12 config reads, and zero timeout, retry, result error,
  transaction mismatch, CRC error, sequence gap, duplicate, timestamp anomaly,
  parser resync, partial byte, or disconnect.
- RS485: 1419/1419 FC03 requests, zero timeout, CRC error, exception, or retry
  failure; the measurement sequence advanced by 6115.
- MCU counters advanced from 732/732 to 4596/4596 generated/sent frames. Queue
  full drops, transport-not-ready drops, FAST/SLOW drops, encode errors, UART
  errors, TX errors, RX overflow, and priority-queue-full remained zero.

An earlier 600 s run received 3611 valid telemetry frames and all 24 command
responses without CRC error or disconnect, but missed frame sequence 2190 and
resynchronized across 55 bytes. That byte count is one 56-byte FAST frame minus
its first sync byte. It did not reproduce in the strict rerun, and the MCU
generated/sent/drop counters remained clean, so it is retained as an isolated
W02/Windows receive-path transient rather than hidden or treated as command
frame interleaving.

## Known limitations

The frozen Modbus map `0x0102` does not expose OL as a direct active-config
register. Consequently OL cannot be compared by two simultaneous read APIs;
it was verified through BLE active-config reads plus the shared CAP/OL
cross-field validator. The W02 UART remains 9600 8N1 without hardware flow
control, and the MCU still cannot observe the phone/PC BLE connection state.
Calibration, factory reset, OTA, AT, FF12, and Runtime Drift control remain out
of scope.

Current status: `STAGE 5C-C SOFTWARE COMPLETE; BLE COMMAND HARDWARE TESTED;
TRANSPORT INTEROPERABILITY TESTED; COMPLETE`.
