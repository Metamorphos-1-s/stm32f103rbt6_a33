# Stage 5M-R5B Reference-Lock Engineering Beta

## Baseline

The requested remote branch `stage5mr5-reference-lock-offline` was not present
in the authenticated GitHub repository. The expected commit
`5892297eddc09f9a76e67a2b9fc968b64e005b63` was therefore not available. R5 was
rebuilt from the pushed R4 baseline
`e6665f2056eeacb46208483dfbc106d2b45d6e44`.

R5 is distinct from both the product `RuntimeDriftCompensator` and the earlier
Host-only `StaticDriftCompensator`. The R3/R4 failed candidate is not enabled.

## Frozen contract

This engineering beta is limited to the current 3 kg sensor, e=0.05 g,
Profile 0 and 10 Hz. Input is calibrated uncompensated gross mass in ug. Raw
ADC counts remain available for hardware diagnostics and are not used as
cross-sensor mass thresholds.

The 10 Hz stream is reduced to robust 1 Hz samples. An exact 300+600 value
implementation required 4,016 bytes of state and caused the STM32F103RB image
to exceed 20 KB RAM by 2,936 bytes. That first reconstruction is retained as
superseded evidence and is not the Beta implementation.

The RAM-qualified Beta groups each ten 1 Hz values into a median. It stores 30
reference-block medians and 60 rolling observation-block medians as signed
32-bit deltas from a 64-bit base. Rate and integration retain 1/1000 ug
resolution. Division truncates toward zero. Median averaging also truncates
toward zero.

OFF clears and does not apply the R5 offset. DOSING_NO_COMPENSATION applies the
existing offset but freezes all learning. STATIC_COMPENSATION preserves the
offset, waits 15 seconds, builds a 300-second locked reference, then evaluates
a rolling 600-second observation median every 60 seconds.

## Offline reconstruction

The exact-window reconstruction reproduced the requested result within the
published rounding precision, but is not linkable on this MCU. After adopting
the documented block-median structure, all offline gates were rerun. The six R4
captures produce median OLS improvement 9.94%, median endpoint improvement
19.69%, minimum endpoint improvement 4.37%, zero formal reverse amplifications,
and maximum 10-second offset change 0.000500 g.

The adverse second 500 g result is retained: raw -0.0193847 g/h, corrected
-0.0213215 g/h, OLS change -9.99%, endpoint improvement +4.37%.

The synthetic 12-hour scenario produces 0.203058 g final offset, 0.016942 g
loaded/unloaded residual, 500.00 g and 0.00 g displays at e=0.05 g, and
0.000051 g maximum 10-second correction. This is synthetic behavior evidence,
not a real 12-hour qualification.

Five real load/unload cycles produce exactly ten automatic rebases. All twelve
slow-dosing combinations preserve the full mass change with zero offset change.

## Qualification boundary

This is a current-sensor engineering beta only. The 2-4 hour qualification,
real 12-hour qualification and cross-sensor validation remain deferred. It is
not approved for legal metrology or production acceptance.
