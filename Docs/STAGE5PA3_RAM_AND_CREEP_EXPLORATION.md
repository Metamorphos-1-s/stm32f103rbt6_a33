# Stage 5P-A3: RAM Reuse And Opened Creep Evidence

## Status

```
0x051C BASELINE PRESERVED;
RAM REUSE HOST CANDIDATE READY, TARGET ARM MAP NOT RUN;
EARLY CREEP MODEL SELECTION INCOMPLETE;
NO FIRMWARE FLASH OR ACTIVE ALGORITHM CHANGE.
```

The starting commit is `d6312dd9898ba8f01a43cd83a76fa41546d88275`,
the flashed 0x051C engineering build. This branch changes two RAM workspaces
and one focused storage regression. R5 parameters, its C and Python models,
display behavior, Checkweigh, Modbus mapping, persistent format, and the
device's 0x051C image are unchanged. This analysis never touched the device.

## RAM: Measured Baseline And Candidate

The exact 0x051C Debug ARM map ends static RAM at `0x20004808`. RAM ends at
`0x20005000`, so there are 2,040 bytes between static data and stack top.
Subtracting the previously audited 1,504-byte conservative stack demand
leaves 536 bytes. The 1,024-byte linker minimum stack is already contained
within the 19,464-byte linked RAM figure. Lowering that number would not
make the conservative stack bound smaller.

| Object in exact 0x051C ARM map | Before | Candidate |
|---|---:|---:|
| A and B temporary payloads | 281 B + 281 B | one 281 B scratch; valid A copied into existing active payload before B is read |
| Factory configuration and candidate target | 344 B + 344 B | one 344 B union, protected by the existing single-operation gate |
| **Projected static-object saving** | | **625 B** |

The projection assumes the stack bound and padding are unchanged; its
**1,161-byte projected collision margin is not an ARM build result**. The
actual target map, worst main/IRQ stack bound, and the ≥512-byte gate remain
`NOT RUN`. No UART2/UART3 DMA ring, Modbus server, BLE transport capacity,
R5 history, or Flash data format was reduced.

On the host, `gcc -std=c11 -Wall -Wextra -Werror` compiled both changed
translation units with 0x051C product defines. Existing Stage 4B fake-Flash
storage tests passed, including the 104-cut power-loss loop and B-newer or
B-corrupt recovery. One new test exercises the previously uncovered case of
both valid slots with A newer. Existing persistence manager tests passed in
both legacy and product-defined host builds; the latter used host-only
R5/Checkweigh restore stubs and therefore does not qualify target I/O.
Host `nm` confirmed removal of one 281-byte payload and one `DeviceConfig`
object. The host `DeviceConfig` occupies 384 bytes due to ABI layout; the
**344-byte** figure comes from the actual baseline ARM map.

The two focused host suites also passed with AddressSanitizer and
UndefinedBehaviorSanitizer. LeakSanitizer cannot run under this environment's
ptrace wrapper, so those runs used `ASAN_OPTIONS=detect_leaks=0`; leak
detection is **NOT RUN**, and these focused suites do not replace the full
project sanitizer or target gates.

The full 41-test Host Debug/Release CTest, ARM Debug/Release/strict builds,
candidate MAP, target stack analysis, and hardware tests were **not run** in
this environment (ARM compiler and CMake are unavailable). These gates cannot
be inherited from 0x051C after source changes.

Reproduce the baseline projection and compare the exact candidate map after
building with the original ARM toolchain:

```sh
python Tools/stage5pa3/ram_map_audit.py \
  --output Tools/stage5pa3/ram_baseline_projection.json
python Tools/stage5pa3/ram_map_audit.py \
  --candidate-map path/to/candidate_debug.map \
  --output path/to/candidate_ram_result.json
```

`candidate_target_build` must change from `NOT RUN` to an actual gate result
before this RAM change can be accepted into a new firmware build.

## Opened Data: Early Behavior Is Not Uniform

`Tools/stage5pa3/opened_creep_analysis.json` was generated using CSV blobs
pinned to the 0x051C commit. Every alternate checkout file was compared with
the corresponding Git blob ID before reading. All six runs are **opened
development data**. The initial value is each run's 15–45 second median; it
is not an independently known true mass. The R5 column is an offline replay
from the frozen Python model, starting STATIC at the first record and using
one-second medians of the recorded 10 Hz mass. It is an illustrative model
comparison, not a byte-for-byte replay of the device's historical mode
transitions or sample-level admission. Each
checkpoint is a median of the following 60 seconds, not an instantaneous
sample. The time origin is recorder start, not necessarily the precise
moment when the weight touched the pan.

| Run and checkpoint | Raw change from early reference | Frozen R5 replay change | R5 replay offset |
|---|---:|---:|---:|
| 500 g, 30 min: 5 min | +0.046186 g | +0.046186 g | 0 |
| 500 g, 30 min: 10 min | +0.051255 g | +0.051255 g | 0 |
| 500 g, 30 min: 15 min | +0.057451 g | +0.056983 g | +0.000286 g |
| 500 g, 30 min: 25 min | +0.038864 g | +0.030659 g | +0.008213 g |
| Unloaded zero, 15 min: 5 min | −0.033513 g | −0.033513 g | 0 |
| Unloaded zero, 15 min: 10 min | −0.063083 g | −0.063083 g | 0 |
| R4 500 g constant run A, 1 h: 5 min | +0.019713 g | +0.019713 g | 0 |
| R4 500 g constant run A, 1 h: 25 min | +0.011828 g | +0.011828 g | 0 |
| R4 500 g constant run B, 1 h: 5 min | +0.001126 g | +0.001126 g | 0 |
| R4 500 g constant run B, 1 h: 25 min | −0.009012 g | −0.009012 g | 0 |
| R4 cold empty, 1 h: 5 min | −0.056887 g | −0.056887 g | 0 |
| Stage 5L cold empty, 1 h: 5 min | +0.284436 g | +0.284436 g | 0 |

In the 30-minute loaded record, the 300–360 s versus 15–45 s median change
is −40 raw ADC counts, −43 filtered counts, +0.048439 g filtered mass, and
+0.081107 g conditioned display mass. Thus the early change is visible in
the measurement path, while the display path also changes its size; treating
every panel change as R5 drift would conflate those paths. This is an
inference from opened data, not a causal isolation of sensor, fixture,
temperature or electronics.

Even without a load event, the two cold-start empty runs move in opposite
directions. Their five-minute changes from their *own* early baselines are
−0.056887 g and +0.284436 g. These records are a direct reason to keep
warm-up/common zero drift separate from a hypothesized load-dependent
response. The previous R5D 12-hour hardware record measured +0.174604 g
uncompensated and −0.039216 g corrected end-to-end drift, but it did not
identify a load-specific early response or independently validate any new
algorithm.

The previously frozen Stage 5P-A `EARLY_TRACKING` report remains `REJECT`:
`r4_cycles` and `filt2_load` have zero improved 5/10/15-minute checkpoints.
As another *diagnostic only*, a one-exponential curve fitted on minutes 1–10
of the 30-minute loaded run has 0.010869 g median absolute residual on
minutes 15–25 (raw 0.049284 g relative to its initial reference). Applied
to the R4 loaded run B under the same fit/check schedule, its residual is
0.009928 g, **worse** than the raw 0.005069 g. The unloaded recovery chooses
a different, slow 3,600 s time constant within the exploratory grid. These
results do not support committing a universal exponential or a preselected
short/long handoff. The fitted curves must not be called independently
validated or merged into R5.

Reproduce the opened-data analysis in a full checkout:

```sh
python Tools/stage5pa3/analyze_opened_creep.py \
  --output Tools/stage5pa3/opened_creep_analysis.json
```

In a sparse checkout, supply `--data-root` pointing at another complete
checkout. Inputs are hash-checked against the pinned commit. Two successive
generations in this session produced the identical JSON SHA-256
`F3711D12C6FC05F94CD6480A3F5FFFDA6FCC0FB56E10FC7E1F4FEBE78EBBAC47`.

## Next Decision Gate

First complete the ARM map and Host suite against the RAM candidate. In
algorithm work, compare an event-anchored load-history model and a unified
robust reference controller against frozen R5 and the rejected early
candidate. A load-dependent mechanism cannot be identified from one loaded
trace alone: its apparent early change also includes warm-up and common zero
drift. Use time-aligned raw/calibrated/filtered/conditioned channels, explicit
load/unload timestamps, and unloaded zero-return data for attribution.
The next data collection need only cover the candidate distinction: a
stable empty reference, one controlled 500 g load maintained for at least
30–60 minutes, a controlled unload and a zero-return observation, with a
second repeat for consistency. Capture the actual 10/40 Hz and filter
profile and mode transitions; do not reuse this opened data as a new holdout.

For any future algorithm, preserve strict DOSING offset freeze, true
load/unload span, inherited offset on an actual load step, safe reset on
ZERO/calibration, rate and filter switching, the 10-second correction bound,
and stable display behavior. Evaluate the opened 5/15/30-minute response and
the existing 12-hour trace first. Freeze the candidate before evaluating on
new untouched data. A new algorithm can enter SHADOW only after its software
and RAM gates pass; it should not automatically replace the existing R5 or
split into short/long stages just because the problem has two timescales.
