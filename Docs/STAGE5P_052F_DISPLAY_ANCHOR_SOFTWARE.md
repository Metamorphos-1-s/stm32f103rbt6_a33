# Stage 5P 0x052F display anchor following — software result

## Result and scope

Starting point: `stage5p-panel-display-smooth-v2`,
`3937faa46d5b2433fedc8b44f8b4d3328c80b8ee` (0x052E).

Result: **focused software gates PASS; complete strict Host suite INCOMPLETE;
hardware NOT RUN by user request**. Firmware identity is 0x052F; register
map remains 0x010F. No device was connected, programmed, reset or commanded.
This report does not supersede the 0x052E hardware observation.

## Problem and behavior

In 0x052E, small unstable input changes returned before the total
anchor-displacement check. Motion evidence depended on successive nonzero
derivatives. Intermittent additions produced one nonzero derivative followed
by many unchanged conversions; the evidence leaked away. Displayed weight
could remain at the old anchor despite an arbitrarily large cumulative
increase while official stable stayed false.

The new locked-state path:

1. Checks authoritative desired count against the displayed anchor first.
   A displacement strictly greater than 8e immediately enters TRACKING and
   follows the current value.
2. Continues filling the existing nine-point median ring after lock.
   Only a new sample sequence feeds the ring or advances the state.
3. Times the median's offset from the displayed anchor. Equal-valued new
   samples at the displaced weight retain evidence; return to the anchor
   or direction reversal resets evidence.
4. Follows the median after either 300 ms continuously at >=3e on the same
   side, or 1500 ms continuously at any nonzero same-side median offset.
   The slow clock survives movement between the small and fast bands.
5. Keeps the ordinary display anchor locked while following. A displayed
   lock is not the official stable flag. The latter is not changed.

The two existing clocks are reused: `candidate_start_ms` for the slow
same-side offset, and `operator_anchor_start_ms` for the fast band only
when this is **not** an operator zero anchor. The ring and state layout
are unchanged. For an actual operator zero anchor, its existing 3 s
unstable timeout is honored; a large displacement releases immediately.

The existing 100 ms numeric publication gate and its large-step bypass
remain unchanged. Sampling, filtering, R5 admission/compensation, formal
weight, checkweigh and alarm algorithms are not modified. The tests also
exercise candidate-enabled application and SAVE wiring.

These are initial software parameters. No claim is made that every physical
drop is visible when its mass is within the sensor's noise band.

## Software validation

| Check | Result |
| --- | --- |
| Display behavior scenarios, Host Debug | 68 scenarios, 0 failures |
| Display behavior scenarios, Host Release | 68 scenarios, 0 failures |
| Focused CTest Debug / Release | 7/7 each |
| ASan + UBSan display scenarios | PASS; leak detection disabled because executor /proc inspection is unavailable |
| Strict ARM Release GCC 13.3.1, -Os, -Wextra -Werror | PASS |
| Feature disabled, paired frozen/current ARM builds | BINs byte-identical |
| Complete strict Host build | INCOMPLETE; frozen and current Stage5A both stop on unused Join32 in modbus_register_model.c |

The 68 scenarios include assumed 0.02/0.05/0.10 g droplets at 1/2/5/10 s
intervals at both 10 and 40 Hz with official stable false, noisy droplets,
positive/negative persistent offsets, a cumulative ramp, immediate unload,
direction reversal, duplicate sequences, sequence/tick wrap, division
digits 1/2/5, g/kg/lb, source/view transfer, operator zero, forced reset,
overload, calibration and lock prohibition. Droplet masses are synthetic
test inputs, not a measurement of the user's dropper. After a final
plateau, the droplet cases reach the new weight rather than holding the
old anchor.

The new behavioral tests fail against the frozen 0x052E implementation.
The full Host blocker was independently reproduced in the frozen source;
no warning suppression was added for it.

## ARM resource evidence

| Metric | Candidate |
| --- | ---: |
| BIN bytes | 98,948 |
| Linker RAM, including reserved 1024 B stack | 19,432 / 20,480 B |
| Static collision margin including audited IRQ nesting | 624 B |
| Required static margin | 512 B |
| CONFIG_A / CONFIG_B linked use | 0 / 0 B |

BIN SHA-256:
`7D4D5EE893F5D8D3F6B1548AD91F0F0D10E13836C7877AECBEA517E5A1B0C032`.

The paired frozen build has the same RAM and static stack bound. This is
static callgraph analysis, not a new runtime stack watermark.

Build reproducibility boundary: the local frozen 0x052E reconstruction is
99,180 B, whereas the historical installed 0x052E artifact is 98,980 B.
The historical environment/flag/library difference is not isolated here.
The byte-identical feature-OFF comparison is between the **paired local
builds**, not a claim to reproduce the historical 0x052C BIN. Exact compiler
identity, flags and hashes are in `software/build_environment.json` and
`software/arm_comparison.json`.

## Existing-record replay

Both implementations were replayed from reset with identical input snapshots.
These are **decimated, non-atomic Host snapshot replays** at about 8 Hz:
the original capture read weight, sequence, stability and display in
separate Modbus requests. Thousands of intermediate conversions are absent.
No interpolation was performed; none of these results is a full 40 Hz replay
or a hardware panel result.

| Input capture | Rows | Frozen 0x052E software changes | New software changes |
| --- | ---: | ---: | ---: |
| 0x052C diagnostic record, including large transitions | 1517 | 51 | 67 |
| 0x052D loaded record | 965 | 53 | 37 |
| 0x052E loaded record | 979 | 31 | 26 |

The transition-rich record has more changes under the new follower; the
table is descriptive and is not an overall static-jitter PASS gate.
Replay counts differ from recorded device counts because initialization,
missing samples, non-atomic fields and sample cadence differ.

## Reproduction

Use CMake/Ninja and a native C11 compiler:

```sh
cmake -S Tests/host -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host --target stage5p_display_smooth_tests stage5p_unified_smooth_tests stage5p_save_continuity_smooth_tests stage5p_save_continuity_tests stage5p_unified_tests stage5nb_output_safety_tests stage5mr5e_d1d_display_tests
ctest --test-dir build/host -R 'stage5p_display_smooth_logic|stage5p_unified_smooth_tests|stage5p_save_continuity_smooth_tests|stage5p_save_continuity_logic|stage5p_unified_defaults_and_startup|stage5nb_output_safety_logic|stage5mr5e_d1d_display_logic' --output-on-failure
```

Repeat in a clean directory with `-DCMAKE_BUILD_TYPE=Release`. ARM build:

```sh
cmake --preset Stage5PA2DCalibration -B build/arm -DCMAKE_BUILD_TYPE=Release \
  '-DCMAKE_C_FLAGS=-Wall -Wextra -Werror -fstack-usage -fcallgraph-info=su' \
  -DA33_ENABLE_STAGE5PA13C_SHADOW=ON -DA33_ENABLE_STAGE5PA13E_ACTIVE=ON \
  -DA33_ENABLE_STAGE5P_UNIFIED=ON -DA33_ENABLE_STAGE5P_BATTERY_TRANSACTION=ON \
  -DA33_ENABLE_STAGE5P_PANEL_BATTERY_ZERO_LONGRATE=ON \
  -DA33_ENABLE_STAGE5P_R5_CHECKWEIGH_INTEGRATION=ON \
  -DA33_ENABLE_STAGE5P_SAVE_CONTINUITY=ON -DA33_ENABLE_STAGE5P_DISPLAY_SMOOTH=ON
cmake --build build/arm
arm-none-eabi-objcopy -O binary build/arm/stm32f103rbt6_a33.elf build/arm/firmware.bin
python Tools/stage5mr5c/analyze_ram_stack.py --elf build/arm/stm32f103rbt6_a33.elf --map build/arm/stm32f103rbt6_a33.map --callgraph-root build/arm --output build/ram
python Tools/stage5pa13d/nested_stack_gate.py --analysis build/ram --callgraph-root build/arm --output build/ram/nested_stack.json
```

Offline capture replay:

```sh
python Tools/stage5_unified/replay_display_anchor.py Results/stage5p_052c_display_diag/quiet_run_1/samples.csv Results/stage5p_052d_display_candidate/retry_2/hardware/display_capture_500g/samples.csv Results/stage5p_052e_display_candidate/hardware/display_capture_500g/samples.csv --output build/replay
```

Evidence: `Results/stage5p_052f_display_anchor/software/`. The software-only
candidate remains unqualified for panel appearance, physical load-edge
response, actual droplet response and long-duration drift.
