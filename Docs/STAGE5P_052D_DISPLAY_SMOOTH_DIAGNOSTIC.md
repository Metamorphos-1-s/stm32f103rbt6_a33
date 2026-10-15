# Stage 5P display smooth diagnostic and rollback

## Decision

**0x052D hardware candidate: FAIL_ROLLED_BACK.** The display-only
candidate passed its focused Host test and strict ARM build, but the
candidate was not accepted on hardware: after programming and Verify,
COM5 timed out on two read attempts. No panel or load qualification was
credited to 0x052D. The exact 0x052C application and V3 configuration were
restored and compared byte-for-byte. Historical 0x052A/0x052B failures,
A13B long-duration efficacy `INCONCLUSIVE`, and full Host-suite
`INCOMPLETE` remain unchanged.

## What the existing evidence shows

The precise 0x052C read-only capture contains 1,517 valid records with no
read errors (CSV SHA-256
`36AF7EAE50B8B1B6E767B5855AE0B2DE4F3F8DA220B12FF3556CF47CA2FB525E`).
During the empty/static portion the display was locked and official stable
was true; the capture did not contain the user's reported idle flicker.
At the load edge, raw ADC and filtered ADC moved together, official stable
became false, the display conditioner changed from LOCKED to TRACKING with
release reason `DISPLAY_RELEASE_UNSTABLE`, and display counts followed the
40 Hz-derived weight path. The capture had 277 display-count changes and
22 unique counts. The largest observed count jump was the real load step,
not a TM1628 scan artifact. The later stable 500 g region still showed
small count changes while the conditioner was tracking/reacquiring.

The TM1628 driver marks its RAM dirty only when encoded segment bytes
change (`TM1628_SetGridSegments`), and `DeviceManager_Process20ms()` flushes
the whole RAM only while dirty. It does not repeatedly write identical
segments. The internal chip scan is therefore not the cause of a rapidly
changing numeric value. Public registers expose the raw/filtered/weight and
display-condition snapshots, but not physical segment current or a display
RAM readback.

## Candidate design and software result

0x052D adds the independent build flag
`A33_ENABLE_STAGE5P_DISPLAY_SMOOTH` without changing the public Map. The
display-only behavior is:

* In LOCKED, a short unstable indication must persist for three samples
  before the display releases its anchor. A confirmed large display step
  (more than eight display divisions) releases/follows immediately.
* On NET/GROSS pages, ordinary numeric segment publication is limited to
  one update per 100 ms; the first value, page changes and large steps are
  immediate. ADC, metrology, R5 and formal checkweigh processing remain at
  40 Hz. Indicators, messages, menus and alarms are not throttled.
* The existing “only changed segment bytes dirty the TM1628” behavior is
  retained.

Focused Host Debug and Release `stage5p_display_smooth_logic` both passed.
The test covers the three-sample transient hold and immediate large-step
release. Existing 0x052C focused tests (SAVE continuity, unified startup,
5NB output safety and D1-D display) were rebuilt and passed 4/4.

Strict ARM Release 0x052D built with GCC 13.3.1, `-Os`, strict warnings and
the complete 0x052C feature set:

* BIN: 98,788 B, SHA-256
  `D9A5278BBAB8FE2F68F13F1C7A175A686C81F1A5101819ED65AACB816BE8EEBF`
* ELF SHA-256
  `CD3E847D98138C9D934F4BB0B81344E168F1CC3A1EE1E80BD0A83026A9D4D2A9`
* RAM: 19,432 B / 20,480 B
* Conservative collision margin: 536 B (minimum 512 B)
* Flash: 98,788 B / 126,976 B
* Map end: `_ebss = 0x200047E8`, `_estack = 0x20005000`

Rebuilding with the flag OFF reproduced the ordinary 0x052C BIN SHA
`E87C5FCFDCC4FE8800725741AFE39BA8378FDE917A7C45A4453BA4CBE8826DD6`,
so the old image remains reproducible and isolated.

## Hardware attempt and recovery

Fresh preflight before programming read 0x052C/0x010F, 40 Hz/filt1/3,
R5 OFF+SHADOW/offset 0, checkweigh STATIC, valid calibration, slot A
sequence 23, revision/saved 23/23 and zero safety counters. The 126,976 B
application backup SHA was
`F7FEA7553D6B3DB0D2F87B3BF92D43B17F742D6A730092D336ACB63C187AA24B`.
The 4,096 B V3 backup SHA was
`05CB5BAFB2E722702F8B132F1FAC6C273BA273A7A1A3ABD50F6FDCA72CECFBC1`.
Both slots were valid before the attempt.

The exact 0x052D BIN was programmed and Verify passed. The configuration
read immediately after programming still matched the preflash SHA, but
COM5 timed out twice. No user display result was recorded for 0x052D.
The application was then erased/programmed back to the exact 0x052C BIN
and Verify passed. An explicit SWD reset was required before COM5 returned;
this reset is part of rollback evidence, not a product qualification
operation.

The first rollback read showed slot B sequence 22, indicating that the
page-erase command used for the candidate attempt was not an acceptable
application-only erase boundary and affected slot A. This is retained as
a hard failure. The complete preflash 4 KB V3 image was restored through
the rollback procedure and verified byte-for-byte; final config SHA again
matched `05CB5BAF...CFBC1`.

Final readback at 2026-10-15 17:13:24 UTC:
0x052C/0x010F, 40 Hz/filt1/strength3, R5 OFF+SHADOW, offset 0,
checkweigh STATIC, slot A sequence 23, revision/saved 23/23,
dirty/fault/overrun 0. The 500 g load remained on the platform; no
additional physical action was requested after the rollback.

## Qualification status

| Item | Result |
| --- | --- |
| Source-chain audit and baseline capture | PASS in observed scope |
| Display-conditioner transient/large-step Host test | PASS |
| Display numeric publication candidate software build | PASS |
| 0x052D target startup/Modbus | FAIL, no response after Verify |
| 0x052D display visual validation | NOT RUN |
| 0x052D load visual-rate validation | NOT RUN |
| 0x052C exact rollback and config preservation | PASS |
| Physical TM1628 segment-current/readback proof | NOT RUN |
| Long-duration drift, cross-sensor and formal metrology | unchanged / NOT RUN |

No software candidate is left on the device. The candidate requires a new
review of its startup failure and a corrected, verified application-only
erase procedure before another hardware attempt. The current branch does
not claim the display issue is solved on the device.

## Reproduction

```text
cmake -S Tests/host -B <debug> -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=<llvm-mingw-clang>
cmake --build <debug> --target stage5p_display_smooth_tests
ctest --test-dir <debug> -R stage5p_display_smooth_logic --output-on-failure

cmake -S . -B <arm> -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake \
  -DA33_ENABLE_STAGE5P_DISPLAY_SMOOTH=ON <frozen-0x052C-flags>
cmake --build <arm>
arm-none-eabi-objcopy -O binary <arm>/stm32f103rbt6_a33.elf <arm>/firmware.bin
python -B Tools/stage5_unified/analyze_display_diag_052c.py \
  Results/stage5p_052d_display_candidate/baseline_display_capture/samples.csv \
  --output Results/stage5p_052d_display_candidate/baseline_display_analysis.json
```
