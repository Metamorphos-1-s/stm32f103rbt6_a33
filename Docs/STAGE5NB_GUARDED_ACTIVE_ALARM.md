# Stage 5N-B Guarded ACTIVE Alarm

## Software Result

**STAGE 5N-B GUARDED ACTIVE SOFTWARE READY; 0x0516 ARTIFACT BUILT; HARDWARE
FLASH AUTHORIZATION REQUIRED; ACTIVE OUTPUT NOT YET QUALIFIED.**

The device remains on the restored original 0x0515. No SWD access, reset,
configuration read, flash, mode change or alarm-output activation occurred in
Stage 5N-B0 through B5. Hardware work is paused at the required authorization
boundary.

## Strict Build Closure

MSVC 19.43.34810 `/W4 /WX` initially stopped at the two A3 debts: C4310 in the
D1-C flag mask and C4127 in the R5 size test. Continuing the previously blocked
full build exposed the same flag expression in D1-B, a missing target-local CRT
compatibility definition, and two more runtime checks of compile-time
contracts. All fixes are mechanical: narrow the flag before complement, use
C11 static assertions for size/enum contracts, and apply the same existing
runner-only CRT definition.

No warning was disabled and no test was removed. Full strict MSVC now builds
and passes 32/32. ARM `-Wextra -Werror` passes. D1-C parity remains 49,105/0,
and the deployable 0x0515 BIN remains byte-identical, proving the product-side
mask rewrite is code-equivalent. Standard Release also remains byte-identical.

## Guarded ACTIVE

Firmware 0x0516 adds volatile OFF, STATIC and DYNAMIC modes. Boot/reset default
is OFF. Mode changes are RAM-only, generation-protected and do not affect
revision, dirty, SAVE or Flash. OFF keeps SHADOW calculation available while
formal state is DISABLED and every lamp/buzzer is off.

STATIC uses only the frozen STATIC candidate. DYNAMIC uses only the frozen
current DYNAMIC output. The Stage 5N-A source and parameters are unchanged:
three STATIC stable samples, one DYNAMIC sample, 20,000 ug DYNAMIC hysteresis
and zero dwell. Existing AlarmConfig supplies limits, NET/GROSS source and
buzzer enables.

All unsafe or incomplete conditions are fail-silent: mode transition, OFF,
PENDING, INVALID, any fault, calibration or data older than 250 ms maps to
formal DISABLED and immediately clears all outputs. Mode entry remains safe for
the first new candidate sample and activates no earlier than the next sample.
The old LimitChecker path remains untouched in Release and 0x0515.

The only output owner is the existing AlarmOutputManager. LOW maps to yellow,
OK to green and optional one-shot qualified beep, HIGH to red and optional
nonblocking internal/external alarm phases. Host GPIO tests cover output
polarity, mutual exclusion, enable flags, residue-free mode transitions and
all safety states.

## Controls And Protocol

The advanced menu adds `ALArn` with `OFF`, `StAtIC` and `dynAnI`. It follows the
R5E confirm/apply/cancel/timeout transaction model and detects external updates
through a volatile generation, returning BUSY instead of applying stale local
state.

PLC control uses the existing mailbox: Beta command 34 SET_MODE and 35
GET_STATUS. It adds no SAVE and changes no persistent configuration. Public
Map remains 0x0104, Schema 2 and Persistent Format 3. Engineering status at
0x02C0-0x02CF is Beta-only and outside the public Map contract. Existing
clients see safety states as formal DISABLED(0) with all outputs off. BLE V1 is
unchanged and remains read-only.

## Verification

Host CTest passes 32/32 under strict MSVC. Stage 5N-A Python passes 27/27.
Python/C parity is Stage 5N-A 45,711/0, R5 25,057/0 and D1-C 49,105/0.
Stage 5B is 30/30, Stage 5C 12/12, Stage 5L 8/8 and G2 8/8. Debug, Release,
0x0515, 0x0516, Stage5NBHardware and strict ARM builds pass. ASan and UBSan are
NOT RUN. No dynamic allocation was introduced.

Standard Release ELF remains 172,200 bytes, SHA-256
`82E726F5B32A0DE36A5E686F62A937EC4FD9CBB488DB9733E83D2062673EF486`.
Frozen 0x0515 remains 105,416 bytes, SHA-256
`62648C3AD79C324B1860BE3005B8EA365C2BCE98C084BE4F5FD20D7599065228`.

The 0x0516 Beta BIN is 106,796 bytes, SHA-256
`6AECAEE3D8B2EFD00CE7EFA879CFC1B4BFA8F5753F27CC97F29E234FB2ADC851`.
Its ELF is 2,012,504 bytes, SHA-256
`721BD55249335D1B97E0C83E7D5FFED935ED2200A4F057E2C01835A4BCFEAFB7`.
The B6-only diagnostic BIN is 107,180 bytes, SHA-256
`4AFAAF85EBDBF10D779EA1DAC88834154E2CD35606AB12BC1BBB0C993DCEA985`.

0x0516 uses 19,472 B linker RAM and 106,796 B Flash. Static RAM to stack top is
2,032 B. The largest main chain is 1,144 B and largest IRQ chain 72 B. Adding a
32 B exception frame and 256 B indirect-call allowance gives a 1,504 B
conservative stack bound and 528 B collision margin, passing the 512 B gate.

## Hardware Plan And Boundary

After explicit authorization, configuration and application are backed up over
SWD. Stage5NBHardware is tested first so A3 INVALID_INPUT can prove formal
state and all outputs become safe before automatic recovery. The exact 0x0516
Beta is then flashed for default-OFF, local/PLC control and supervised physical
testing. The persisted alarm configuration is currently disabled; hardware
qualification may use a documented RAM-only configuration transaction without
SAVE, followed by reset to restore persisted revision 8/8.

Failure forces OFF, evidence preservation and verified rollback to the frozen
0x0515. Success retains 0x0516 but ends OFF with R5 OFF + SHADOW and all outputs
off. Physical sensor-fault, D1-C nonzero-offset, cross-sensor, 40 Hz, metrology,
ASan, UBSan and Stage 5O remain deferred.
