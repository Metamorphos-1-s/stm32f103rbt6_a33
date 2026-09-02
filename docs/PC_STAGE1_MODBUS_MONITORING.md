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
- Total xUnit: 28/28 passed, 0 failed, 0 skipped, 5.250 s Release run.
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
| Modbus TCP | PARTIAL | 600.040 s and 5346/5346 responses PASS; 10/10 reconnect cycles and WPF live path PASS; network interruption and panel comparison pending |
| RTU RS232 | BLOCKED | COM5 identified as USB-RS232, but SerialDebug interference caused first-I/O abort; clean retest pending |
| RTU RS485 | NOT RUN | CH340 ports detected but physical type/wiring unconfirmed |

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
disconnected and exited without a remaining process.

## Known limitations and Stage 2 gate

Connection-setting persistence remains optional and is not implemented. RTU
cannot cryptographically identify an extremely late response with identical
slave/function/length; serialization plus T3.5/drain behavior is used. Hardware
performance, panel comparison and recovery remain open.

PC Client Stage 1 code is complete, but hardware validation is pending. PC
Client Stage 2 requires TCP hardware PASS plus at least one actual RTU physical
link PASS, including 600-second monitoring, reconnect/recovery and confirmation
that no write function was observed.

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
