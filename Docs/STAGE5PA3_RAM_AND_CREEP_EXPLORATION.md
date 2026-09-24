# Stage 5P-A3: RAM Reuse And Opened Creep Evidence

## Status

```
0x051C DEVICE BASELINE PRESERVED;
RAM REUSE TARGET SOFTWARE GATES PASS;
EARLY CREEP MODEL SELECTION INCOMPLETE;
NO DEVICE ACCESS, FIRMWARE FLASH OR ACTIVE ALGORITHM CHANGE.
```

The starting commit is `d6312dd9898ba8f01a43cd83a76fa41546d88275`,
the flashed 0x051C engineering build. This branch changes two RAM workspaces
and one focused storage regression. R5 parameters, its C and Python models,
display behavior, Checkweigh, Modbus mapping, persistent format, and the
device's 0x051C image are unchanged. This analysis never touched the device.
The R5 C source Git blob remains
`40102a398c39843e8ed10f86c5bca8c908bddb3d` and the V3 schema blob
remains `060c3c429eafa15398a4966ad7bd4a6bce6e81df` at both baseline and
candidate HEAD.

| Gate | Conclusion |
|---|---|
| RAM target map and conservative stack | PASS |
| Flash dual-slot, power-cut, transaction and full Host regression | PASS |
| ARM Debug / Release / strict warnings | PASS |
| Focused ASan | PASS |
| UBSan on this Windows bench | NOT RUN |
| Event-anchored early-load scoring | NOT RUN: no verified load event |
| Product R5 algorithm change or hardware qualification | NOT RUN |
| Early creep model selection | MODEL SELECTION INCOMPLETE |

## RAM: Measured Baseline And Candidate

The exact 0x051C Debug ARM map ends static RAM at `0x20004808`. RAM ends at
`0x20005000`, so there are 2,040 bytes between static data and stack top.
Subtracting the previously audited 1,504-byte conservative stack demand
leaves 536 bytes. The 1,024-byte linker minimum stack is already contained
within the 19,464-byte linked RAM figure. Lowering that number would not
make the conservative stack bound smaller.

| Object in ARM Debug map | Before | After |
|---|---:|---:|
| A and B temporary payloads | 281 B + 281 B | one 281 B scratch; valid A copied into existing active payload before B is read |
| Factory configuration and candidate target | 344 B + 344 B | one 344 B union, protected by the existing single-operation gate |
| ConfigStore object RAM | 1,303 B | 1,022 B |
| PersistenceManager object RAM | 1,126 B | 782 B |
| **Linked Debug RAM** | **19,464 B** | **18,840 B** |
| **Conservative stack / collision margin** | **1,504 / 536 B** | **1,504 / 1,160 B** |

The original 625-byte object-size saving was only a projection. Actual linked
RAM falls by **624 B** because of layout/alignment. The fresh ARM call graph
still has a 1,144-byte main chain and 72-byte IRQ chain; with the existing
32-byte exception frame and 256-byte indirect-call allowance, the conservative
bound remains 1,504 B. Actual static end is `0x20004598`, RAM top is
`0x20005000`, and the measured collision margin is **1,160 B**, above the
512-byte floor. No UART2/UART3 DMA ring, Modbus server, BLE transport capacity,
R5 history, or Flash data format was reduced.

The local `/W4 /WX` Host CTest covers the 104-cut power-loss loop, both slots
valid with either A or B newer, corruption of either single slot, candidate
SAVE success/failure rollback, factory reset, and cross-request rejection
while the shared union is occupied. Product-defined host tests use R5 and
Checkweigh restore stubs and do not qualify target I/O. The actual ARM map,
not host ABI object sizes, establishes the 281-byte scratch and 344-byte
transaction union sizes above.

The patch author's earlier Linux host runs reported focused ASan/UBSan success
with LeakSanitizer disabled. Those are historical patch notes, not results
reproduced on this Windows bench. The local sanitizer status is stated below;
focused sanitizer tests do not replace the full project or target gates.

The full Host Debug/Release CTest is **41/41 PASS** in both configurations.
ARM Debug, Release and strict `-Wextra -Werror` builds pass; Debug and strict
RAM are 18,840 B, Release RAM is 18,784 B. Focused MSVC ASan storage and
persistence tests are **3/3 PASS** after adding the installed runtime directory
to PATH. The first ASan attempt failed to launch for a missing DLL on PATH;
this was a setup failure, not a test PASS. UBSan is **NOT RUN** because this
Windows host lacks a UBSan-capable native compiler/runtime. No device access
or flash was performed. The unflashed A3 Debug BIN is 108,776 B with SHA-256
`5A1F26784DD9729954EC0115137FD2871F670684F5726C7A65E63368AEB60F2E`.
The Release BIN is 92,420 B with SHA-256
`A1F3D5C7F604A9CB79F0CA540DC8C664898135C803E2529FD3047C56E5D4A410`.
The strict Debug BIN is byte-identical to the Debug BIN. These images are
**unflashed** and share the 0x051C identity only for software comparison;
the installed 0x051C firmware remains the earlier A2D build.

Reproduce the baseline projection and compare the exact candidate map after
building with the original ARM toolchain:

```sh
python Tools/stage5pa3/ram_map_audit.py \
  --output Tools/stage5pa3/ram_baseline_projection.json
python Tools/stage5pa3/ram_map_audit.py \
  --candidate-map path/to/candidate_debug.map \
  --stack-report path/to/stack_watermark.json \
  --output path/to/candidate_ram_result.json
```

`Results/stage5pa3/ram_map_measured.json` records the actual symbol sizes,
static end and collision PASS. The tool requires a fresh stack report before
marking a candidate map's collision gate as PASS.

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
checkout. Inputs are hash-checked against the pinned commit. The checked-in
and freshly reproduced JSON both have SHA-256
`AB8BD6ED8D951EAC7EF7B296BA213A38F98F0FCD18B352A250E94D15A9C90673`.
The earlier printed `F371...` SHA was stale and is not the file's actual hash.

## Conditional Three-Model Comparison

`Tools/stage5pa3/compare_opened_models.py` reproduces a second, explicitly
conditional comparison. It verifies the Git blob IDs of all six CSVs and the
opened 12-hour CSV/summary at the frozen commit. The frozen R5 column is the
same one-second Python replay described above. The event-anchored load-history
column is **NOT SCORABLE** for every loaded trace: no verified placement
timestamp exists, and the 12-hour ACTIVE recorder starts after loading and
reference establishment. The known 12-hour unload event can test DOSING
freeze, not locate the earlier load. Recorder start is never substituted for
the physical load event.

The unified robust-reference column is a fixed *illustrative* controller:
60-second rolling median, 10 mg MAD stability gate, 0.05 mg/s correction cap,
0.5 g absolute offset cap, 20 g step detector and 15-second post-step holdoff.
It starts STATIC at the first recorder row and initializes its reference from
the 15-45 second window. These choices were not fitted to the six runs; the
result is still opened-data and conditional, not an independent validation.
Each number below is signed milligrams from that run's 15-45 second median,
**not absolute weight error**. Entries are 5 / 10 / 15 / 30 minutes; `NA`
means the following 60-second checkpoint window is absent.

| Opened run | Raw | Frozen R5 | Unified conditional |
|---|---|---|---|
| 500 g 30 min | +46.186 / +51.255 / +57.451 / NA | +46.186 / +51.255 / +56.983 / NA | +32.636 / +22.848 / +13.474 / NA |
| 500 g R4 A | +19.713 / +8.449 / +14.081 / +5.632 | +19.713 / +8.449 / +14.081 / +5.632 | +6.488 / -2.712 / +3.108 / +0.638 |
| 500 g R4 B | +1.126 / +2.252 / -3.380 / +7.885 | +1.126 / +2.252 / -3.380 / +7.885 | -1.860 / -4.200 / -2.304 / +3.459 |
| unloaded zero 15 min | -33.513 / -63.083 / NA / NA | -33.513 / -63.083 / NA / NA | -21.413 / -36.120 / NA / NA |
| cold empty R4 | -56.887 / -79.417 / -82.796 / -104.762 | -56.887 / -79.417 / -82.796 / -76.768 | -43.764 / -50.780 / -40.334 / -15.912 |
| cold empty 5L | +284.436 / +475.938 / +613.931 / +883.723 | +284.436 / +475.938 / +613.182 / +838.260 | +283.036 / +466.138 / +591.306 / +818.110 |

The three loaded five-minute changes differ by over 40 mg, while the two
cold-start empty changes point in opposite directions. Even a relatively
flat hypothetical loaded output can be produced by tracking a common thermal
zero movement. The same unified controller *amplifies* the tiny R4 B 10-minute
deviation from +2.252 to -4.200 mg. Neither visual improvement nor a single
loaded trace identifies sensor load creep.

The recorded 12-hour ACTIVE run changed +174.604 mg uncompensated and
-39.216 mg with frozen R5. A conditional unified replay beginning at ACTIVE
start gives -4.850 mg and ends with 179.220 mg offset. Its maximum ten-second
offset change is 500 ug, equal to the fixed 50 ug/s cap. This replay has a different
initialization history from the actual R5 device; it is **not** a head-to-head
qualification. Actual DOSING unload held the R5 offset exactly, retained the
physical load/unload step, had zero automatic rebuilds, and returned to an
unloaded last-five-minute corrected median of -121.616 mg versus its earlier
empty baseline -99.130 mg. Applying the hypothetical 179.220 mg offset to the
recorded unloaded 93.498 mg would yield -85.722 mg, a +13.408 mg residual
from that baseline, but this is an offline counterfactual only.

Five synthetic tests for the illustrative controller pass: DOSING offset
freeze and exact step preservation without rebuilding on unload; inherited
offset and full physical load step in STATIC; ZERO or calibration reset to zero;
profile change with 10/40 Hz and filt0-3 bounds; and 50 ug/s rate plus 0.5 g
cap. They do **not** establish physical 10/40 Hz
compatibility, display holding quality or product safety. The six opened
records have no synchronized ZERO/calibration, filter-switch or rate-switch
events to score. The authoritative raw gross, filtered mass and conditioned
panel channels are kept separate; for example the 30-minute loaded run's
300-360 s versus 15-45 s change is +48.439 mg in filtered mass and +81.107
mg in conditioned display mass, while raw ADC moves -40 counts.

Reproduce with:

```sh
python Tools/stage5pa3/test_compare_opened_models.py
python Tools/stage5pa3/compare_opened_models.py \
  --output Results/stage5pa3/opened_model_comparison.json
```

The full numerical output and exact pinned blob IDs are in that JSON.
**MODEL SELECTION INCOMPLETE** remains the only defensible algorithm decision.

## Next Decision Gate

The RAM candidate now passes its target software gates, but is not flashed.
The minimal new data needed to distinguish load-dependent creep from common
zero drift is: a warmed, timestamped 10-minute empty reference; a marked
500 g placement held 30-60 minutes; a marked unload with at least 20 minutes
of zero-return recording; then a second repeat after recovery. Add a matched
empty-only warm-up run under the same power/temperature conditions, and log
temperature at least at the start and end. Capture authoritative raw gross,
raw ADC, filtered mass, corrected candidate mass, panel count, offset,
rate/filter, R5 mode/state and precise physical event UTC. This targeted
dataset, not the six opened traces, can test whether a load-history state
adds information beyond a unified robust reference or common zero drift.
Freeze model constants before examining the new run; do not reuse the opened
data as a new holdout.

For any future algorithm, preserve strict DOSING offset freeze, true
load/unload span, inherited offset on an actual load step, safe reset on
ZERO/calibration, rate and filter switching, the 10-second correction bound,
and stable display behavior. Evaluate the opened 5/15/30-minute response and
the existing 12-hour trace first. Freeze the candidate before evaluating on
new untouched data. A new algorithm can enter SHADOW only after its software
and RAM gates pass; it should not automatically replace the existing R5 or
split into short/long stages just because the problem has two timescales.
