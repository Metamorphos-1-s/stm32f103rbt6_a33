# Stage 5N-A Dynamic/Static Checkweigh Alarm Shadow

## Result

**STAGE 5N-A CHECKWEIGH ALARM SOFTWARE CANDIDATE READY; SHADOW HARDWARE
QUALIFICATION PENDING; ACTIVE OUTPUT REMAINS BLOCKED; D1-C NONZERO-OFFSET
QUALIFICATION DEFERRED; STAGE 5O REMAINS DEFERRED.**

The 0x0515 artifact was application-only flashed after explicit authorization
and device Verify passed. The SHADOW hardware run is a partial pass; active
alarm output work remains blocked.

The stage started at `04d53ba1a596110e835fb33b8877893d1afb0765` on the
frozen D1-C branch and continues on
`stage5na-checkweigh-alarm-offline-shadow`. The exact 0x0514 recovery BIN is
102,656 bytes, SHA-256
`65B9CE4D6D9383DAC6F05E842BFA24C38BB2F246EA1770DA7ABB5B6C7C028877`.
Its future 12-hour nonzero-offset qualification remains deferred and cannot be
replaced by 0x0515 evidence.

## Candidates

STATIC SHADOW consumes the configured authoritative NET or GROSS microgram
weight and official stable. It needs three consecutive stable 10 Hz samples.
Unstable is PENDING, and R5 DOSING explicitly keeps STATIC PENDING. Invalid
limits/input, fault, overload, calibration, sequence or timestamp failure are
INVALID. Explicit ZERO/TARE/clear/config/mode reset remains PENDING for the
complete reset sample and reacquires only on later samples.

DYNAMIC SHADOW converts the current raw ADC sample through the active frozen
CalibrationModel and zero offset. It does not compare raw counts with limits.
The selected parameters are 20,000 ug (2d) hysteresis, one-sample confirmation
and zero minimum dwell. One-sample confirmation is required by the real 500 g
unload: its physically sampled OK interval can be only one sample. Hysteresis
still reduced 599 synthetic raw boundary returns to zero confirmed changes.

The equality contract is frozen: below low is LOW, low and high equality are
OK, above high is HIGH, and low above high is INVALID. Equal limits are legal
for the SHADOW boundary contract.

## Offline evidence

The finite search evaluated 240 combinations; eight passed. Real 10 Hz data
cover 500 g load/unload, five cycles, empty and loaded static, slow and faster
fill, positive/negative boundaries and synthetic reset/fault/DOSING cases.
Existing physical runs are DEVELOPMENT or OPENED REGRESSION. Thresholds used
to expand coverage are explicitly synthetic and were selected before candidate
outputs were scored.

Worst real detection delays are 103 ms for 500 g load, 0 ms for unload,
153 ms across five cycles and 215 ms for fill. There are zero clear-crossing
misses, direction errors, stable-far false switches, unstable STATIC valid
classifications, DOSING STATIC valid classifications, invalid-condition valid
outputs and repeated stable alarm events. A clear reference crossing is an
independent fast classification persisting for two real samples; latency starts
at its first crossing sample.

Python/C parity covers 45,711 samples with zero mismatch over both inputs,
stable/process-active/valid, immediate and confirmed classes, counters,
suppression/reset reasons and event counts. Frozen R5 parity is 25,057/0 and
D1-C parity is 49,105/0.

## Isolation and resources

The candidate is an independent module. It does not reference LimitChecker,
AlarmOutputManager, OutputGpio, BLE telemetry or DisplayController. It writes
only its own state. `App_UpdateAlarmOutputs`, public Modbus 0x0220-0x023B,
BLE checkweigh, panel and authoritative weights remain on the formal path.
Engineering registers expose SHADOW state and a Beta-only command changes
volatile signed-int32 microgram limits; neither writes persistent config.

Candidate state is 24 bytes and volatile limits add 16 bytes, for 40 bytes
permanent RAM. Linker RAM is 19,488 bytes versus 19,448 for 0x0514. The global
maximum main call chain remains 1,128 bytes, so RAM plus global-chain increase
is 40 bytes, below the 56-byte hard budget. Conservative stack remains 1,488
bytes and collision margin is 528 bytes, above the 512-byte gate. Candidate
local stack is 72 bytes and its manager wrapper is 136 bytes, but neither is
the system maximum. The loop-free 670-byte Thumb candidate is bounded below
40 us; including calibration conversion remains below 60 us at 72 MHz.

## Firmware and next gate

Standard Release remains 172,200 bytes with SHA-256
`82E726F5B32A0DE36A5E686F62A937EC4FD9CBB488DB9733E83D2062673EF486`.
The 0x0515 Beta uses Map 0x0104, Schema 2 and Persistent Format 3:

- BIN: 105,416 bytes, SHA-256
  `62648C3AD79C324B1860BE3005B8EA365C2BCE98C084BE4F5FD20D7599065228`
- ELF: 1,989,152 bytes, SHA-256
  `46D70CEC4DF59ABF6BCC3D5AA490C5C526EDFCF6D64BA6F436E633A9763066DC`

R5 C/header/Python blobs remain `a4c88335be16d288d8d1bbe91bbfe7e32e4aea04`,
`7a2b1b4ff9776a4f60891a2f74973b7c09bd9bfe` and
`5e4b9335c0d87f4dc179251c5275c6b9882c864f`. D1-C parameters are unchanged.
The device remains 0x0514, OFF + SHADOW, with configuration SHA
`A615A475C2D7B5D64126EAEB3AAE9E3D094348FA4C4AEC7BDB8EF0010D4A3BB4`.

The application download erased only sectors 0-102 and device Verify passed.
Configuration SHA before, after and at final state remained
`A615A475C2D7B5D64126EAEB3AAE9E3D094348FA4C4AEC7BDB8EF0010D4A3BB4`.

Five valid 500 g load/unload cycles captured 19 physically observable
transitions. One unload moved directly HIGH to LOW within one 10 Hz sample, so
the independent fast reference had no intermediate OK state. Every observable
transition was confirmed in the same sample: median and maximum delay are 0
ms, with zero miss and zero direction error. All cycle files have 100% sequence
coverage and maximum sequence gap 1.

The DOSING pause run has 300/300 process-active samples, 300 STATIC PENDING and
zero STATIC valid classification. ZERO was captured around the command; the
first following sample made both candidates PENDING with explicit RESET reason.
Volatile empty-OK, equal-limit and low-above-high tests passed. Across all
physical cycle records, formal alarm state, three lamps and both buzzers have
zero nonzero record. Fault, overrun, dirty and SAVE remain zero and
revision/saved revision remain 8/8.

The user could not perform slow physical fill with pauses. Mechanical
disturbance, physical TARE/CLEAR TARE and physical fault injection are also not
run; their reset/error contracts pass Host injection only. These gaps prevent a
complete SHADOW qualification. They are not replaced by fast cycles or labeled
as passed.

Final device state after MCU reset is firmware 0x0515, Map 0x0104, OFF + R5
SHADOW, offset/reference/evaluation = 0, Alarm SHADOW enabled only for
diagnostics, fault/overrun/dirty/SAVE = 0 and revision/saved = 8/8. Active
output remains prohibited. ASan and UBSan are NOT RUN. Stage 5N-B authorization
cannot be requested until the deferred physical SHADOW coverage passes.
