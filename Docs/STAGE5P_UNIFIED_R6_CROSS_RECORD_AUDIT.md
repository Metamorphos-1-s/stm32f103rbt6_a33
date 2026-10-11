# Stage 5P Unified-R6 cross-record resource audit

Baseline: `b14a4454cf2a709cc2971c6c1dc116b9e988730d`

## Audit scope and stop rule

This stage performed offline-only audit. The frozen resource contract's
`Checker.complete()` requires one measurement epoch to reach coverage `0x1FF`.
The R4 and R5 records are different epochs, so their union is reported as
engineering evidence only and is **not** promoted to a single-epoch PASS.
No COM5 access, flash, configuration write or 0x0524 validation was performed.

## Recomputed run evidence

The audit script `Tools/stage5_unified/audit_cross_record_resources.py` reads
the complete CSV and final cumulative snapshot for each run, checks every
path's CSV maximum against the final snapshot, and records the first mode value
at which each path counter advanced. Tests are in
`Tools/stage5_unified/test_audit_cross_record_resources.py`.

| Run | Epoch | Rows | Mask | Mode values | Stack untouched | Produced/consumed/FIFO |
| --- | ---: | ---: | ---: | --- | ---: | --- |
| R4 | 351272961 | 2,715 | `0x12F` | 0,1,2 observed; path attribution preserved | 1,264 B | 9,610 / 9,610 / 0 |
| R5 | 348651521 | 2,017 | `0x0DF` | 0,2 observed; path attribution preserved | 1,224 B | 7,969 / 7,969 / 0 |

Both runs used the same diagnostic image, `0x0526 / 0x010E`, SHA-256
`A482E1912CB34725553FD04A0E2EAA07BF7709E21D465D75DB579565A7C7001C`, at
72 MHz, 40 Hz, filt1/strength3, on the same calibrated sensor. Both had
flags=1, no fault, overrun or read error, FIFO=0, and no SAVE. Host polling
gaps were retained separately: three in R4 and one in R5.

## Per-path sources

| Path | Source | First-call mode | Calls | Peak cycles | Peak sequence | Epoch |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| OFF | R4/R5 | 0 | 103 / 500 | 216 / 341 | 2 / 255 | R4/R5 |
| HOLDOFF | R4/R5 | 0 / 0 | 150 / 251 | 82,166 / 86,254 | 994 / 594 | R4/R5 |
| REFERENCE_FILL | R4/R5 | 2 / 2 | 300 / 494 | 672,128 / 641,887 | 1,347 / 1,611 | R4/R5 |
| OBSERVATION_FILL | R4/R5 | 2 / 2 | 199 / 199 | 84,638 / 82,291 | 1,545 / 1,687 | R4/R5 |
| TRACKING | R5 | 2 | 180 | 475,050 | 1,897 | R5 |
| FAST_TRACKING | R4 | 2 | 94 | 481,131 | 1,554 | R4 |
| STEP_PENDING | R5 | 2 | 65 | 109,535 | 852 | R5 |
| STEP_SETTLING | R5 | 2 | 300 | 110,812 | 898 | R5 |
| DOSING | R4 | 1 | 1,553 | 85,485 | 753 | R4 |

Mode values are interpreted as `OFF=0`, `DOSING=1`, `STATIC=2`; no relabeling
of the R4 host mode field is performed. The R4 record contains mixed mode values
because it includes profile and pair transitions; path-specific first-call
values are used above.

## Recomputed cross-run peaks

- Feed maximum: `672,128 cycles` = `9.335 ms` at 72 MHz;
- App_Run maximum: `1,036,974 cycles` = `14.402 ms`;
- loop interval maximum: `1,037,026 cycles` = `14.403 ms`.

These are final cumulative values, not intermediate polling summaries. The
Feed margin is approximately 0.665 ms; it is not wide.

## R5 unload edge

The R5 CSV contains a clear unload step at sequence `846 -> 847`, with gross
`455.679279 g -> 0.387464 g`. This is a data-inferred time bracket later
confirmed by the user; the user's message time is not treated as an exact
physical timestamp.

## Qualification conclusion

The cross-record union is `0x12F | 0x0DF = 0x1FF`, and all shared safety and
comparability checks pass. However, the frozen single-epoch contract forbids
using this union to claim `Checker.complete()` PASS. Therefore:

```text
CROSS-RECORD NINE-PATH ENGINEERING EVIDENCE: PASS
SINGLE-EPOCH RESOURCE QUALIFICATION: INCOMPLETE
0x0524 FOCUSED HARDWARE VALIDATION: NOT RUN BY CONTRACT STOP
```

The exact 0x0524 candidate remains unflashed. A13B long-duration efficacy is
unchanged as `INCONCLUSIVE`; migration, SAVE, power-cycle, UI, battery and
formal metrology gates remain deferred.
