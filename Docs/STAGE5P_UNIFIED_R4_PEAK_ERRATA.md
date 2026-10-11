# Stage 5P Unified-R4 peak-summary errata

This additive erratum preserves the original R4 report and raw evidence.

The R4 report quoted an early polling frame rather than the final cumulative
maximum. Direct recomputation over all 2,715 rows in
`resource_capture_r4b/polls.csv` and the final target snapshot gives:

| Metric | Correct final maximum | 72 MHz conversion |
| --- | ---: | ---: |
| A13C Feed | 672,128 cycles | 9.335 ms |
| App_Run | 1,027,997 cycles | 14.278 ms |
| Main-loop interval | 1,028,049 cycles | 14.278 ms |

The final snapshot identifies the Feed peak as path index 2,
`REFERENCE_FILL`, sequence 1,347. The final cumulative App_Run and interval
values are from the same diagnostic epoch `351272961`; the target clock is
72 MHz. The original smaller values (82,166 / 475,028 / 475,080) were valid
intermediate polling values, but not the declared capture maxima.

All corrected values remain below the 10 ms Feed and 25 ms App_Run/interval
limits, although Feed has only about 0.665 ms of headroom. This must not be
described as a wide margin.
