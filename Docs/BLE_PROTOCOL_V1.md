# BLE Telemetry Protocol V1

Stage 5C-B is a read-only stream from STM32 to W02 USART1 at 9600 8N1, then
FFE0/FFE1 Notify. FFE2 is not used for product commands. FFE1 Notify boundaries
are not frame boundaries; clients must feed all notification bytes to a stream
parser.

## Common frame

All multi-byte values are little-endian. CRC is CRC-16/Modbus (initial value
`0xFFFF`, reflected polynomial `0xA001`) over offsets `0` through the final
payload byte. The CRC itself is little-endian.

| Offset | Size | Type | Name | Description |
|---:|---:|---|---|---|
| 0 | 2 | u8[2] | sync | `A5 5A` |
| 2 | 1 | u8 | version | `0x01` |
| 3 | 1 | u8 | message_type | `0x01` FAST, `0x02` SLOW, `0x80` REQUEST, `0x81` RESPONSE |
| 4 | 2 | u16 | payload_length | Message-specific; request payload <=128 bytes |
| 6 | 2 | u16 | frame_sequence | Increments per frame, wraps |
| 8 | 4 | u32 | timestamp_ms | Device monotonic milliseconds, wraps |
| 12 | N | bytes | payload | Message-specific fields |
| 12+N | 2 | u16 | crc16 | CRC over bytes 0..11+N |

## FAST_WEIGHT (0x01)

Exact size is 56 bytes (`12 + 42 + 2`), sent every 200 ms (5 Hz).
Mass values are signed int64 micrograms; status and display fields are u8.

| Payload offset | Size | Type | Name |
|---:|---:|---|---|
| 0 | 4 | u32 | measurement_sequence |
| 4 | 8 | i64 | display_mass_ug |
| 12 | 8 | i64 | operational_net_mass_ug |
| 20 | 8 | i64 | operational_gross_mass_ug |
| 28 | 8 | i64 | tare_mass_ug |
| 36 | 1 | u8 | stable |
| 37 | 1 | u8 | display_locked |
| 38 | 1 | u8 | overload |
| 39 | 1 | u8 | unit (`0=kg,1=g,2=lb`) |
| 40 | 1 | u8 | decimal_places |
| 41 | 1 | u8 | division |

## SLOW_STATUS (0x02)

Exact size is 73 bytes (`12 + 59 + 2`), sent every 1000 ms (1 Hz).

| Payload offset | Size | Type | Name |
|---:|---:|---|---|
| 0 | 4 | i32 | raw_count |
| 4 | 4 | i32 | filtered_raw |
| 8 | 8 | i64 | uncompensated_gross_mass_ug |
| 16 | 8 | i64 | compensated_gross_mass_ug |
| 24 | 8 | i64 | runtime_drift_offset_ug |
| 32 | 1 | u8 | runtime_drift_enabled |
| 33 | 1 | u8 | runtime_drift_state |
| 34 | 1 | u8 | persistent_dirty |
| 35 | 8 | i64 | capacity_ug |
| 43 | 8 | i64 | overload_threshold_ug |
| 51 | 1 | u8 | filter_mode |
| 52 | 1 | u8 | filter_strength |
| 53 | 1 | u8 | active_profile |
| 54 | 1 | u8 | app_state |
| 55 | 4 | u32 | fault_mask |

The scheduled application bandwidth is `56*5 + 73*1 = 353 byte/s`, or 36.8%
of the 960 byte/s 8N1 budget. The largest frame is 73 bytes, below the 255-byte
effective TX ring capacity. A full queue drops the entire newest frame; no
blocking, partial writes, or stale-frame retries are used.

## COMMAND_REQUEST (0x80) and COMMAND_RESPONSE (0x81)

Command traffic uses the same V1 frame envelope and CRC. It is a byte stream on
FFE2 and is parsed independently of FFE1 notification boundaries. All command
integers are little-endian.

Request payload (`6 + data_length` bytes):

| Offset | Size | Type | Name |
|---:|---:|---|---|
| 0 | 2 | u16 | transaction_id |
| 2 | 1 | u8 | operation |
| 3 | 1 | u8 | flags (reserved, currently zero) |
| 4 | 2 | u16 | data_length |
| 6 | N | bytes | operation data |

Response payload (`8 + data_length` bytes):

| Offset | Size | Type | Name |
|---:|---:|---|---|
| 0 | 2 | u16 | transaction_id (copied from request) |
| 2 | 1 | u8 | operation (copied from request) |
| 3 | 1 | u8 | result |
| 4 | 2 | u16 | detail_code (underlying `CommandResult`, or persistence error) |
| 6 | 2 | u16 | data_length |
| 8 | N | bytes | response data |

Supported operations are `GET_DEVICE_INFO (0x01)`, `GET_ACTIVE_CONFIG (0x02)`,
`TARE (0x10)`, `CLEAR_TARE (0x11)`, `ZERO (0x12)`, `RESET_ZERO (0x13)`,
`SET_WEIGHT_VIEW (0x14)`, `SET_DISPLAY_UNIT (0x15)`, `BEGIN_CONFIG_EDIT
(0x20)`, `SET_CONFIG_MASS (0x21)`, `SET_UNIT_DISPLAY (0x22)`,
`SET_PROFILE_FIELD (0x23)`, `VALIDATE_CONFIG (0x24)`, `APPLY_CONFIG (0x25)`,
`DISCARD_CONFIG (0x26)`, and `SAVE_CONFIG (0x27)`. Sample rate and gain remain
read-only. Calibration, factory reset, communication apply, OTA, AT, FF12 and
Runtime Drift control are unsupported.

Result values are `OK=0`, `INVALID_COMMAND=1`, `INVALID_ARGUMENT=2`,
`INVALID_STATE=3`, `NOT_STABLE=4`, `OUT_OF_RANGE=5`, `TARE_ACTIVE=6`,
`ZERO_DISABLED=7`, `CALIBRATION_INVALID=8`, `OVERLOAD=9`, `BUSY=10`,
`PERSISTENCE_FAILED=11`, `POWER_UNSAFE=12`, `UNSUPPORTED=13`,
`INTERNAL_ERROR=14`, and `TRANSACTION_CONFLICT=15`. CRC failures and malformed
frames have no business response because the transaction cannot be trusted.

The parser accepts split sync/header/payload/CRC, merged frames, garbage and CRC
failure recovery. Application work is bounded to 64 input bytes and two
complete requests per `App_Run()`.

The MCU keeps a four-entry fixed transaction cache. An exact retry replays the
original response without re-executing the command. Reusing an ID with a
different operation or payload returns `TRANSACTION_CONFLICT`. Responses use a
four-entry priority queue in front of telemetry and never block weighing.

Configuration writes use the existing `CommandService`, `ConfigEdit` and
`ConfigApplication` path. Only one volatile config-edit owner is allowed; BLE
and Modbus return `BUSY` when the other transport owns staging. `APPLY_CONFIG`
changes active RAM and sets `persistent_dirty`. `SAVE_CONFIG` responds only
after the existing persistence completion/no-change/failure event.

## Semantics

`display_mass_ug` is the final display-conditioning value. Operational net and
gross are `WeightSnapshot.net_mass_ug` and `gross_mass_ug`; the latter already
includes Runtime Drift compensation. `uncompensated_gross_mass_ug` is taken
before that compensation. `tare_mass_ug` is the active tare. `persistent_dirty`
is the public runtime `config_dirty` state and means a persistent change is not
saved. Runtime Drift is read-only telemetry; Stage 5C-B does not enable or
modify it.

## Client behavior

The parser must resynchronize on `A5 5A`, reject unknown version/type/length,
verify CRC, and retain an incomplete tail. It must support split headers,
payloads and CRCs, merged frames, garbage, CRC failure recovery, sequence gap or
duplicate detection, and timestamp wraparound. The existing `ble_soak.py`
entry point exposes this as `ble_soak.py telemetry`; it subscribes only to FFE1
and writes fast/slow CSV plus a JSON summary.

## Reference frames

For a fixed test vector (`sequence=0x1234`, `timestamp=0x78563412`, FAST
payload fields `1,2,3,4,5,1,1,0,0,2,1`), the exact 56 bytes are:

`a55a01012a00341212345678010000000200000000000000030000000000000004000000000000000500000000000000010100000201e45f`

For SLOW (`sequence=0x1235`, `timestamp=0x78563413`, negative mass values
`-1..-5`, capacity `1000000`, OL `3000000`), the exact 73 bytes are:

`a55a01023b00351213345678fffffffffefffffffdfffffffffffffffcfffffffffffffffbffffffffffffff00000140420f0000000000c0c62d000000000001020005000000007cbc`
