# Stage 5M-F Pipeline and RAM Audit

## Frozen baseline

The stage starts at `18d89803abdb10e417d10728d7b43cdbb4ac2057` on
`stage5mf-low-ram-adaptive-filter`. R5E is Firmware `0x0512`, Map `0x0104`,
Schema 2 and Persistent Format 3. The standard Release SHA-256 is
`82E726F5B32A0DE36A5E686F62A937EC4FD9CBB488DB9733E83D2062673EF486`.

The R5 C implementation/header/Python blobs remain respectively
`a4c88335be16d288d8d1bbe91bbfe7e32e4aea04`,
`7a2b1b4ff9776a4f60891a2f74973b7c09bd9bfe`, and
`5e4b9335c0d87f4dc179251c5275c6b9882c864f`.

## Current 10 Hz path

`CS1237 -> RawMeasurementSample -> WeightEngine_ProcessRawSample ->
WeightFilter -> CalibrationModel_ConvertMass -> uncompensated gross -> R5 ->
tare/net -> StabilityDetector -> DisplayConditioner -> display/PLC/BLE/alarm`.

The active high-precision profile is `MEDIAN3_IIR`, strength 3. The first two
raw samples prime the median; each later sample uses median-of-three raw counts
then an IIR update rounded symmetrically by `WeightMath_DivideRoundNearest`.
Calibration occurs only after this official filter. Stability consumes the
official net mass. Display, PLC, BLE, alarm, ZERO/TARE, and overload use the
official `WeightSnapshot` path.

R5 receives `snapshot.uncompensated_gross_mass_ug` after the official filter
and calibration, before the external R5 offset is applied. It also receives the
unchanged official timestamp and sample sequence. Stage 5M-F must not reorder
or replace that call.

## Reusable values and isolation

Each accepted sample already exposes raw ADC, official filtered raw,
calibrated official uncompensated gross, timestamp, sequence, near-rail status,
official stable status, R5 mode/application, and frozen R5 offset. A candidate
can read these values after WeightEngine and R5 processing without changing
them.

There is no simultaneous filt0/filt3 mass in the product. A faster candidate
therefore needs one small streaming raw-domain state and a calibration-scale
conversion that avoids an additional per-sample 64-bit division. It must not
instantiate a second WeightEngine or a second full WeightFilter. Candidate
output may subtract a read-only R5 offset only after candidate filtering; that
result cannot feed R5.

Reset events are power-on, calibration commit, profile/filter change, ZERO,
TARE/CLEAR TARE if the candidate contract requires output re-anchoring, sample
gap, invalid calibration, near rail, fault and numeric overflow. R5 DOSING is a
read-only `process_active` input, not a reset or control output.

## RAM and stack budget

R5E has 12 B `.data`, 18,392 B `.bss`, 1,048 B unallocated after the fixed
1,024 B minimum stack, and 2,072 B from static end to `_estack`. The
conservative stack bound is 1,488 B, leaving 584 B. The mandatory collision
margin is 512 B, so new static state plus new worst-chain stack is limited to
72 B. The design target is at most 48 B persistent state and 24 B additional
stack.

The historical adaptive filter is 256 B and contains a 16-sample `int64_t`
trend array; it is excluded. A viable replacement may use one candidate output,
one fast raw accumulator, one signed motion EWMA, timestamps/counters and small
state/reason flags. It may not add a history array, dynamic memory, or shrink
DMA/communication buffers.

## Diagnostics

R5E Beta diagnostics occupy `0x0280–0x02A7` completely. `0x02A8` onward is
currently unallocated. If and only if offline and new-holdout gates pass, a
Beta-only extension can use that space without moving existing R5E addresses,
changing Map `0x0104`, Schema 2, or Persistent Format 3. Priority is candidate
weight, state, reason, fault and transition count.

## Data classification

All Stage 5M-A tuning runs are **DEVELOPMENT**. Every former Stage 5M-A holdout
has been opened and is now **OPENED REGRESSION**, including filt1/filt3 steps,
faster slow-fill, creep and cold-control results. The historical conclusion
remains `NO ACCEPTABLE CANDIDATE` and the robust dual IIR remains a failed
256-byte reference.

A **NEW HOLDOUT** must be captured only after the low-RAM model, parameters,
acceptance script, RAM budget and capture plan are committed. Before that
freeze, product/Beta integration is prohibited.

## Integration decision

The official path and R5 input can be kept byte-for-byte and sample-for-sample
unchanged by running an isolated candidate after the official snapshot is
available. This audit therefore allows Python development and fixed-point Host
work. Target/Beta SHADOW integration remains conditional on all new-holdout,
parity, RAM/stack, CPU, and regression gates.
