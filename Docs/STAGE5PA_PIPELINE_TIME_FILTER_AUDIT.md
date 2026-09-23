# Stage 5P-A Pipeline, Time and Filter Audit

Baseline is `97dea68416636f03fee5a6ab6612cd3291ba1166` on the controlled D1-D
branch. The device preflight was 0x0517, 10 Hz, filt3, R5 OFF+SHADOW,
Checkweigh OFF, revision/saved 8/8 and zero fault/overrun/dirty/SAVE.

## Pipeline

`CS1237 raw ADC -> WeightFilter -> calibration -> WeightEngine gross/net ->
R5 external drift application -> D1-D DisplayConditioner -> panel/Modbus/BLE`
and in parallel `WeightSnapshot gross/net or fast calibrated mass -> frozen
Stage 5N shadow -> guarded Checkweigh`. Display never feeds R5, authoritative
weight, stability, calibration, tare or Checkweigh.

## Time Semantics

R5 already aggregates samples into real timestamp one-second buckets and uses
second-based holdoff/reference/observation/evaluation windows. Stage 5P-A keeps
that behavior. The product build adds 100 ms physical-time pacing to D1-D
evidence and 200 ms elapsed-time STATIC confirmation. At 40 Hz, evidence is
therefore not four times faster than at 10 Hz. The legacy 0x0517 build is
compile-time isolated and byte-frozen.

The product build supports only 10 and 40 Hz. 640/1280 Hz remain rejected by
the product filter initializer. Filter coefficients are normalized by rate:
average window scales 4x at 40 Hz; IIR strength increases by two bits, capped
at eight; median3 retains its median stage and rate-scaled IIR. All rate/filter
changes reset the filter and stability path through the existing profile
switch transaction.

## Persistent Request State

Persistent Format 3 payload byte 280 was previously reserved and is now used
only by the 0x0518 product build for requested R5 mode/application and
Checkweigh mode. The payload remains 281 bytes and V3 slots/CRC/sequence remain
unchanged. Old records have byte 0 and safely decode as OFF+SHADOW/OFF. Invalid
new request bits are clamped to those safe requests while preserving calibration
and all other decoded fields. Offset, reference, evaluation, windows, anchors,
stable state and outputs remain RAM-only.

The startup path restores requested modes only after initialization. It begins
with zero runtime compensation and guarded outputs; explicit SAVE is still the
only persistence operation. Runtime request changes mark the config dirty but
do not write Flash until the existing save transaction completes.

## EARLY_TRACKING

The candidate was evaluated separately on ten opened development datasets using
15 s holdoff, 15-45 s reference, 10 s evaluation, 30 s median window, two
confirmations, earliest change 65 s, 0.001667 g per 10 s and 0.100 g cap. It
had no reverse amplification, but `r4_cycles` and `filt2_load` failed the
required improvement count. The candidate is rejected and is not linked into
the product firmware. Existing R5 long-term behavior is preserved.

## Waivers

`OWNER-ACCEPTED QUALIFICATION WAIVERS; NOT METROLOGICALLY OR CROSS-SENSOR
QUALIFIED`. External 40 Hz physical timing, cross-sensor, metrology, physical
sensor fault, new independent 12 h and ASan/UBSan evidence remain waived or
deferred.
