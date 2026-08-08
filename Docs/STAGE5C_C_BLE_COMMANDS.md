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

Current status: `STAGE 5C-C SOFTWARE COMPLETE; NOT TESTED ON HARDWARE`.
