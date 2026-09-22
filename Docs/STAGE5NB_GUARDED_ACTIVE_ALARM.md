# Stage 5N-B Guarded ACTIVE Alarm

## Software Result

**STAGE 5N-B GUARDED ACTIVE ENGINEERING BETA READY; CHECKWEIGH REMAINS
DEFAULT-OFF AND VOLATILE; PHYSICAL SENSOR-FAULT QUALIFICATION DEFERRED; D1-C
NONZERO-OFFSET QUALIFICATION DEFERRED; 40 HZ REMAINS BLOCKED; STAGE 5O ENTRY
REQUIRES EXPLICIT DECISION.**

The authorized B6 hardware session completed. The device now runs the exact
0x0516 engineering Beta and its final state is CHECKWEIGH OFF, R5 OFF + SHADOW,
all formal outputs off, fault/overrun/dirty/SAVE 0/0/0/0 and revision/saved
8/8. Power cycling proved that ACTIVE mode and the RAM-only alarm setup do not
persist.

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

The final 0x0516 Beta BIN is 106,884 bytes, SHA-256
`759AF431C2F7379296817419090DA305C55D18110A6E1733312B8C5388BF98FE`.
Its ELF is 2,012,656 bytes, SHA-256
`DF8AE6B4187C0925D196B2BF01055E534EE5F4419B154C3A451E1497D4B1133E`.
The B6-only diagnostic BIN is 107,260 bytes, SHA-256
`86BAE31EDAE95FBE50468002C24E52CE34507B2C79544247C84D71A02741E682`.

0x0516 uses 19,472 B linker RAM and 106,884 B Flash. Static RAM to stack top is
2,032 B. The largest main chain is 1,144 B and largest IRQ chain 72 B. Adding a
32 B exception frame and 256 B indirect-call allowance gives a 1,504 B
conservative stack bound and 528 B collision margin, passing the 512 B gate.

## Hardware Qualification

The configuration region was backed up before flashing and compared after the
diagnostic image, after the exact Beta, and at final cleanup. Every copy is
4,096 bytes with SHA-256
`A615A475C2D7B5D64126EAEB3AAE9E3D094348FA4C4AEC7BDB8EF0010D4A3BB4`.
Only application pages 0-104 were erased. No SAVE was sent.

The diagnostic build ran DYNAMIC/HIGH with 500 g, then injected 2,000 ms of
INVALID_INPUT covering 20 target samples. Six host polls observed formal
DISABLED with every lamp and buzzer off. The target recovered automatically,
performed the safe reacquisition sequence and returned to HIGH. The operator
independently confirmed the physical outputs extinguished and recovered.

The exact Beta passed default OFF, local STATIC, local DYNAMIC, LOW/OK/HIGH,
three 137 g/637 g DYNAMIC cycles, slow LOW-to-OK-to-HIGH loading, mechanical
disturbance, TARE, CLEAR TARE, STATIC-to-DYNAMIC switching, PLC OFF/STATIC,
stale local-edit rejection with `bUSY`, emergency OFF and power-cycle default
OFF. No mutually exclusive lamps overlapped. LOW was yellow and silent, OK was
green with a one-shot qualified beep, and HIGH was red with nonblocking
intermittent internal and external alarms.

The supervised session exceeded 30 minutes. A 900.172 s OFF capture contains
2,722 samples and zero read errors. COM5 was later physically unplugged during
the supervised ACTIVE work, so that interval is supported by point snapshots
and explicit operator observations rather than a continuous serial trace. A
subsequent ACTIVE recorder ended on a short/CRC response because device power
was physically removed; that interrupted capture is retained and is not used
as PASS evidence. The real power cycle itself passed the required default-OFF
gate. These host-evidence interruptions did not hide a device fault:
fault/overrun remained zero before power loss and final state was clean.

Physical sensor-fault, D1-C nonzero-offset, cross-sensor, 40 Hz, metrology,
ASan, UBSan and Stage 5O remain deferred. The SWD INVALID_INPUT result is not
represented as a physical sensor-disconnection qualification.
