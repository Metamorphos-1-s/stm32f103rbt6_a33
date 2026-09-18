# Stage 5M-R5D 500 g 12-Hour Long-Duration Qualification

## Result

**STAGE 5M-R5D 12-HOUR SAFETY QUALIFICATION PASSED; CORRECTION EFFICACY
INCONCLUSIVE DUE TO LOW NATURAL DRIFT.**

This is not a full efficacy PASS. All safety gates passed, but the configured
legal verification interval is `e = 1.000 g` and the observed 12-hour
uncompensated endpoint drift was `+0.174604 g`, below one e. The active g
display division is `0.01 g`; the `0.05 g` value anticipated by the task is the
stability-enter threshold, not e. Against the stricter display division, the
drift stimulus was ample, but corrected endpoint drift `-0.039216 g` exceeds
half a display division (`0.005 g`). These facts are reported together rather
than selecting the more favorable criterion.

## Frozen baseline

The branch starts at R5C commit
`6440c409bb3454619b96d152921b7e09f6ebaf6f`. Firmware is `0x0511`, Map is
`0x0104`, and R5 signature is `0x55B5`. The unchanged 99,440-byte Beta BIN is
SHA-256 `2494CB70923C67E38F1642285CF3CEE717C1A207C42BF97B96FCC2A3BCFBDEFC`.
The standard Release ELF remains
`82E726F5B32A0DE36A5E686F62A937EC4FD9CBB488DB9733E83D2062673EF486`.
No firmware, algorithm, parameter, filter, calibration, Flash configuration or
SAVE operation changed in R5D.

The immutable qualification evidence head is
`ebb28efff4e9dfd500f225f81b7ccd9c174ec050`. This report commit and the final
portable-manifest commit follow it; the delivery HEAD is reported by Git and
the final handoff rather than attempting a self-referential commit hash.

The installed sensor is the existing 3 kg C3 load cell and the test load was a
500 g standard mass. Active configuration is g, two decimal places, division
digit 1, 10 Hz, gain 128, `MEDIAN3_IIR`, strength 3, window 8 and 1000 ms hold.
Calibration was valid. Electronics had been continuously powered for 26.12 h
before the preflight SWD configuration read; that read reset the MCU but did
not remove sensor/electronics power. Room temperature was not measured.

## Timeline and completeness

| Phase | UTC | Result |
|---|---|---|
| Empty OFF+SHADOW baseline | 2026-09-17 18:31:28 to 18:41:28 | 600 records, complete |
| Initial SHADOW+DOSING loaded | 2026-09-17 18:44:44 to 18:45:43 | 60 records, offset frozen |
| SHADOW+STATIC reference build | 2026-09-17 18:46:03 to 19:02:04 | HOLDOFF -> REFERENCE_FILL -> OBSERVATION_FILL -> TRACKING |
| ACTIVE+STATIC qualification | 2026-09-17 19:03:32 to 2026-09-18 07:03:32 | 43,200.052 s, 43,200 records |
| ACTIVE+DOSING unload | 2026-09-18 07:15:52 to 07:45:52 | 1,800.034 s, 1,800 records |
| User removal confirmation | 2026-09-18 07:16:52.152 | Captured in unload record |

The ACTIVE run has 100.0% nominal record coverage, maximum host sample gap
1.086 s, zero read errors, reconnects and host polling gaps, and no uptime
regression. It remained ACTIVE/STATIC/TRACKING/non-limited for 43,200 of 43,200
records. No automatic reference rebuild occurred. The 71,064,000-byte raw frame
file is the largest artifact and remains below the 95 MB limit.

## Robust results

All values below use medians, not individual samples.

| Metric | Result |
|---|---:|
| Empty baseline, final 5 min | -0.099130 g |
| Empty baseline MAD / peak-to-peak | 0.004506 g / 0.041680 g |
| ACTIVE first 5 min, uncompensated | 500.042806 g |
| ACTIVE first 5 min, corrected | 500.041252 g |
| ACTIVE final 5 min, uncompensated | 500.217410 g |
| ACTIVE final 5 min, corrected | 500.002036 g |
| Uncompensated endpoint drift | +0.174604 g |
| Corrected endpoint drift | -0.039216 g |
| Absolute-drift improvement | 77.540% |
| OLS uncompensated trend | +0.008885 g/h |
| OLS corrected trend | -0.002387 g/h |
| OLS offset trend | +0.011273 g/h |
| Final 5 min offset | +0.215374 g |
| Maximum absolute offset | 0.215374 g |
| Maximum 10 s offset change | 0.000500 g |
| Unloaded 2-5 min uncompensated / corrected | +0.144189 g / -0.070925 g |
| Unloaded final 5 min uncompensated / corrected | +0.093498 g / -0.121616 g |
| Final zero residual versus initial empty, uncompensated | +0.192628 g |
| Final zero residual versus initial empty, corrected | -0.022486 g |
| DOSING load/unload step, uncompensated | 500.085613 g |
| DOSING load/unload step, corrected | 500.085613 g |
| Step loss | 0.000000 g |

Hourly medians are preserved in `hourly_summary.csv`. Uncompensated hourly
median rose from 500.050692 g in hour 1 to 500.241066 g in hour 12. Corrected
hourly median was 500.037390 g in hour 1 and 500.035992 g in hour 12. The
endpoint five-minute windows are authoritative for the qualification metric;
OLS is reported independently and is not substituted for the endpoint result.

## Safety gates

Across ACTIVE and unload records: fault and overrun were zero; dirty and SAVE
request count were zero; revision/saved revision remained `7/7`; firmware,
Map and R5 signature remained fixed; uncompensated gross remained readable.
ACTIVE never left TRACKING. Maximum offset was below 0.500 g and maximum 10 s
change was below 0.001 g. During all 1,800 DOSING samples offset had exactly
one value, `215114 ug`. The compensated and uncompensated load/unload steps
were identical, so no physical step was swallowed.

Configuration SHA-256 before and after the workflow was identically
`D74C98D8D4221437773155E8D1ED75BC59D71AE2D285D5C2F18F6B494AA5DC86`.
Final device state is OFF+SHADOW with offset/reference/evaluation zero,
fault/overrun/dirty/SAVE zero and revision/saved revision `7/7`.

## Software and evidence

Host CTest passed 25/25. Python/C parity passed 25,057 samples (22,557 real)
with zero mismatch. R2/R3/R4/R5, Stage 5B/5C/5L, R5D and Manifest unit tests
passed. Debug, Release, Beta and Beta `-Wextra -Werror` builds passed; Release
and Beta hashes remained frozen. ASan and UBSan are **NOT RUN** because the
portable MinGW runtimes are unavailable. pytest is also unavailable; this is
not reported as a test PASS or firmware failure.

Cross-sensor validation, metrology certification, ASan and UBSan remain
**DEFERRED**. The real 12-hour test is no longer deferred; its result is the
low-natural-drift safety qualification stated above.
