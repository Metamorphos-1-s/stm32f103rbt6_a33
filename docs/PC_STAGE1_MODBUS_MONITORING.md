# PC Client Stage 1 Modbus Monitoring

## Scope and baselines

PC Client Stage 1 is a read-only Modbus TCP/RTU monitor. Runtime code exposes
FC03 only. FC06/FC16 codecs remain solely for frozen protocol regression tests;
there is no UI, service or repository method capable of invoking them.

- Client input baseline: `89e29ee03bd205860dcec7b8344248ad01be08ad`
- STM32 contract baseline: `9e242c7649ad79ac4c3bae347b3259847f4bc09e`
- CH579 required hardware baseline: `3cd21ecefe6486d28a664ea356cb4ed6e7b3090a`
- Register map: `0x0104`, zero-based PDU addresses, default Unit ID 1

## Architecture

`A33.Instrument.Protocol` contains CRC, RTU/TCP envelope validation, FC03
response parsing, exception preservation, signed register codecs and the
embedded machine-readable register map. It has no WPF, socket or serial-port
dependency.

`A33.Instrument.Core` owns `IModbusTransport`, TCP/RTU transports,
`ReadOnlyModbusClient`, `InstrumentMonitoringService`, decoding, scheduling,
state and bounded diagnostics. The transport contract accepts a unit ID and
raw PDU and returns the validated response PDU plus bounded ADU evidence.

`A33.Instrument.Wpf` is MVVM without a third-party framework. It binds
connection settings and read-only monitoring data to `MainViewModel` and never
constructs protocol bytes or opens a socket/serial port in UI code.

## Transport behavior

TCP uses asynchronous `TcpClient`/`NetworkStream`, reads the six-byte MBAP
prefix exactly, validates Length, reads the declared remainder, then validates
TID, PID and Unit ID. Each request receives an incrementing TID.

RTU uses `SerialPort.BaseStream`, an explicit T3.5-or-longer pre-request quiet
interval, input drain, partial-read assembly based on FC03 byte count or the
five-byte exception format, maximum 256-byte ADU, slave/function/length/CRC
validation and cancellation. RS232 and RS485 share this implementation.

`ReadOnlyModbusClient` serializes requests with a semaphore. A timeout or
cancelled request cannot overlap its successor. TCP TID validation and RTU
quiet/drain handling isolate stale data as far as their respective protocols
permit.

## State and polling

States are DISCONNECTED, CONNECTING, CONNECTED, MONITORING, DEGRADED,
RECONNECTING, FAULTED and DISCONNECTING. Startup remains DISCONNECTED and does
not touch network or serial resources. Connect reads the map version first and
refuses further decoding unless it is `0x0104`; it then reads configured word
order.

The single polling loop performs only FC03:

- 200 ms default: the fully defined contiguous realtime range from
  `display_weight` through `sample_sequence`.
- 1000 ms: config dirty, fault mask, display-condition prefix and the fully
  defined active checkweigh range.
- Once per connection: map version and active word order.

The fast period has a 100 ms lower bound. Polling is sequential and cancelled
on Stop or Disconnect. A recoverable timeout enters DEGRADED without clearing
the last snapshot; a later valid sample restores MONITORING. Transport loss
causes at most three cancellable reconnect attempts. User stop/disconnect never
continues reconnecting.

## Data and diagnostics

Mass truth is signed `Int64` micrograms. Word order is read from the active
configuration. Display conversion and formatting use integer arithmetic only.
The UI hides live weight once data is stale while retaining the timestamp and
last snapshot for diagnostics.

The firmware realtime status word is a special fixed layout: register `0x0004`
contains the low word and `0x0005` the high word, independent of configured
multi-register word order. The decoder applies the stable (`bit 4`), zero
(`bit 5`), tare-active (`bit 6`) and overload (`bit 7`) masks to that low-word
layout. Other signed 32/64-bit fields continue to use the configured word
order.

The final panel display value comes from realtime `0x0000–0x0001`. The slower
display-condition block is used for lock/release diagnostics only and never
overwrites the live panel value; doing so would mix two sampling epochs and can
make a stable 0.01 g panel display flicker with a stale 0.00 g diagnostic value.

Diagnostics expose TX/RX, timeout, CRC, MBAP/TID, Unit ID, exception, bad-frame,
transport and reconnect counts; request/response snapshots are capped at 128
bytes and the event list at 200 entries. Clearing diagnostics is local and
sends no request.

## WPF surface

The connection pane supports TCP host/port and RTU COM/baud/parity/stop bits,
Unit ID, connect/request timeout and poll period. Controls cover connect,
disconnect, start/stop monitoring and COM refresh. Monitor and Diagnostics tabs
show exact mass/status, map, endpoint, stale state and bounded protocol evidence.
Inputs are disabled while connected. The only Stage 2 text is an unavailable
label; there is no write control.

Startup QA confirmed a visible main window, default TCP settings,
DISCONNECTED/Not connected state, no TCP connections, no serial open, no
fabricated status values and normal close with no remaining process. Two
startup defects found during QA (missing StartupUri and read-only Run binding
mode) were fixed before signoff.

## Automated verification

- Protocol: 16/16 passed.
- Core/transport: 12/12 passed.
- Total xUnit: 30/30 passed, 0 failed, 0 skipped (serial occupancy and closed-port classification included).
- Debug build: PASS, 0 warnings, 0 errors.
- Release build: PASS, 0 warnings, 0 errors.
- WeChat TypeScript regression: 23/23 passed.
- Contract validation: 69 definitions, no overlap, PASS.

Coverage includes RTU CRC/exception/slave/function/length, TCP TID/PID/Length/
Unit, split and buffered stream reads, signed values and both word orders, map
gate, single in-flight transaction, FC03-only polling, stop/cancel, timeout
recovery, retained snapshot, three-reconnect limit and bounded diagnostics.

The Stage 0 FC03 zero-response JSON vector contained one excess zero byte. The
firmware and FC03 byte-count rule require four data bytes, so the corrected
valid vector is `01 03 04 00 00 00 00 FA 33`. Both implementations consume the
same corrected JSON contract.

## Hardware validation

| Path | Result | Evidence |
| --- | --- | --- |
| Modbus TCP | PASS | 600.040 s and 5346/5346 responses; 10/10 reconnect cycles, WPF live path, interruption detection/recovery and panel comparison PASS |
| RTU RS232 | PASS | COM5 600.087 s and 5336/5336 responses; 10/10 cycles, occupancy, unplug/replug recovery and panel comparison PASS |
| RTU RS485 | PASS | COM5 USB-RS485: 600.069 s, 5337/5337 responses, zero errors; 10/10 reconnect cycles and panel comparison PASS |

TCP hardware FC03 monitoring was executed; no write function was observed.
The COM5 RTU probe sent one FC03 request but received no response before the
I/O-aborted port failure; no blind retries were sent. See
`Results/pc_stage1_hw/environment.md`.

The TCP window ran from 2026-09-02 19:52:01 to 20:02:02 CST for 600.040
seconds. It completed 5346/5346 requests/responses with zero timeout, MBAP/TID,
Unit ID, exception, bad-frame, transport or reconnect errors. Maximum sampled
data age was 288.534 ms. Map `0x0104` and HighWordFirst were read from the
device. Ten subsequent connect/monitor/disconnect cycles passed 10/10. The WPF
path entered MONITORING, displayed live g values and Map `0x0104`, then stopped,
disconnected and exited without a remaining process. A quick cable-only replug
test returned the same process to MONITORING with 1035/1034 responses, two
reconnects, one timeout, one transport error, zero CRC/MBAP/Unit/bad frames and
maximum stale time 4770.364 ms.

COM5 was identified as USB-RS232 with CH340 `VID_1A86&PID_7523`. A firmware
Python FC03 cross-check first confirmed Map `0x0104`. The original .NET
BaseStream async receive produced a CH340 I/O-aborted error, so the serial path
was changed to a background short-timeout `SerialPort.Read` loop that remains
asynchronous to WPF and checks cancellation at 50 ms boundaries. The corrected
path ran from 2026-09-03 00:53:51 to 01:03:51 CST for 600.087 seconds and
completed 5336/5336 responses. Timeout, CRC, Unit ID, Modbus exception,
bad-frame and transport errors were zero; maximum data age was 242.700 ms and
no write function was observed. Ten subsequent connect/monitor/disconnect
cycles passed 10/10 (110/110 responses). Controlled port occupancy produced a
clear UnauthorizedAccessException with no request sent or application crash.
The controlled TCP interruption probe included a full instrument/CH579 power
loss. It stopped reception, retained the last snapshot as stale, made exactly
three bounded reconnect attempts and entered FAULTED without crashing. It
recorded 3 timeouts and 1 transport error with no CRC/MBAP/Unit/bad frames.
Because the target remained unreachable afterwards, cable-only same-process
network recovery was initially pending. In the later cable-only test the cable was
reinserted after the three bounded reconnect attempts, so the same process was
already FAULTED. TCP was reachable again immediately afterwards. After CH579
was powered back on, a new client reconnect passed Map `0x0104` and
completed 91/91 responses over 10.097 seconds with zero error.

## Known limitations and Stage 2 gate

Connection-setting persistence remains optional and is not implemented. RTU
cannot cryptographically identify an extremely late response with identical
slave/function/length; serialization plus T3.5/drain behavior is used. The
hardware stage is complete for TCP, RS232 and RS485; iOS/other PC environments
remain outside this validation.

PC Client Stage 1 code and TCP/RS232/RS485 communication gates are complete.
WPF and instrument-panel data comparison is also complete. This stage is
ready for PC Client Stage 2.

## Modified files and Git

- `contracts/modbus-v0104/`: expanded field-level map and corrected FC03 vector.
- `apps/pc/src/A33.Instrument.Protocol/`: strict TCP/RTU codecs, exceptions and
  JSON-driven register map; runtime transports removed from this layer.
- `apps/pc/src/A33.Instrument.Core/`: TCP/RTU transports, FC03-only client,
  monitoring state machine, decoders and diagnostics.
- `apps/pc/src/A33.Instrument.Wpf/`: MVVM commands/view model and operational
  Monitor/Diagnostics interface.
- `apps/pc/tests/A33.Instrument.Protocol.Tests/`: Protocol and Core transport
  tests.
- `Results/pc_stage1_hw/`: bounded hardware environment and NOT RUN templates.
- `README.md`, `docs/CLIENT_ARCHITECTURE.md`,
  `docs/PROTOCOL_TRACEABILITY.md`: stage and traceability updates.

Functional commit subject: `PC Client Stage 1: add Modbus TCP and RTU monitoring`.
