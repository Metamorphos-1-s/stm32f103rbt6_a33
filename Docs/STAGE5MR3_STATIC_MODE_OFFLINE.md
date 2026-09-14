# Stage 5M-R3 Static-Mode Drift Compensation

Status: OFFLINE SEARCH IN PROGRESS.

## Corrected understanding of R2

The R2 value 0.008 g is a minimum estimator deadband because the implementation
requires abs(delta) >= 0.008 g. It is not a maximum accepted drift. Small real
drift therefore fails to trigger consistently, while cold-start 2 is often
blocked by the direction-consistency classifier. This explains the limited
static improvement.

## Simplified operating contract

- DOSING_NO_COMPENSATION is the safe boot default.
- PLC or the front panel selects DOSING_NO_COMPENSATION before liquid, powder,
  or repeated incremental dosing.
- STATIC_COMPENSATION declares that the applied load is expected to remain
  constant and allows stronger long-term correction.
- Re-entering STATIC_COMPENSATION rebuilds the reference after a 15 second
  hold-off.
- A fast step still freezes correction and rebuilds the static reference.

The mode is runtime-only in this phase. It is not added to persistent
configuration or any public protocol until offline and C-parity gates pass.

## Sensor-independent input contract

Raw ADC samples are required for acquisition diagnostics, rail detection and
fault analysis, but fixed raw-count thresholds are prohibited because count
sensitivity changes with the load cell, capacity, excitation, PGA gain and
calibration span.

The algorithm decision domain is calibration-normalized micrograms derived from
the active two-point calibration. A future implementation must reset its
reference and offset whenever calibration, sensor, PGA gain or sample rate
changes, and must remain disabled when calibration is invalid.

The offline gate includes synthetic scale-invariance cases representing
different ADC counts per gram. Real cross-sensor validation remains required
because all current captures came from one physical sensor.

## Search and gates

The isolated evaluator sweeps:

- observation windows: 60, 120, 180 and 300 seconds;
- estimator deadbands: 0, 0.00025, 0.0005, 0.001 and 0.002 g;
- maximum declared-static rates: 0.5, 1.0, 2.0 and 5.0 g/h;
- fast-step thresholds: 0.020, 0.050 and 0.100 g;
- correction limits: 0.003, 0.0045 and 0.006 g/min.

A candidate passes only if:

- none of the three static captures has amplified slope;
- every static capture improves by at least 50 percent;
- median improvement is at least 70 percent;
- a constant tail does not cause continued offset chase;
- all real and synthetic dosing-mode cases keep their offset exactly frozen;
- the 10 second correction is no more than 0.001 g;
- calibrated sensor scaling produces identical decisions and offsets.

Every observation window records its accepted/rejected result, reason, estimated
rate, desired offset, actual offset and target offset.

## Boundaries

This branch is offline-only. It does not modify or build product firmware, does
not authorize a flash operation, and does not enter fixed-point C or hardware
shadow mode. Fast response, adaptive settling, checkweigh alarms and conveyor
weighing remain separate later stages.
