# Stage 5N-A2 Checkweigh SHADOW Hardware Closure

## Result

**STAGE 5N-A SOFTWARE CANDIDATE PRESERVED; SHADOW HARDWARE QUALIFICATION
INCOMPLETE; ACTIVE OUTPUT REMAINS BLOCKED.**

Stage 5N-B entry is not approved. No candidate defect was established, but the
four required physical scenario groups were not all completed. The extreme
monotonic slow ramp was invalidated by reported intermediate unloads, and no
safe reversible physical fault injection exists in frozen firmware 0x0515.

The stage started at `39462080bc8cd12ca2d31c7dce3a1740637c06e5` on
`stage5na-checkweigh-alarm-offline-shadow` and continued on
`stage5na2-checkweigh-shadow-hardware-closure`. Firmware, candidate parameters,
R5, D1-C, filtering, calibration and formal alarm paths were not modified.

## Frozen Device

The live device was 0x0515 / Map 0x0104 / Schema 2 / Persistent Format 3,
Profile 0, 10 Hz, filt3/strength3 and valid calibration. Alarm SHADOW limits
were 100 g and 400 g with 0.020 g dynamic hysteresis. R5 remained OFF + SHADOW
with offset/reference/evaluation/rebase zero. Initial and final fault, overrun,
dirty and SAVE were zero; revision/saved revision remained 8/8.

The 0x0515 BIN remains 105,416 bytes, SHA-256
`62648C3AD79C324B1860BE3005B8EA365C2BCE98C084BE4F5FD20D7599065228`.
The standard Release ELF remains 172,200 bytes, SHA-256
`82E726F5B32A0DE36A5E686F62A937EC4FD9CBB488DB9733E83D2062673EF486`.

Configuration-region SHA was **NOT RUN**. Reading it requires an SWD operation
that resets the MCU, and no reset authorization was given in this stage. The
previous expected SHA is not presented as a new A2 measurement. No SAVE,
configuration write, flash operation or firmware programming occurred.

## State Definitions

STATIC consumes authoritative configured NET/GROSS and official stable. It is
PENDING while unstable and requires three consecutive stable samples before a
valid LOW/OK/HIGH result. DYNAMIC uses the calibrated current ADC path with
20,000 ug hysteresis and one-sample confirmation. INVALID and PENDING retain
their frozen Stage 5N-A meanings. Candidate state never drives the formal
LimitChecker, Modbus/BLE alarm surfaces, lamps or buzzers.

## Physical Evidence

The combined evidence contains 12,796 records, zero read errors, 110 observed
DYNAMIC transitions and zero state outside the permitted hysteresis envelope.
Every observed transition had candidate equal to confirmed in that sample.
The maximum host observation gap was 391 ms; the planned slow-operation segment
had 44 observed transitions and a maximum adjacent observed transition bound of
258 ms. Both are below the 600 ms response gate. Direct cross-region samples
are identified separately and are not counted as missed intermediate states.

Slow loading covered LOW, OK and HIGH, reverse unloading, segmented loading,
and user-timed 2, 5 and 10 second holds. Formal outputs and safety counters
remained zero. The user reported several intermediate unloads while approaching
the high threshold. Those bytes are preserved as segmented/disturbance evidence
but the segment is excluded from extreme monotonic-ramp qualification.

Mechanical disturbance passed at stable LOW, OK, HIGH and nominal 100 g near
the lower threshold. The four recovery windows contained respectively 42, 35,
37 and 36 unstable records. STATIC was PENDING in every unstable record and
reacquired LOW, OK, HIGH and LOW after official stable returned. No fault,
overrun or formal output activation occurred.

Physical panel TARE and CLEAR TARE were executed under an unchanged nominal
100 g load. Gross remained approximately 100 g; TARE produced tare
approximately 100.005 g and net approximately zero; CLEAR restored tare zero
and net approximately 100 g. The physical CLEAR reset sample was captured as
PENDING/RESET. The physical TARE reset sample fell between host polls, so two
formal CommandService captures supplemented the evidence. TARE sequence 109124
was PENDING/PENDING with RESET and reacquired at sequence 109143. CLEAR reset
sequence 109374 was PENDING/PENDING and reacquired after the reset. These
commands use the same CommandService actions as the panel and did not SAVE.

Physical fault injection is NOT RUN. Frozen 0x0515 exposes no safe reversible
fault injection command. Disconnecting the load cell does not reliably make
CS1237 data stale, requires power removal under the product manual and risks
electrical disturbance or calibration integrity. Shorting, overload and live
wiring changes were prohibited. Host fault/stale/reset contracts remain covered
by Stage 5N-A software tests but are not relabeled as physical evidence.

## Safety And Isolation

Across all captures, STATIC contract errors, dynamic hysteresis violations and
formal-output nonzero records were zero. R5 offset/reference/rebase remained
zero. Revision/saved revision remained 8/8; dirty, SAVE, fault and overrun
remained zero. The final device is empty and stable, 0x0515, 10 Hz,
filt3/strength3, OFF + SHADOW, with formal alarm state, lamps and buzzers zero.

## Software Regression

Host CTest passed 28/28 after a non-strict MSVC build. Stage 5N-A Python passed
12/12 and Python/C parity passed 45,711 samples with zero mismatch. R5 parity
passed 25,057/0 and D1-C parity passed 49,105/0. Stage 5B passed 30/30, Stage 5C
12/12, Stage 5L 8/8, Stage 5L-R G2 8/8, and the selected R2/R3/R4/R5 tool
regressions passed. Debug, standard Release and 0x0515 Beta builds passed.
The evidence Manifest verified file lengths, SHA-256 values and Git blobs in
independent `core.autocrlf=false`, `input` and `true` clones.

The strict MSVC `/WX` build failed on two pre-existing frozen warnings: C4310
in D1-C and C4127 in an R5 host test. They were preserved and not modified.
The executable regression was rebuilt without `/WX` and passed 28/28. ASan and
UBSan are NOT RUN. No dynamic allocation was found in the candidate path.

## Deferred And Next Gate

The missing extreme monotonic ramp and safe physical fault/recovery scenario
must be completed before Stage 5N-B can be requested. ACTIVE alarm output
remains blocked. D1-C nonzero-offset qualification, real 12-hour display
validation, cross-sensor validation, 40 Hz external timing, metrology
certification, ASan, UBSan and Stage 5O remain deferred.
