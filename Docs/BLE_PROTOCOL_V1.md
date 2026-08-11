# BLE Telemetry Protocol V1

The V1 stream runs from STM32 to W02 USART1 at 9600 8N1 and then FFE0/FFE1
Notify. FFE2 carries framed command requests. FFE1 Notify and FFE2 write
boundaries are not frame boundaries; clients must use stream parsers.

## Common frame

All multi-byte values are little-endian. CRC is CRC-16/Modbus (initial value
`0xFFFF`, reflected polynomial `0xA001`) over offsets `0` through the final
payload byte. The CRC itself is little-endian.

| Offset | Size | Type | Name | Description |
|---:|---:|---|---|---|
| 0 | 2 | u8[2] | sync | `A5 5A` |
| 2 | 1 | u8 | version | `0x01` |
| 3 | 1 | u8 | message_type | `0x01` FAST, `0x02` SLOW, `0x03` CHECKWEIGH, `0x80` REQUEST, `0x81` RESPONSE |
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

The scheduled application bandwidth is `56*5 + 73*1 + 22*1 = 375 byte/s`, or 39.1%
of the 960 byte/s 8N1 budget. The largest frame is 73 bytes, below the 255-byte
effective TX ring capacity. A full queue drops the entire newest frame; no
blocking, partial writes, or stale-frame retries are used.

## CHECKWEIGH_STATUS (0x03)

Exact size is 22 bytes (`12 + 8 + 2`), sent every 1000 ms (1 Hz). It reports
live state only; thresholds remain available through GET_ACTIVE_CONFIG.

| Payload offset | Size | Type | Name |
|---:|---:|---|---|
| 0 | 1 | u8 | state: 0 disabled, 1 low, 2 OK, 3 high, 4 overload, 5 fault |
| 1 | 1 | u8 | flags |
| 2 | 1 | u8 | weight_source: 0 NET, 1 GROSS |
| 3 | 1 | u8 | reserved, zero |
| 4 | 4 | u32 | config_revision |

Flags are bit0 limit enabled, bit1 current WeightSnapshot stable, bit2 logical
alarm active, bit3 green, bit4 yellow, bit5 red, bit6 internal buzzer GPIO and
bit7 external buzzer GPIO. `alarm_active` remains set throughout both 250 ms
alarm phases; the two buzzer flags report actual GPIO phase and may be clear.
Lamp and buzzer flags come from AlarmOutputManager diagnostics, not a telemetry
recalculation. A retained qualified state with `stable=0` is valid.

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
`DISCARD_CONFIG (0x26)`, `SAVE_CONFIG (0x27)`, `SET_CONFIG_FIELD (0x28)`, and the calibration operations
`0x30..0x36` defined below. Sample rate and gain remain read-only. Factory
reset, communication apply, OTA, AT, FF12 and Runtime Drift control are
unsupported.

`GET_DEVICE_INFO` response data is 12 bytes:

| Offset | Size | Type | Name |
|---:|---:|---|---|
| 0 | 1 | u8 | protocol_version (`1`) |
| 1 | 1 | u8 | firmware_version high byte (`5`) |
| 2 | 1 | u8 | firmware_version low byte (`10`) |
| 3 | 1 | u8 | reserved (`0`) |
| 4 | 2 | u16 | schema_version (`2`) |
| 6 | 2 | u16 | register_map_version (`0x0103`) |
| 8 | 4 | u32 | capability bits |

The firmware bytes combine as `(high << 8) | low`, currently `0x050A`.

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

`GET_ACTIVE_CONFIG` remains prefix compatible. The original 55-byte prefix and
all offsets are frozen. An optional 29-byte AlarmConfig tail extends new
responses to 84 bytes. Parsers must accept at least 55 bytes, parse the old
prefix first, and parse the tail only when 84 or more bytes are present.

| Tail offset | Absolute offset | Size | Type | Name |
|---:|---:|---:|---|---|
| 0 | 55 | 1 | bool | limit_enable |
| 1 | 56 | 1 | u8 enum | weight_source: 0 NET, 1 GROSS |
| 2 | 57 | 1 | bool | internal_buzzer_enable |
| 3 | 58 | 1 | bool | external_buzzer_enable |
| 4 | 59 | 1 | bool | qualified_beep_enable |
| 5 | 60 | 8 | i64 ug | lower_limit_ug |
| 13 | 68 | 8 | i64 ug | upper_limit_ug |
| 21 | 76 | 8 | i64 ug | hysteresis_ug |

`SET_CONFIG_MASS (0x21)` keeps its `field u8 + value i64` layout and exposes
mass field IDs 3 lower, 4 upper and 5 hysteresis. `SET_CONFIG_FIELD (0x28)` uses
`field u8 + value i32`; exposed scalar IDs are 15 limit enable, 16 source,
17 internal buzzer, 18 external buzzer and 19 qualified beep. These allowlists
do not expose other internal ConfigField values. All requests operate on the
same ConfigEdit staging copy and use the existing validate/apply/replay/conflict
semantics. Classification fields reset old qualification after successful
Apply; the three buzzer fields take effect without resetting classification.

## Calibration workflow

Calibration uses the existing two-point `CalibrationModel`, MetrologyManager
apply path and ordinary `SAVE_CONFIG`; the BLE layer never calculates a scale
or writes Flash. All mutating calibration requests after BEGIN carry the
little-endian `session_id` returned by the MCU. A stale or mismatched session
ID returns `INVALID_STATE`.

| Operation | ID | Request data | Allowed state | Success transition | Side effect | Persistent effect |
|---|---:|---|---|---|---|---|
| GET_CALIBRATION_STATE | `0x30` | none | any | none | read-only snapshot | none |
| BEGIN_CALIBRATION | `0x31` | none | IDLE/APPLIED, no owner/config edit | WAIT_ZERO_STABLE | locks unit/display/capacity/rate/gain, owner=BLE | none |
| SET_CALIBRATION_MASS | `0x32` | `u16 session_id, i64 mass_ug` | active BLE session before RESULT_READY | zero captured: WAIT_LOAD_STABLE; otherwise unchanged | stages checked `MassValueUg` | none |
| CAPTURE_CALIBRATION_ZERO | `0x33` | `u16 session_id` | ZERO_READY | ZERO_CAPTURED or WAIT_LOAD_STABLE | stores the current stable 8-sample filtered-raw average | none |
| CAPTURE_CALIBRATION_LOAD | `0x34` | `u16 session_id` | LOAD_READY, zero and mass present | RESULT_READY | stores the stable average and invokes `CalibrationModel_BuildMass` | none |
| APPLY_CALIBRATION | `0x35` | `u16 session_id` | RESULT_READY | APPLIED, owner released | atomically applies candidate to RAM | sets `persistent_dirty=1` |
| CANCEL_CALIBRATION | `0x36` | `u16 session_id` | active session owned by caller | IDLE, owner released | discards session/candidate; active calibration unchanged | none |

There is no separate VALIDATE or DISCARD operation: load capture already runs
the authoritative model validation, and the existing business model has one
safe CANCEL action. APPLY never implies SAVE. `SAVE_CONFIG (0x27)` remains the
only Flash operation and clears dirty only after persistence completion.

Calibration states are `0 IDLE`, `1 WAIT_ZERO_STABLE`, `2 ZERO_READY`,
`3 ZERO_CAPTURED`, `4 WAIT_LOAD_STABLE`, `5 LOAD_READY`, `6 RESULT_READY`,
`7 APPLIED`, and `8 FAILED`. Owners are `0 NONE`, `1 LOCAL_UI`, `2 MODBUS`, and
`3 BLE`. An active session rejects a second owner with `BUSY`; read-only
telemetry, GET_DEVICE_INFO, GET_ACTIVE_CONFIG and Modbus FC03 continue.

Every calibration response contains the 44-byte state snapshot below, including
error responses when a valid operation could be decoded:

| Offset | Size | Type | Name |
|---:|---:|---|---|
| 0 | 1 | u8 | state |
| 1 | 1 | u8 | owner |
| 2 | 2 | u16 | session_id |
| 4 | 1 | u8 | locked_unit; current active unit while IDLE |
| 5 | 1 | u8 | locked_decimal_places; current value while IDLE |
| 6 | 1 | u8 | locked_division_digit; current value while IDLE |
| 7 | 1 | u8 | flags: active, stable, zero, load, candidate, result, dirty, active calibration valid |
| 8 | 8 | i64 | calibration_mass_ug |
| 16 | 8 | i64 | locked_capacity_ug; current capacity while IDLE |
| 24 | 4 | i32 | zero_raw |
| 28 | 4 | i32 | load_raw |
| 32 | 4 | i32 | span_raw (`load-zero`) |
| 36 | 4 | u32 | captured sample_sequence |
| 40 | 1 | u8 | raw stability sample_count |
| 41 | 1 | u8 | last underlying CommandResult |
| 42 | 2 | u16 | reserved, zero |

The raw window is 8 filtered samples, spread at most 50 counts, held for 500 ms.
If it is not ready, capture returns `NOT_STABLE`; no sample is taken. Mass must
be positive, not exceed locked capacity, and round-trip exactly through the
locked unit/decimal/division representation. The session expires after 120 s
without a valid mutating calibration command. GET state and telemetry do not
refresh this timer.

All operations use the existing four-entry transaction cache. Exact retry of
BEGIN, SET MASS, either CAPTURE, APPLY, CANCEL or SAVE replays the original
response and does not repeat sampling, apply or Flash programming. The largest
calibration request is 24 bytes on UART (`14 + 10`); a state response is 66
bytes (`14 + 8 + 44`). Protocol maxima remain 142-byte request and 110-byte
response frames. A response burst remains below the existing 255-byte priority
TX capacity and does not reduce FAST 5 Hz or SLOW 1 Hz scheduling.

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
