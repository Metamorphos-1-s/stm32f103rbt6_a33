# Stage 5P-A4: warm 500 g loading and zero-return attribution

## Verdict and scope

**DATA CAPTURE PASS FOR THREE CONTINUOUS EDGES; FIRST LOAD EDGE MISSING;
MODEL SELECTION INCOMPLETE.** This is development/mechanism evidence, not an
independent holdout or proof of load-cell creep. A4 is based on A3 HEAD
`d0857440d12364f716be2f51f4bb3e27f4678ff5`, branch
`stage5pa4-hot-500g-attribution`. No firmware, R5/display algorithm, filter,
calibration, ZERO/TARE, SAVE, or rate change was made. The device remains
0x051C, Map 0x0104, valid calibration raw zero -44003/raw span -487921 /
500,000,000 ug, 10 Hz/filt3/strength3, R5 OFF+SHADOW, checkweigh OFF;
revision/saved 15/15, dirty/fault/overrun 0, SAVE counter 6. End-of-run COM5
read-only postflight confirmed this state. Temperature: **not measured**;
there was no confirmed sensor or manual reading.

## What was actually recorded

The unmodified read-only `Tools/stage5na/stage5na2_hw.py` was invoked with
`--expected-firmware 0x051C` (its default is an older 0x0515 identity). A
10-second pre-record validated UTC, host monotonic nanoseconds, MCU uptime,
sample sequence, raw ADC, filtered *raw ADC counts*, measured authoritative
`gross_ug`, panel `display_count`, conditioned display and display anchor.
**Filtered mass in ug and an independent uncompensated diagnostic mass are not
mapped by this recorder**: the former is `null`, and the measured gross is
used as authoritative uncorrected weight only because actual R5 stayed OFF.
The recorder's primary, R5, display, and shadow register blocks are read at
different instants, not an atomic ADC snapshot. Do not infer missing fields.

| Dataset (UTC, September 25, 2026) | Records | Read errors | Max host poll gap |
|---|---:|---:|---:|
| Empty 07:02:11–07:32:12 | 3,596 | 0 | 0.547 s |
| First load already on pan 07:38:10–08:05:02 | 3,222 | 0 | 0.547 s |
| Continuous recorder 08:05:05–09:30:31 | 10,244 | 0 | 0.547 s |

The continuous recorder was configured for a three-hour maximum and stopped
gracefully at the user's request after 85 min 26 s. There are no >2 s polling
gaps, MCU uptime/sequence regressions, device faults, overrun increments, R5
offset changes, config revisions, or additional SAVE requests in any dataset.
The first load **was not** captured: the empty recorder ended at 07:32:12,
the operator confirmed placement at 07:38:10.706, and the next recorder's
first sample at 07:38:11 was already loaded. There is a roughly six-minute
unrecorded gap. A second recorder change at 08:05:02–08:05:05 adds another
approximately three-second gap while still loaded. These records must not be
spliced into a fictitious continuous file. The first load's 1/2/5/10/15/30
**event-relative** checkpoints are NOT SCORABLE.

## Physical transitions and event-time uncertainty

The `event_timeline.csv` contains the human confirmation separately from the
bracket of adjacent ~0.5 s host polls that crossed 250 g. The raw ADC's largest
nearby jump occurs in a different Modbus read block; its bracket is independent
and is **not** a DRDY/electrical timestamp. Human messages arrived 27–40 s
after the measured transitions, and cannot be used as exact edge times.

| Event | Gross midpoint bracket UTC | Human confirmed UTC | Delay | Stable before-to-15–45 s step |
|---|---|---|---:|---:|
| Load 1 | **not captured** | 07:38:10.706 | unknown | not identifiable |
| Unload 1 | 08:06:52.630–53.104 | 08:07:20.207 | 27.109 s | -500.009 g |
| Load 2 | 08:37:12.762–13.237 | 08:37:46.857 | 33.625 s | +499.997 g |
| Unload 2 | 09:10:49.382–49.857 | 09:11:29.888 | 40.031 s | -500.017 g |

The gross brackets are at most 0.475 s wide **at host polling resolution**;
the actual placement/removal and ADC conversion may fall outside a naive
instantaneous interpolation because weight filtering introduces latency.
Steps above use the 5–30 s pre-edge median and 15–45 s post-edge median;
they are stable-window load/unload span estimates, not a single-sample jump.

## Early motion separated from transient

Each continuous segment uses its own 15–45 s *after observed midpoint*
median, then medians of the following full minute at each checkpoint. The
first-load row alone uses 15–45 s *after recording began*, explicitly **not**
after actual placement. Values below are changes in measured gross from each
row's early reference, in mg. `NA` means a complete one-minute window is
absent, not a zero trend.

| Phase | Early gross | 1 min | 2 min | 5 min | 10 min | 15 min | 30 min |
|---|---:|---:|---:|---:|---:|---:|---:|
| First loaded segment, **start-relative only** | 499.989863 g | +18.021 | +27.032 | +23.653 | +12.953 | +3.379 | NA |
| Unload 1, event-relative | -0.027032 g | -15.769 | -31.537 | -39.422 | -58.569 | -59.696 | NA (only 20 s) |
| Load 2, event-relative | 499.924536 g | +36.042 | +51.811 | +52.937 | +36.042 | +143.044 | +150.365 |
| Unload 2, event-relative | +0.033790 g | -18.021 | -34.916 | -76.591 | -107.565 | -140.792 | NA |

The first recorded loaded segment rises by 24 mg at the *recording-relative*
five-minute point but largely loses that change by minute 15; its precise
event-relative drift is unknown. The second load rises 150 mg by minute 30.
Both measured unload phases fall (about -60 mg and -141 mg by minute 15),
with distinctly different amplitudes. These are directionally compatible
with a load/recovery history but not repeatable at one common magnitude.
Unload 2 has only ~19 min 41 s of post-edge data by stop time, less than the
requested 20 minutes; a 30-minute recovery point is NOT RUN.

The earlier 30-minute empty run changes from a first-five-minute median of
+0.009011 g to a last-five-minute median of -0.018021 g, or -27.032 mg over
approximately 25 minutes between window centers. A **linear extrapolation**
of -1.081 mg/min yields at 5/15/30 minutes respectively an empty-control
change of -5.407/-16.222/-32.443 mg. Subtracting it changes the load-2
gross changes at 5/15/30 minutes from +52.937/+143.044/+150.365 mg to
+58.344/+159.266/+182.808 mg; unload 1 at 5/15 min from -39.422/-59.696
to -34.015/-43.474 mg, and unload 2 from -76.591/-140.792 to
-71.184/-124.570 mg. This is **not** a simultaneous unloaded control:
temperature was not measured, empty drift can change sign, and extrapolation
cannot isolate a sensor mechanism. Raw ADC and filtered ADC counts move with
gross: load 2 at 30 minutes is +150.365 mg gross, **-134 raw ADC counts**
and **-133.5 filtered ADC counts**. Panel display-count median changes by
+16 counts (0.16 g), but is listed separately and never fitted as
mass. Panel midpoint was observed ~0.234 s after the gross poll-bracket
center; panel 90% of the 500 g step appeared ~1.25–1.77 s later. This
resolution-limited holding/filter/quantization behavior explains some visual
lag, not the entire minutes-long raw/filtered/gross movement.

## Counterfactual R5 and mechanism decision

Actual R5 was OFF throughout, offset/reference zero. The offline replay uses
one `ReferenceLock` instance for all 10,244 continuous rows, one-second medians
and **no reset between unload and load segments**. The declared hypothetical
schedule is STATIC at recorder start (already loaded), DOSING 2 s before each
of the three retrospectively detected edges, STATIC again 60 s after each.
It preserves inherited offset and records no DOSING offset change; final
simulated offset is +30,274 ug. Because those switches are chosen *after* seeing
the edges and 10 Hz individual samples were not replayed, this is a contract
exercise, **not** a causal live-controller comparison or hardware PASS. No
ZERO, calibration, filter or rate switch took place to validate transitions.
The reproducible `r5_counterfactual_trace.csv` includes every replayed second's
mode, uncorrected input, corrected output, offset, state and rebuild count;
`analysis.json` reports 1/2/5/10/15/30 minute corrected-window checkpoints.
Checkpoints are clipped to each physical phase, so the incomplete first-unload
30-minute window is explicitly NOT RUN rather than contaminated by load 2.

A unified robust reference could potentially follow the loaded and unloaded
baselines without load-event metadata, but on these records it could also
cancel true low-rate mass changes or common zero drift. An event-history
controller can explicitly preserve the 500 g step, but cannot reconstruct the
missing first placement time and cannot uniquely attribute post-unload
recovery to load history. Neither model has an independently identified
temperature/zero component here; no candidate constants are chosen. Any later
comparison must preserve DOSING offset freeze, exact physical span, ZERO and
calibration resets, profile/rate transitions and the correction-speed bound.

Evidence favoring an event-associated component: the sign reverses after
placement/removal and appears in ADC counts as well as gross; a full ~500 g
span survives each observed transition. Evidence for common warm-zero drift:
the earlier empty-only run drifts negative, and the opened A3 cold-start
records have opposite directions. Mechanical settling/recovery of pan,
fixture, or load cell is consistent with the opposite post-unload sign, but
not isolated from thermal/electronics effects. No ambient temperature was
measured and there is no simultaneous unloaded reference sensor. The missing
first load edge and incomplete second unload hold further limit attribution.

The present checkweigh OFF setting is the earlier saved A2D configuration;
it was not separately polled as a requested-mode register during A4. A4
read-only formal output status stayed OFF and saved revision remained 15/15.

**MODEL SELECTION INCOMPLETE.** Do not tune an event-history or unified
robust-reference product algorithm using these records and then claim the same
records are independent holdout. The minimum discriminating data are a
timestamped continuous empty→500 g→empty sequence repeated twice with no
recorder handoff at any physical edge, at least 30–60 min loaded and 20 min
unloaded per cycle, a matched empty-only thermal control and actual temperature
reading if available. If a counterfactual is developed, keep DOSING offset
strictly frozen, preserve physical step, bounded 10-second correction, and
explicit ZERO/calibration/profile-switch reset rules.

Reproduce locally without contacting hardware:

```text
python Tools/stage5pa4/analyze_hot_cycles.py --input Results/stage5pa4/thermal_cycle --output Results/stage5pa4/analysis.json
python Tools/stage5pa4/test_analyze_hot_cycles.py
```

All raw CSVs, events, failed first-load boundary assumption and partial
segments remain archived. No 12-hour, 10/40 Hz/filter, alarm or new RAM
hardware qualification was run; no merge, tag or PR was created.
