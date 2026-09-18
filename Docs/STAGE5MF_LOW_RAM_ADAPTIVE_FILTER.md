# Stage 5M-F Low-RAM Adaptive Filter

## Conclusion

**STAGE 5M-F NO ACCEPTABLE CANDIDATE; PRODUCT AND BETA OUTPUT UNCHANGED;
STAGE 5N/5O ENTRY REQUIRES EXPLICIT DECISION.**

The stage stopped at the required offline gate. It did not open a new holdout,
implement target C, change Firmware `0x0512`, integrate Beta SHADOW, flash the
device, or alter the official measurement/R5 paths.

## Audit and data discipline

The official 10 Hz chain and RAM boundary are documented in
`STAGE5MF_PIPELINE_AND_RAM_AUDIT.md`. Formal WeightEngine output, stability,
display, PLC/BLE/alarm, ZERO/TARE and R5 input all remain on the existing
MEDIAN3_IIR strength-3 path. R5 blobs and parameters are unchanged.

Every Stage 5M-A tuning run is DEVELOPMENT. Former holdouts are OPENED
REGRESSION and were not used for ranking. Because no development candidate
passed, no parameter/result-script freeze was promoted and no NEW HOLDOUT was
captured. The Stage 5M-A historical result remains `NO ACCEPTABLE CANDIDATE`.

## Candidate attempt

The attempted O(1) model has PRECISION, FAST and SETTLING states. Planned C
state was at most 48 B: fast mass, continuous candidate output, previous input,
signed motion EWMA, timestamp/sequence, counters and flags. It has no history
array, floating-point product path, mass attraction, dynamic allocation or R5
feedback. Explicit DOSING forces FAST and forbids candidate stable.

Two 729-combination DEVELOPMENT searches were preserved. The first exposed an
unsuitable filt2 retry step run and was not used as a qualification result. The
second used explicit DEVELOPMENT filt0 load/unload captures.

Best failed DEVELOPMENT result:

| Metric | Result | Gate |
|---|---:|---:|
| Static noise | 0.005370 g | <=0.0068 g PASS |
| Static motion false-positive | 0.00% | <=1% PASS |
| Load 10-90% | 0.561 s | <=0.60 s PASS |
| Unload 10-90% | 0.694 s | <=0.60 s FAIL |
| Stable time | 10.662 s | <=3.0 s FAIL |
| Automatic slow-fill false-stable | 61.20% | <=10% FAIL |
| Explicit DOSING false-stable | 0% | 0% PASS |
| Maximum transition jump | 0.004224 g | <=0.01 g PASS |
| Load/unload overshoot | 0.014644/0.010139 g | <=0.05 g PASS |

Opened regression also failed: load 0.622 s, stable 9.737 s and automatic
slow-fill false-stable 38.68%. The historical robust dual IIR remains worse for
RAM (256 B) and also fails stable/slow/static-false-positive gates.

The result demonstrates the unresolved single-signal ambiguity: aggressive
quiet detection improves response but declares stability during slow or paused
addition; conservative detection avoids that error but cannot meet the 3 s
stable gate. Explicit DOSING solves the declared-process case, not automatic
slow-fill classification.

## Product and safety state

No product source, CMake target, register, map, schema, persistent format,
firmware identity or binary changed. R5E remains Firmware `0x0512`, default
OFF+SHADOW. Its BIN remains 100,792 B with SHA-256
`BA8F02B2024042D601FD7F2D75BEF9E1004AACAE16852DD97CD2B28777BAF6B9`.
Standard Release remains
`82E726F5B32A0DE36A5E686F62A937EC4FD9CBB488DB9733E83D2062673EF486`.

R5D/R5E evidence is preserved. Target RAM/stack and CPU measurements are not
claimed for the rejected Python candidate. Python/C candidate parity, new
holdout and hardware SHADOW are **NOT RUN** by design. ASan/UBSan remain **NOT
RUN** because the portable runtimes are unavailable.

## Next decision

Do not tune the failed model on opened regression data. A future attempt needs
either an explicit process-active contract for all slow/intermittent addition,
a genuinely independent auxiliary process signal, or a revised stable-time
requirement. Stage 5N/5O entry requires an explicit product decision and is not
authorized by this result.
