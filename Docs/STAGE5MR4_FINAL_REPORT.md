# Stage 5M-R4 Final Report

## Decision

**STAGE 5M-R4 FROZEN CANDIDATE FAILED INDEPENDENT VALIDATION**

R4 started from R3 commit
`ca3008a56bbd3577f71c65b60329c28052551285`. The blind replay protocol and its
Python 3.8 output-only compatibility fix were committed before the persisted
result. The report binds every input CSV and Manifest V2 by length and SHA-256.

The installed sensor is 3 kg. Its calibration was zero -43989 counts, span
-487850 counts, span mass 500 g, giving approximately 887.722 counts/g in the
negative load direction. Configured verification interval e is 0.05 g.

Six independent static runs were evaluated. Median improvement for runs at or
above 0.015 g/h was 16.73%; the 70% stretch was not met. The second 500 g hold
was amplified from -0.019385 g/h to +0.027313 g/h. This blocks the candidate.

Five load/unload cycles produced 20 detected static step events and zero offset
movement over their short plateaus. A fixed 20-transition mode trace passed
dosing freeze, 15-second holdoff, no-jump, and 0.001 g/10 s safety gates.

## Scope stopped

- No parameters were tuned on R4 holdout data.
- Cross-sensor hardware validation was not run.
- Normalized successor research was not started.
- Fixed-point C and Python/C parity were not run.
- Shadow diagnostics were not built or flashed.
- Active compensation was not evaluated or enabled.
- Product Release, Map 0x0104, and Persistent Format V3 were unchanged.
- No merge, tag, or pull request was created.

The appropriate next phase requires a new development data set and a separately
defined candidate. These R4 runs must remain locked as validation evidence and
must not be reused for iterative tuning.
