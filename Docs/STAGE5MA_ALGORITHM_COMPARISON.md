# Stage 5M-A Algorithm Comparison

## Frozen holdout result

| Algorithm | Static noise g | Load 10-90 s | Stable s | Unload 10-90 s | Slow false-stable | Static false-positive | RAM B | Evidence |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| filt0 | 0.01273 | 0.301 | 2.104 | 0.229 | n/a | 1.95% | existing | hardware baseline |
| filt1 | 0.01110 | 0.441 | 2.314 | 0.224 | n/a | 0% | existing | hardware baseline |
| filt2 | 0.00929 | 0.359 | 3.385 | 0.301 | n/a | 0% | existing | hardware baseline |
| filt3 | 0.00544 | 1.669 | 8.278 | 1.628 | n/a | 0% | existing | hardware baseline |
| dual IIR | 0.00544 | 0.319 | 5.573 | 0.339 | 24.56% | 1.53% | 256 | offline holdout |
| robust dual IIR | 0.00544 | 0.319 | 5.573 | 0.339 | 24.56% | 1.53% | 256 | offline holdout |

The robust candidate isolates one synthetic/observed single-point disturbance for at most one input observation and then permits a confirmed step. No qualifying large anomaly exists in the selected historical 10 Hz datasets, so robust and non-robust aggregate metrics are identical except for the recorded step-edge disturbance.

Frozen targets were static noise <=0.0068 g, load 10-90 <=0.60 s, stable <=3.0 s, slow-fill false-stable <=10%, and static motion false-positive <=1%. The reference meets noise and rise-time targets but fails stable time, slow-fill classification and static false-positive. It is not selected.

## Pareto result

filt3 remains the lowest demonstrated hardware noise path. filt0 is fastest and lowest-resource. The robust dual path is the closest combined offline reference, but no evaluated candidate occupies the required speed/noise/slow-classification point. A single composite score is intentionally not used.

Result: `NO ACCEPTABLE CANDIDATE`. Requirements were not relaxed after holdout. A new algorithm/state version would require development-only work and a newly declared holdout reopening; the current holdout result remains immutable evidence.

The cold-control and creep slopes remain visible in candidate output. Their source is not identified, and no automatic zero or drift subtraction is present.
