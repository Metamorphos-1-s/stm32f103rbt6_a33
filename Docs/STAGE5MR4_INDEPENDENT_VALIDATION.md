# Stage 5M-R4 Independent Validation

R4 begins from exact R3 commit `ca3008a56bbd3577f71c65b60329c28052551285`. GitHub Actions run 34870894842 completed successfully at that SHA. A clean Python 3.13 replay reproduced the frozen selected parameters and all R3 gates.

The first blind replay will use 180-second windows, 15-second block medians, whole-window OLS, 0.002 g deadband, 1.0 g/h declared-static ceiling, 0.020 g fast step, 75 micrograms/s cap, 875 permille damping and 15-second re-enable holdoff without tuning.

The operator confirmed the installed load-cell nameplate is 3 kg, matching the
device configuration. The R4 task's 6 kg statement is retained as a documented
text error and is not used as sensor metadata.

## Independent result

The new evidence set contains two cold empty runs, one hot empty run, two 500 g
constant-load runs, one 1 kg constant-load run, and five complete 500 g
load/unload cycles. The 1 kg steady boundary was recorded from the operator
confirmation before replay. No R4 sample was used for R3 parameter selection.

The replay protocol was committed before the first persisted blind result. It
uses the frozen R3 parameters without a grid search. A fixed 20-transition mode
schedule was also defined before replay because product firmware 0x0510 does
not yet implement the proposed compensation modes.

| Run | Raw g/h | Corrected g/h | Improvement |
|---|---:|---:|---:|
| Cold empty 1 | -0.090504 | -0.024018 | 73.46% |
| Cold empty 2 | -0.081077 | -0.057411 | 29.19% |
| Hot empty | +0.085434 | +0.076369 | 10.61% |
| 500 g hold 1 | +0.048015 | +0.037040 | 22.86% |
| 500 g hold 2 | -0.019385 | +0.027313 | -40.90% |
| 1 kg hold | +0.020596 | +0.019106 | 7.23% |

All six runs exceed the 0.015 g/h significance threshold. Median improvement is
16.73%, below the required 50%. The second 500 g run reverses direction and
increases absolute slope, so the no-amplification gate also fails.

The bounded-update safety behavior did pass: maximum 10-second correction was
0.00075 g, the maximum correction-induced sample step was 0.000075 g, dosing
mode kept offset strictly frozen, all 20 mode transitions had no correction
jump, and every static re-enable respected the 15-second holdoff.

Status: **STAGE 5M-R4 FROZEN CANDIDATE FAILED INDEPENDENT VALIDATION**.

No R4 tuning, normalized successor, fixed-point implementation, diagnostic
firmware, flash operation, or hardware shadow run is authorized from this
result.
