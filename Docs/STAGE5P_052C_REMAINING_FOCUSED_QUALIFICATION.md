# Stage 5P 0x052C remaining focused qualification

## Scope

This branch starts at 0x052C SAVE-continuity closure commit
`3f2f72e51e189082b72c7d3ccad12279cf2f4bbc` and does not change product
code or firmware. The 0x052A and 0x052B `FAIL_ROLLED_BACK` results remain
unchanged. The 0x052C SAVE continuity result remains PASS, and A13B
long-duration efficacy remains INCONCLUSIVE. No new 12-hour run or full
500 g matrix was performed.

## Read-only preflight

The preflight at 2026-10-14 11:22:28 UTC read 0x052C/Map 0x010F,
40 Hz/filt1/strength3, valid calibration raw zero/span 41868/485780 with
500 g span, R5 ACTIVE+STATIC and checkweigh STATIC. The volatile R5 offset
was -17,565 ug at that instant. Fault and overrun were zero, and the
saved V3 state was slot A sequence 23, revision/saved 23/23. Later
temporary no-SAVE operations made RAM revision 25/23 dirty=1; this was
deliberately restored by the authorized power cycle.

## Same-sample weight evidence

The public 0x0340 block contains the authoritative sample sequence,
uncompensated gross, applied offset, formal gross/net and tare. The
0x02E0 block contains the shadow static/dynamic inputs and its own sample
sequence. Records were accepted as same-sample only when those sequence
values were equal; no interpolation was used.

| Capture | Rows | Same-sequence | Nonzero offset | Static input = formal net | Formal gross equation |
| --- | ---: | ---: | ---: | ---: | ---: |
| Static, before mode change | 89 | 28 | 28 | 0 mismatches | 0 mismatches |
| Dynamic, temporary RAM mode | 134 | 38 | 38 | 0 mismatches | 0 mismatches |

The dynamic captures show a nonzero offset and valid formal dynamic
classification, but 0x052C does not expose an independent **uncorrected**
fast calibrated gross register. The public dynamic input is already the
fast path after the applied offset and tare. Therefore the exact equation
`fast_gross - applied_offset - tare` cannot be independently proven from
this unchanged image. Dynamic same-sample numeric qualification is
**INCOMPLETE**, not PASS. The temporary DYNAMIC request was not saved and
was restored to saved STATIC by the later power cycle.

## Panel observations

These are user visual observations, not electrical LED measurements:

| Page | Display | Top LEDs, left to right | Bottom LEDs, left to right |
| --- | --- | --- | --- |
| NET before tare | 0.43 | 1, 4 | 1, 5 |
| GROSS before tare | 0.43 | 2, 4 | 1, 5 |
| Battery | 42P | 4, 6 | 1, 5 |
| NET after tare | 0.00 | 3, 4 | not reported |
| GROSS after tare | 0.43 | 3, 4 | not reported |

The user reported the top zero LED (left 5) off on both NET and GROSS,
and bottom LED 4 (`*`) never lit. The top tare LED (left 3) was on after
tare. A post-observation read showed formal net -15,292 ug, stable false,
zero false, tare inactive after the page session; fault and overrun were
zero. The earlier interpretation that the zero bit was set was corrected:
registers 0x0004 and 0x0005 are separate status words, not one 32-bit
value. The final 45-second zero-status capture had 450 valid reads,
stable true throughout, zero bit false throughout, no faults/overruns,
net range -71,514 to 44,500 ug, and its longest observed stable interval
inside +/-20,000 ug was only 1.6 s across host polls. Since host polling
does not cover every 40 Hz sample, this does not prove continuous entry
qualification. Zero-lamp behavior remains **INCOMPLETE/NOT QUALIFIED**;
no threshold or firmware change was made.

## Physical power-cycle early state

A reconnectable read-only recorder ran before the user-confirmed physical
power cycle and was stopped after post-boot qualification. The raw CSV
contains 64,346 rows, SHA-256
`764DA1C9A5837A11581F3F534E933FC4C0E0C25977907B271071FC327DBCBC10`,
with 48 expected serial-disconnect/read errors during the power break.

The recorded reset boundary is unambiguous: immediately before it,
sequence 875,501 and uptime 21,925,697 ms were observed with R5 offset
-305,550 ug, runtime revision/saved 25/23 and dirty=1. The first visible
post-reset records had sequence=0, uptime=0, R5 state OFF, offset=0,
checkweigh formal output off, revision/saved 23/23 and dirty=0. This is
the first observed post-reset state; host timing cannot claim an unseen
MCU sample before it.

The first post-reset row with a nonzero sample sequence was at uptime
541 ms. Formal checkweigh qualification was first observed at uptime
2,770 ms, sequence 91, while R5 was in step-settling and offset remained
zero. The post-reset trace later covered R5 states OFF, STEP_SETTLING,
STEP_PENDING, HOLDOFF and REFERENCE_FILL. No post-reset nonzero offset
was observed until uptime 70,427 ms; this confirms offset was rebuilt in
RAM rather than restored from Flash. The trace did not reach TRACKING
within the captured early window. The final read-only state after the
temporary RAM changes and power cycle was 0x052C/0x010F, 40 Hz/filt1/3,
R5 ACTIVE+STATIC, checkweigh STATIC, slot A sequence 23, revision/saved
23/23, dirty/fault/overrun 0 and offset -2,939 ug.

Physical power-cycle early transition: **PASS for the observed boundary,
default-off offset reset and request/config restoration**. “First MCU
sample” in the absolute sense is **INCOMPLETE** because the recorder can
only report the first response it observed.

## Invalid-input safety

Source audit found the only bounded valid=false entry in
`Diagnostics/stage5na3_fault_injection.c`, enabled only by the dedicated
Stage5NA3 diagnostic build. The exact 0x052C image has no safe public
target command that activates this injection. No disconnection, short,
overload or direct output-variable manipulation was used. Formal invalid
input behavior therefore remains **NOT RUN** on target. Existing Host
and diagnostic-build evidence is not relabeled as an exact 0x052C
hardware result.

## Artifacts and reproducibility

Raw same-sample captures, reconnectable power-cycle frames/errors,
panel observations, pre/post snapshots, and SHA manifest are under
`Results/stage5p_052c_remaining/`. Recompute the offline summary with:

```text
python -B Tools/stage5_unified/analyze_remaining_052c.py Results/stage5p_052c_remaining --output Results/stage5p_052c_remaining/analysis.json
```

Final device identity is 0x052C/0x010F; its saved configuration is
sequence 23 with both slots valid from the prior closure evidence. No
SAVE, firmware change or configuration write was performed in this
remaining-qualification session. Full Host CTest remains INCOMPLETE,
cross-sensor and formal metrology qualification remain NOT RUN, and
long-duration efficacy remains INCONCLUSIVE.
