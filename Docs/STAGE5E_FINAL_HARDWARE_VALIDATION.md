# Stage 5E Final Hardware Validation

## Decision

Stage 5E checkweigh, alarm outputs, local configuration, Modbus, BLE, storage,
and concurrent operation are hardware qualified. The tested product firmware is
the frozen branch baseline `b27df9af2923697490c382500f7069862a7c16aa`.

Status: **SOFTWARE COMPLETE; HARDWARE COMPLETE; CONCURRENCY TESTED; COMPLETE**.

No product P0 or P1 defect was found. No product source, protocol, persistence
schema, CubeMX configuration, or alarm behavior was changed during hardware
qualification.

## Baseline And Equipment

- Branch: `stage5e-checkweigh-alarm`.
- Initial product SHA: `b27df9af2923697490c382500f7069862a7c16aa`.
- MCU: STM32F103RBT6, 128 KiB Flash.
- Debug probe: ST-LINK V2, serial `E1007200D0D2139393740544`, SWD hot-plug;
  NRST was not connected.
- RS485: CH340 on COM7, slave 1, 115200 baud, 8N1.
- BLE module: W02, address `C8:46:82:00:83:24`.
- Reference load: nominal 500 g plus an approximately 1 g load.
- Release ELF: `build/Release/stm32f103rbt6_a33.elf`.
- Release ELF SHA-256:
  `1670D22F754E806F7B39B69BE4B6ACD7495704E433C98A2B9DB8E8917AD15AEB`.
- Release BIN size: 73,264 B.
- Flash time: 2026-08-10 22:54:55 +08:00.
- STM32CubeProgrammer erase/program/verify and software reset passed.

## Test Configuration

- Unit `g`, decimal places 2, division 1.
- Capacity and overload threshold: 3,000.00 g.
- Limit enabled.
- LOW 499.00 g, HIGH 501.00 g, hysteresis 0.20 g.
- Weight source NET.
- Internal buzzer, external buzzer, and qualified-OK beep enabled.
- Final storage: Schema V2, 344 B payload, active slot B, sequence 16,
  committed CRC-valid A/B slots, persistent dirty 0.

## Checkweigh Results

The normal state and output matrix passed through local observation, Modbus,
and BLE CHECKWEIGH_STATUS `0x03`:

- Below LOW: LOW, yellow, no alarm.
- LOW through HIGH inclusive: OK, green, one qualified entry beep.
- Above HIGH: HIGH, red, periodic internal and external alarm.
- LOW to HIGH and HIGH to LOW direct transitions passed.
- Fast-load stable gating did not generate state chatter or repeated qualified
  beeps. A prior qualified state was retained while unstable; a cleared
  qualification remained DISABLED until the next stable sample.

### Exact Boundaries

Exact hardware equality was captured using a repeatable canonical NET mass and
temporary RAM-only limits with hysteresis zero. The temporary values were never
saved and slot B sequence 16 was restored by power cycle.

- Canonical boundary: `498656822 ug`.
- LOW run: 149 valid synchronized windows, zero mismatch; 4 samples below LOW
  were LOW, 144 samples above LOW were OK, and the exact equality sample was OK.
- HIGH run: 30 valid synchronized windows, zero mismatch; 25 samples above HIGH
  were HIGH, and the exact equality sample was OK.

This proves the frozen comparison semantics: `mass < LOW` is LOW,
`LOW <= mass <= HIGH` is OK, and `mass > HIGH` is HIGH.

### Hysteresis

- LOW hysteresis: after LOW, transition to OK occurred at 499.201323 g for
  LOW 499.00 g and HYS 0.20 g.
- HIGH hysteresis: after HIGH, the state was retained above 500.80 g and
  returned to OK at approximately 500.80 g or below.
- No reversed hysteresis direction or state chatter was observed.

### Slow Ramp

A formal 120.079 s monotonic boundary-crossing run was recorded at 10.14 Hz.
The load ranged from 498.557411 g to 501.095780 g.

- 1,218 synchronized samples; zero inconsistent state windows.
- LOW to OK at 80.532 s and 499.201323 g.
- OK to HIGH at 117.188 s and 501.003147 g.
- Exactly one LOW-to-OK qualified beep and normal HIGH periodic alarm were
  confirmed locally.
- No RGY chatter, duplicate qualified beep, stuck lamp, or stuck alarm.
- The current short-window StabilityDetector remained STABLE for all 1,218
  samples while the mass changed slowly.

The final point is a known P2 characterization item. State classification and
outputs remained correct, so it is deferred to Stage 6 metrology/stability
qualification. LimitChecker does not add a second stability detector.

## Weight Semantics

- NET with a 500 g tare reclassified near zero to LOW and reported source NET.
- GROSS with the same tare retained the 500 g gross classification as OK.
- CLEAR TARE restored NET near gross and requalified on the next stable sample.
- ZERO within range produced ZERO, not TARE, and requalified on the next stable
  snapshot.
- Limit OFF disabled RGY and alarm; Limit ON waited for a new qualification.
- Local display, Modbus, and BLE agreed for NET, GROSS, TARE, CLEAR TARE, ZERO,
  and Limit enable state.

## Outputs And Priority

- Yellow, green, and red physical output mapping passed.
- Qualified OK produced one 100 ms functional beep and did not repeat while OK.
- Qualified beep disable retained green but suppressed the entry beep.
- Internal and external buzzer enables independently controlled their channels
  without changing red.
- HIGH, OVERLOAD, and FAULT alarm logic uses the frozen 250 ms ON / 250 ms OFF
  phase. Logical alarm and phase-active bits were distinguished over Modbus.
- Functional timing was confirmed over multiple audible/observable cycles;
  absolute waveform timing was not instrument verified.

## Overload, Fault, And Calibration

Safe overload testing used temporary RAM-only CAP/OL 500.00 g and a normal
500 g load. OVERLOAD, red, alarm active, and internal/external phase activity
were observed through local output and Modbus. On load removal the state first
became DISABLED while unstable, then requalified LOW when stable. Formal 3 kg
CAP/OL was restored by power cycle.

No destructive physical FAULT injection was performed. Host classification,
FaultManager priority, and the complete red/alarm output chain passed. The
final MCU fault mask was zero. Physical FAULT injection is recorded as NOT
TESTED because no safe production-firmware injection mechanism was available.

Calibration suppressed ordinary LOW/OK/HIGH outputs. OVERLOAD retained higher
priority during a calibration session. Cancel/exit restored normal weighing.

## Local Interface

Local CAP/OL, alarm values, source, output enables, calibration entry/cancel,
SAVE, and return to weighing were exercised. No product P1 issue remained.

Two UI observations are deferred to the next product version: the selected
digit should blink, and numeric digit selection should reach higher positions
so large edits do not require long key repeats. These are P2 usability items
and were explicitly excluded from this frozen qualification build.

## Modbus

- Register map `0x0103` passed and existing addresses did not move.
- Active alarm reads, staging, validation, APPLY, CANCEL, revision, and dirty
  semantics passed.
- FC16 atomic 64-bit mass writes passed; FC06 and partial FC16 mass writes were
  rejected with exception 03.
- Invalid LOW greater than HIGH was rejected without changing active config.
- Illegal function, quantity, address, read-only write, malformed FC16, bad
  CRC, truncation, garbage, and subsequent valid-frame recovery passed.
- Cross-transport ownership returned BUSY in both BLE-owner and Modbus-owner
  directions.

## BLE

- Protocol V1, FAST `0x01`, SLOW `0x02`, CHECKWEIGH_STATUS `0x03`, REQUEST
  `0x80`, and RESPONSE `0x81` passed.
- Device info reported firmware `0x050A`, Schema 2, map `0x0103`, and
  capabilities 1023.
- GET_CONFIG remained 84 B: frozen 55 B prefix plus 29 B alarm tail.
- Valid set/validate/apply, invalid config rejection, replay, transaction
  conflict, cross-transport BUSY, and Modbus/BLE readback consistency passed.
- CHECKWEIGH_STATUS covered LOW, OK, HIGH, DISABLED requalification, NET,
  GROSS, TARE interaction, RGY, alarm logical state, and phase activity.

## Persistence

A real Modbus edit set qualified beep OFF and saved it to slot A sequence 15.
BLE and Flash readback agreed and dirty cleared. The setting was restored ON
and saved to slot B sequence 16. Both records were committed and CRC-valid.
After power cycle, local, Modbus, and BLE restored the formal values, slot B
sequence 16, and dirty zero. Unsaved RAM-only edits reverted on power cycle.

## 600 Second Concurrency

The final Release concurrency run lasted 606.047 s and intentionally exercised
OK, LOW, HIGH, TARE/NET, GROSS, requalification, and return to OK.

BLE results:

- 4,214 total frames: FAST 3,010; SLOW 602; CHECKWEIGH_STATUS 602.
- Five command requests and five successful responses.
- Disconnect, timeout, retry, CRC, gap, duplicate, timestamp anomaly, resync,
  unknown frame, and partial byte: all zero.

RS485 results:

- 2,724/2,724 successful read cycles.
- Timeout, CRC error, exception, and retry: all zero.
- Latency: mean 55.78 ms, median 55.63 ms, P95 57.02 ms, P99 57.53 ms,
  maximum 58.01 ms.

MCU SWD results:

- 13,772 valid addressed FC03 frames and matching responses.
- UART, DMA, IDLE queue, RTU framing, CRC, length, TX, and protocol errors: 0.
- BLE RX overflow, UART/parse/CRC error, transport reset, priority queue full,
  telemetry drop, encode error, and command parser errors: 0.
- CS1237 read error and FIFO overrun: 0.
- Event queue dropped count: 0; fault mask: 0.
- RGY and alarm outputs followed every observed checkweigh state; no output or
  buzzer lockup occurred.

## Build And Compatibility Gates

Prequalification passed Host CTest 15/15, Stage 5B Python 28/28, Stage 5C
Python 12/12, and clean Debug, Release, and BoardDiagnostics builds.

- Debug: 85,340 B Flash, 14,816 B RAM, image end `0x08014D5C`, margin 41,636 B.
- Release: 73,264 B Flash, 14,800 B RAM, image end `0x08011E30`, margin 53,712 B.
- BoardDiagnostics: 121,484 B Flash, 14,704 B RAM, image end `0x0801DA8C`,
  margin 5,492 B.

All images end below configuration slot A at `0x0801F000`. Schema V2/344 B,
Flash slots A/B, Modbus map `0x0103`, BLE Protocol V1, GET_CONFIG layout,
USART1 W02 at 9600, USART2 configuration, Modbus Framer, and `.ioc` are frozen
and unchanged.

## Qualification Tool Correction

The SWD parser initially used stale Release RAM addresses and produced
contradictory counters. The current Release link map was treated as authority;
the dump base/extent and parser addresses were corrected, and existing
CS1237, event queue, BLE transport, BLE telemetry, BLE command, and fault
counters were added to the read-only report. Re-reading live RAM then produced
internally consistent frame counts and zero error counters. This is a test
tool correction only, not a product P0/P1 fix.

## Final Classification

- P0: 0.
- Product P1: 0.
- P2: slow monotonic loading can remain STABLE under the current short-window
  definition; selected-digit blink and higher digit selection are deferred.
- LOW/OK/HIGH, exact boundaries, hysteresis, stable gating, slow ramp,
  NET/GROSS/TARE/ZERO, outputs, safe OVERLOAD, calibration interaction, local,
  Modbus, BLE, persistence, Flash A/B, and 600 s concurrency: hardware tested.
- FAULT: Host/output-chain tested; destructive physical injection not tested.

Stage 5E is complete and may proceed to Stage 6 only after the branch is
merged, rebuilt on `main`, pushed, and tagged `stage5e-hw-tested`.
