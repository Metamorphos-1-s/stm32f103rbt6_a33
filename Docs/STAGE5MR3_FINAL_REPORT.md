# Stage 5M-R3 Offline Final Report

Status:

STAGE 5M-R3 OFFLINE DEVELOPMENT CANDIDATE PASSED;
70 PERCENT STRETCH AND CROSS-SENSOR HARDWARE VALIDATION PENDING

## Result

The selected explicitly mode-gated rate estimator uses:

- 180 second observation windows;
- 15 second block medians with a full-window least-squares trend;
- 0.002 g estimator deadband;
- 1.0 g/h declared-static ceiling;
- 0.020 g fast-step detector;
- 0.0045 g/min correction limit;
- 87.5 percent trend-rate damping;
- 15 second hold-off after re-enabling compensation.

| Capture | Input slope g/h | Corrected slope g/h | Improvement |
|---|---:|---:|---:|
| Cold start 1 | 0.023508 | -0.008368 | 64.4% |
| Cold start 2 | 0.167963 | 0.068021 | 59.5% |
| 500 g constant load | 0.016558 | -0.003299 | 80.1% |

Every static capture improves by more than 50 percent and the median improvement
is 64.4 percent. The exploratory 70 percent median stretch goal is not met.
This distinction is intentional and is not reported as a 70 percent pass.

Continuing to tune against the same three captures is prohibited because these
records have already been used for selection. Higher improvement must be shown
on independent captures rather than obtained by post-hoc tuning.

## Safety result

- No static capture has amplified absolute fitted slope.
- Constant synthetic tails stop changing and do not chase a completed load.
- All 12 real dosing-mode cases keep the existing offset frozen.
- All 252 synthetic dosing-mode cases keep the existing offset frozen.
- Synthetic increments cover 0.0001 to 0.020 g, 2/5/10 second pauses and
  60/120/240/600 second dosing durations.
- Maximum correction over 10 seconds remains no more than 0.001 g.
- Compensation is disabled by default in the future operating contract.

## Sensor portability decision

The algorithm must not compare fixed raw ADC counts. The same count delta has a
different physical meaning after changing load-cell capacity, sensitivity,
excitation, PGA gain, or calibration span.

The intended pipeline is:

1. acquire the raw ADC sample before drift compensation;
2. retain raw counts for rail, jump and hardware diagnostics;
3. convert raw deltas through the active two-point calibration;
4. perform drift decisions in calibration-normalized mass/division units;
5. apply the additive drift offset before tare and display conditioning.

The current offline evaluator uses calibration-normalized micrograms and proves
identical decisions for synthetic 1000/2000/4000 counts-per-gram sensors.
That proves mathematical scaling invariance, not real cross-sensor behavior.

Before product integration, the absolute microgram parameters must be expressed
relative to configured verification interval e and observed robust noise, for
example:

- estimator deadband = max(fraction of e, robust-noise multiple);
- step threshold = max(multiple of e, robust-noise multiple);
- correction rate = bounded e/min or ppm-full-scale/hour.

A calibration, load-cell, PGA-gain, or sample-rate change must clear the runtime
reference, rate estimate, and compensation offset. Invalid calibration keeps
compensation disabled.

## Operating boundary

STATIC_COMPENSATION declares that the current physical load is intended to stay
constant. DOSING_NO_COMPENSATION is mandatory before liquid, powder, or repeated
incremental dosing. Re-enabling static compensation starts a hold-off and
rebuilds the rate estimator while preserving no unverified trend.

A slow real load can still be consumed if an operator violates this mode
contract. The mode is therefore a safety boundary, not merely a tuning hint.

## Next gate

Before any product firmware or hardware shadow run:

1. capture new empty and constant-load records not used for tuning;
2. capture at least one differently rated/sensitive calibrated load cell;
3. replace absolute thresholds with e/noise-normalized parameter derivation;
4. port the frozen estimator to bounded fixed-point C;
5. prove sample-by-sample Python/C equality;
6. define runtime owner, boot-default, PLC/menu command and BLE read-only status;
7. only then build an isolated non-product shadow diagnostic.

No product source, product image, persistent format, public register map,
hardware configuration, merge, tag, pull request, or flash operation is
authorized by this report.
