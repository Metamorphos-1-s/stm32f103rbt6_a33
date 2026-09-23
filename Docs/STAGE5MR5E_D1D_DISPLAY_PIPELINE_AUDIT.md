# Stage 5M-R5E-D1-D Display Pipeline Audit

## Baseline

The audit starts at `e6308a38796426296f2663831cefaefce5a0225a` on
`stage5nb-guarded-active-alarm`. The device was read without SWD or reset and
reported Firmware 0x0516, Map 0x0104, 10 Hz, filt3/strength3, R5 OFF + SHADOW,
Checkweigh OFF, offset/reference/evaluation 0/0/0, fault/overrun/dirty/SAVE
0/0/0/0 and revision/saved 8/8.

## Pipeline Before D1-D

`WeightEngine` owns authoritative uncompensated and operational gross/net.
R5 can provide an external offset to WeightEngine only when application is
ACTIVE. SHADOW never changes operational gross/net. The original
`DisplayConditioner` consumes the currently selected operational NET or GROSS
mass after WeightEngine and owns TRACKING, CANDIDATE, the nine-sample median
anchor, LOCKED, stability hold and operator-zero protection.

In the 0x0516 Beta, `DisplayController` then converted that conditioned mass to
display counts and ran a second `DirectionalDisplayFollower`. That second
state object was enabled only when R5 application was ACTIVE. OFF and SHADOW
therefore retained the original indefinite 8d lock. The panel could have one
anchor in DisplayConditioner and a second anchor in DisplayController.

The TARE page bypasses display conditioning and shows tare mass. NET/GROSS
pages use the conditioned value only when the page matches runtime
`weight_view`. Unit conversion is performed by `UnitConverter` using the
active unit, decimal places and division digit.

## Consumers

- The physical panel consumes the conditioned NET/GROSS value.
- The Modbus primary block exposes panel display count separately from
  authoritative net, gross and tare values. Its diagnostic block also exposes
  conditioned display mass and anchor.
- BLE FAST telemetry carries conditioned display mass and separate operational
  net/gross/tare masses. BLE SLOW telemetry carries compensated and
  uncompensated gross separately.
- Checkweigh never consumes the panel value. STATIC uses WeightSnapshot
  net/gross and DYNAMIC uses fast calibrated mass. Guarded ACTIVE consumes the
  frozen Stage 5N candidate only.
- ZERO, TARE, CLEAR TARE, calibration, filter reconfiguration, unit changes and
  engine rebuilds already call the DisplayConditioner reset/anchor APIs.

## Unified Ownership

The 0x0517-only build moves normalized display-count ownership into
`DisplayConditioner`. It now has one authoritative target, one display count,
one anchor, one signed evidence accumulator and one release reason.
`DisplayController` is stateless with respect to following and only formats
the conditioned count. The old D1-C source remains unchanged for its opened
regression comparison and for 0x0516 isolation, but no D1-D product runtime
state is allocated from it.

The source identity encodes NET/GROSS, unit, decimal places, 1/2/5 division
and R5 applied/unapplied path. A source change immediately clears candidate
history and evidence and publishes the new authoritative target. R5 mode does
not alter the source because STATIC and DOSING both use the same operational
mass path; ACTIVE application changes do alter it. OFF and SHADOW both target
the formal uncompensated operational mass.

Within LOCKED, delta is evaluated in quantized display-count space:

- `abs(delta) <= 1d`: retain display and leak evidence toward zero.
- `1d < abs(delta) <= 8d`: one evidence point per unique stable sample;
  opposite samples cancel existing evidence first; five points move exactly
  one division and clear evidence.
- `abs(delta) > 8d`: publish the authoritative target in the same sample and
  clear evidence.

TRACKING, CANDIDATE, the nine-sample median, stability hold, operator ZERO,
calibration/overload/invalid releases and explicit unstable response remain.
The median is stored in quantized display counts; quantization is monotonic,
so the median order is preserved while reducing the nine-entry window from
64-bit masses to 32-bit counts.

## Isolation

R5 reference-lock sources, Stage 5N shadow/guard sources and WeightEngine have
no diff from the baseline. Their Git blobs are recorded in the result freeze
audit. Release and 0x0516 are compile-time isolated from D1-D and rebuild to
their frozen hashes. Map remains 0x0104, Schema 2 and Persistent Format 3.

No display result feeds R5, authoritative gross/net, ZERO/TARE math,
calibration, official stability, Checkweigh, PLC control, faults or Flash.

