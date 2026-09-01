# WeChat Mini Program Stage 1 BLE Monitoring

## Scope and architecture

Stage 1 implements read-only BLE monitoring. Pages bind to `DeviceStore`
through `BleMonitorService`; connection policy lives in
`BleConnectionManager`; WeChat API calls live in `WxBleAdapter`. A single
`BleStreamParser` instance processes each connection. No page constructs BLE
frames or invokes state-changing instrument operations.

Data path:

```text
Devices / Monitor / Diagnostics pages
  -> BleMonitorService / DeviceStore
  -> BleConnectionManager / BleCommandClient / BleWriteQueue
  -> WxBleAdapter
  -> FFE0 (FFE1 Notify, FFE2 Write)
```

## Connection state machine

Observable states are `CLOSED`, `OPENING_ADAPTER`, `ADAPTER_UNAVAILABLE`,
`SCANNING`, `CONNECTING`, `DISCOVERING`, `SUBSCRIBING`, `VERIFYING`, `READY`,
`DEGRADED`, `RECONNECT_WAIT`, `DISCONNECTING`, `ERROR` and `INCOMPATIBLE`.
Scanning is bounded to ten seconds and deduplicates opaque `deviceId` values.
`W02_` names are ranked first but are not the sole acceptance rule. The
current connection generation isolates callbacks from old connections.

The client discovers service FFE0 and verifies that FFE1 supports Notify (or
Indicate) and FFE2 supports Write. The FFE1 listener is installed before
notifications are enabled. A new parser and sequence baseline are created for
each connection.

## Protocol and data freshness

FFE1 notifications are byte-stream chunks. The shared V1 parser handles split
sync/header/payload/CRC, merged frames, garbage, CRC recovery, sequence wrap,
gaps, duplicates and a bounded 512-byte buffer. FAST, SLOW and CHECKWEIGH are
decoded into transport-independent domain values. Raw frame history is not
retained; diagnostic events are capped at 20.

Client display policy:

- FAST older than 1,000 ms is marked stale.
- FAST older than 3,000 ms is hidden rather than presented as live weight.
- SLOW and CHECKWEIGH older than 3,000 ms are marked stale.
- No valid frame for 5,000 ms moves a connected session to `DEGRADED`.
- A later valid frame returns `DEGRADED` to `READY`.

A sequence gap or resynchronization updates visible diagnostics but does not
disconnect, erase the last valid sample or synthesize missing samples.

## Read-only commands

Only GET_DEVICE_INFO (`0x01`) and GET_ACTIVE_CONFIG (`0x02`) pass the command
allowlist. FFE2 writes are serialized and split into 20-byte chunks. One
request is in flight; transaction ID and operation must both match. A timed
out read is retried once using byte-identical request data and the same
nonzero transaction ID. Connection changes cancel pending writes and requests.

GET_DEVICE_INFO gates READY on protocol 1, firmware `0x050A`, schema 2 and map
`0x0104`. GET_ACTIVE_CONFIG requires the frozen 55-byte prefix and accepts the
defined optional tails. No TARE, ZERO, configuration mutation, SAVE,
calibration or factory-reset path is exposed.

## Pages

- Devices: adapter state, scan controls, deduplicated devices, RSSI, last-seen
  age and connection action.
- Monitor: display weight, unit, stable/stale state, NET/GROSS/TARE, lock,
  overload, raw values, Runtime Drift, checkweigh lamps, dirty and fault mask.
- Diagnostics: GATT discovery, device versions, RX/frame/command counters,
  gap/duplicate/CRC/resync, copy summary, clear statistics and disconnect.

## Verification

Automated TypeScript gate: 19/19 tests passed. It includes Stage 0 CRC,
golden-frame splitting and signed-int64 coverage plus Stage 1 Mock adapter,
state-machine, old-callback isolation, write queue, read-only command,
byte-identical retry, stale-data and bounded-diagnostic tests.

WeChat Developer Tools `2.02.0` uses base library `3.16.2`. Simulator BLE
unavailability is handled without a blank screen or unhandled rejection. The
two DevTools-internal preload advisories documented by Stage 0-V are not
application warnings.

## Hardware validation

| Check | Android | iPhone |
| --- | --- | --- |
| Device/model, OS, WeChat version | NOT RUN | NOT RUN |
| Permission and Bluetooth-off paths | NOT RUN | NOT RUN |
| W02 discovery and GATT characteristics | NOT RUN | NOT RUN |
| GET_DEVICE_INFO / GET_ACTIVE_CONFIG | NOT RUN | NOT RUN |
| FAST / SLOW / CHECKWEIGH | NOT RUN | NOT RUN |
| 600-second read-only monitoring | NOT RUN | NOT RUN |

Hardware results must be recorded without removing gaps, resync events or
failed attempts. Simulator results are not hardware evidence.

## Known limitations and security

The MCU cannot provide a reliable BLE disconnect indication through W02, and
mobile platforms may use different notification fragmentation and opaque
device identifiers. Reconnect is bounded to three attempts. BLE remains an
auxiliary monitoring interface.

`COMPROMISED — rotation pending`

The compromised AppSecret must be rotated and the previous value invalidated
before preview distribution, upload, release or use of any server API. No
AppSecret, token, cloud API, upload or release operation is present here.
