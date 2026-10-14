# Stage 5P 0x052B focused hardware closure

## Decision

**FAIL_ROLLED_BACK.** This run started from pushed HEAD
`4d4c580d078d277efe13e7a683aae9eb83644141` on the independent
`stage5p-panel-checkweigh-052b-hardware` branch. The historical `0x052A`
result remains **FAIL_ROLLED_BACK**. `0x052B` passed focused 40 Hz load,
classification and DOSING subtests, but its explicit SAVE left the persisted
R5 request ACTIVE+STATIC while the runtime became OFF+SHADOW, without a
reset or power loss. The owner was **not** asked to power-cycle after this
hard failure. Both application and V3 region were restored from this run's
preflash backups and independently read back byte-for-byte.

No new 12-hour drift, cross-sensor or formal metrology qualification was
attempted. A13B long-duration efficacy remains **INCONCLUSIVE**.

## Software and image gate

The exact `0x052B / Map 0x010F` BIN was rebuilt from the baseline source:
97,756 B, SHA-256
`5177DEA1EE55189D5D11466FCFCE54F05E529B525DC49BB7311310A16570A10E`.
ELF SHA-256
`387D71639A5279792AC9655086E40F1AA8145CE85C93121B56A01BEC6D86A7C2`.
Strict ARM Release: Flash 97,756/126,976 B; RAM 19,416/20,480 B;
conservative stack collision margin 552 B (minimum 512 B). Frozen A13B,
A9 and A13C-R C/Python replay results from the parent report remain 0
mismatch; these are historical development data, not new efficacy holdout.

Focused Host Debug and Release each passed 3/3 tests after extending the
actual CS1237 queue -> MeasurementBridge rejection -> GuardedCheckweigh
reacquisition test to both STATIC and DYNAMIC. Full strict 47-test Host
build remains **INCOMPLETE**: Stage4A compiles `UI/menu_controller.c` with
Clang `-Werror` and stops at the existing unused-but-set global
`s_original_unit`. The separate attempt with compatibility warning flags
did not pass either; it is not counted as a full-suite PASS.

The exact `0x052B` application contains no safe public target control that
turns a real bridge sample into `valid=false`. Stage5NA3 changes only the
checkweigh shadow input and cannot prove the new bridge rejection path.
No disconnection, short, overload or direct output-variable manipulation
was used. **Target invalid-input qualification: NOT RUN.** The Host C test
is evidence for both formal modes, not a substitute for a target result.

## Protection and flash

Fresh COM5 preflight read `0x0529 / 0x010F`, 40 Hz/filt1/strength3,
calibration raw zero/span 41868/485780 with 500 g span,
R5 ACTIVE+STATIC with volatile offset -20,118 ug, checkweigh OFF,
rev/saved 22/22 and dirty/fault/overrun 0. Flash reset was expected to
clear that offset; it was not interpreted as compensation efficacy.
Both V3 slots had valid CRC and commit, B seq22 active, A seq21. The saved
manual ZERO range remained 60,000,000 ug (plus/minus 60 g), not the new
device default of plus/minus 10 g.

Full preflash application, 126,976 B SHA-256:
`AB6746226EA9D392687024D225634484513EF82A3DDBB84F805C9F9B8181D225`.
Full V3 region, 4,096 B SHA-256:
`A762D9FC4370704CFA3378AB2756A318BF69DE239426FF2FF6E9158C30EAF104`.
Only application pages 0-123 were erased. Program/Verify of the exact BIN
passed; full application upload matched its prefix with an all-FF tail.
The V3 SHA immediately after flashing was unchanged. Initial boot again
read `0x052B`, 40 Hz/filt1/strength3, R5 ACTIVE+STATIC, checkweigh OFF,
rev/saved 22/22, dirty/fault/overrun 0.

## Load and mode evidence

All times below are UTC; edge brackets come from device-weight observations,
not from the time of the owner's message. Manual messages confirmed each
operation after it occurred. Host polling was about 0.4 s; skipped device
sample sequence numbers are expected host omissions, not proved MCU loss.

| Trace | Rows / read errors | Data-inferred load bracket | Data-inferred unload bracket | 15-45 s loaded span (uncompensated / formal) | Formal result |
| --- | ---: | --- | --- | --- | --- |
| STATIC | 1,168 / 0 | 06:33:57.486-57.887 | 06:35:46.689-47.078 | 500.015769 / 500.015769 g | LOW -> pending -> HIGH -> LOW |
| DYNAMIC | 1,119 / 0 | 06:41:16.388-16.783 | 06:46:46.783-47.183 | 500.025343 / 500.025343 g | LOW -> HIGH -> LOW |
| ACTIVE+DOSING, DYNAMIC | 467 / 0 | 06:51:53.451-53.852 | 06:53:07.858-08.253 | 500.019148 / 500.019148 g | LOW -> HIGH -> LOW |

Every one of the 2,754 atomic authoritative-weight rows satisfied
`formal_gross = uncompensated_gross - applied_offset` and
`formal_net = formal_gross - tare` exactly. No sampled fault, overrun,
sequence regression or host polling gap above 2 s occurred. This checks
the public authoritative block; it does not claim complete physical
40 Hz sequence coverage.

The DYNAMIC fast-weight diagnostics are in a different FC03 block from
the applied-offset/authoritative fields. During the 40 Hz DYNAMIC trace,
**zero** shadow/authority response pairs had the same device sequence.
Thus LOW/HIGH behavior and formal authoritative equations are observed,
but an exact same-sample target assertion for `fast net = fast gross -
applied offset - tare` is **INCOMPLETE**. That mathematical path is tested
in C, not independently established by cross-sample hardware reads.

After a first non-atomic command-entry comparison falsely flagged a
50 ug difference, the original error record was retained. A separate
continuous ACTIVE+DOSING trace then showed all 467 offset readings exactly
51,490 ug through loading and unloading. DYNAMIC classification continued.
Changing formal checkweigh to STATIC while DOSING produced PENDING with
formal state and alarm outputs off. Returning R5 to STATIC retained the
offset, entered rebuild, and the stable empty scale reacquired LOW.

The owner observed on the panel, in one session: NET page top-left LED1
and stable LED4, bottom LOW LED1 and g/kg LED5, display 0.33; GROSS page
top LED2 and LED4, same bottom LEDs and 0.33; battery page top LED4/LED6,
same bottom LEDs and `41P`; returning NET showed 0.32. These are
**USER-OBSERVED**, not independently measured lamp currents. Zero near
zero, tare, OK/HIGH physical LED locations, lb, and every other individual
LED were **NOT OBSERVED**; `*` was not reported lit.

The saved alternate profile 1 was 40 Hz/filt0/strength0, not the required
10 Hz/filt1/strength3. The host script refused to issue a profile command
before any device change. **10 Hz/filt1 target hardware: NOT RUN.** No
filter/profile matrix was inferred from the 40 Hz data.

## SAVE failure and rollback

RAM-only alarm limits were 100/400 g with 0.010 g hysteresis, alarm
function on and buzzer enables off; checkweigh STATIC and R5 ACTIVE+STATIC.
The guarded host SAVE checked exact firmware, active profile, modes,
calibration, safety counters, battery configuration, dirty revision 28/22,
and pre-save request count 0. It sent **one** `COMMAND_REQUEST_CONFIG_SAVE`
with a fresh response token. At 07:07:24.466 UTC, the command was accepted;
by 07:07:24.601 UTC the saved revision was 28, dirty 0, active A seq23,
save count 1, and formal checkweigh still STATIC. At the same post-SAVE
readback, R5 was **OFF+SHADOW, offset 0**, despite the saved ACTIVE+STATIC
request. No power cycle had occurred.

Both after-SAVE V3 slots had valid CRC/commit. The old B seq22 slot was
unchanged. Compared with old B, new A seq23 payload changed only at byte
offsets 208-210, 216-218, 223-224, 235 and 280, all within planned
alarm fields or the combined request byte. The request byte was 0x06 ->
0x16 (R5 ACTIVE+STATIC retained; checkweigh OFF -> STATIC). All other 281 B
payload bytes, including calibration, communication, battery, unit,
filters and ZERO range, were unchanged. The correctly saved but unsafe
to-deliver V3 SHA was
`05CB5BAFB2E722702F8B132F1FAC6C273BA273A7A1A3ABD50F6FDCA72CECFBC1`.
The subsequent read-only recorder captured 478/478 rows with R5
OFF+SHADOW and checkweigh STATIC; no read error, fault or overrun. This
is a persistent runtime/request mismatch, not a transient poll.

Source attribution: `PersistenceManager_RequestSave` enters storage
maintenance; after commit it calls `MetrologyManager_RestartAfterStorage`,
which invokes `MetrologyManager_RebuildEngine`. That path calls
`A13E_Stop(A13C_REASON_PROFILE)` and the normal SAVE completion does not
re-arm the saved R5 request. This code path explains the measured outcome;
the exact target-side exit reason was **not** directly read during the
failed SAVE, so the reason code itself remains an inference.

Per the hard-stop rule, no real power-cycle, target invalid-input test,
additional menu/LED test or further SAVE was attempted. The **fresh**
preflash seq22 4 KB config was restored to pages 124-127 and the full
0x0529 application backup to pages 0-123; each operation passed Verify.
Independent upload/readback hashes exactly matched the two preflash hashes
above. Final COM5: `0x0529 / Map 0x010F`, 40 Hz/filt1/strength3,
R5 ACTIVE+STATIC with post-reset volatile offset 0, checkweigh OFF,
slot B seq22, rev/saved 22/22, dirty/fault/overrun 0. No unrelated worktree
modifications were reverted or committed.

## Remaining gate

| Item | Status |
| --- | --- |
| Exact 0x052B identity, ARM size/RAM/stack, focused Host Debug/Release | PASS |
| 0x052B 40 Hz STATIC/DYNAMIC load classification, formal weight equation | PASS in observed scope |
| Nonzero-offset DOSING freeze and DYNAMIC operation | PASS in observed scope |
| Same-sample DYNAMIC fast-net target arithmetic | INCOMPLETE |
| Real target `valid=false` -> immediate formal output revoke | NOT RUN: no safe injection interface |
| 10 Hz/filt1, remaining physical LEDs, near-zero lamp and tare view | NOT RUN / NOT OBSERVED |
| Joint-request explicit SAVE runtime consistency | **FAIL** |
| Joint-request real power restore | NOT RUN: stopped before power cycle |
| Exact application and config rollback | PASS |
| Full 47 Host CTest | INCOMPLETE: existing Stage4A Clang warning |
| A13B long-duration efficacy | INCONCLUSIVE (unchanged) |

Do not reflash the failed `0x052B` image or reuse these traces as an
independent qualification of a fix. A separate new-identity candidate must
restore R5 safely after storage maintenance, with a C test for SAVE runtime
continuity and new focused target verification before joint persistence can
be accepted. This report does not claim formal metrology qualification.

## Reproduction

The ARM build used GCC 13.3.1, Release, `-Wextra -Werror -fstack-usage`,
and the product feature flags in `CMakeLists.txt` with
`A33_ENABLE_STAGE5P_UNIFIED`, `A33_ENABLE_STAGE5P_BATTERY_TRANSACTION`,
`A33_ENABLE_STAGE5P_PANEL_BATTERY_ZERO_LONGRATE`, and
`A33_ENABLE_STAGE5P_R5_CHECKWEIGH_INTEGRATION` all ON. It also enables the
existing A13E ACTIVE, A13C, D1-D, 5NB, A2D and PA1 Modbus optimization
dependencies. `arm-none-eabi-objcopy -O binary` on the final ELF reproduces
the BIN hash above. Host tests use LLVM-MinGW Clang 23.1.3:

```text
cmake -S Tests/host -B <debug-build> -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=<clang.exe>
cmake --build <debug-build> --target stage5p_unified_tests stage5nb_output_safety_tests stage5mr5e_d1d_display_tests
ctest --test-dir <debug-build> -R 'stage5p_unified_defaults_and_startup|stage5nb_output_safety_logic|stage5mr5e_d1d_display_logic' --output-on-failure
```

Repeat with `-DCMAKE_BUILD_TYPE=Release`. The raw logs are in
`Results/stage5p_052b_hardware/host_{debug,release}_ctest.log` and the
full-suite build failure is in `host_full_strict_failure.log`.

All analysis scripts are read-only with respect to the raw CSV/V3 inputs:

```text
python -B Tools/stage5_unified/analyze_focus_052b.py Results/stage5p_052b_hardware/static_load_1/samples.csv --output <new-json>
python -B Tools/stage5_unified/audit_joint_save_052b.py Results/stage5p_052b_hardware/preflash/config_v3.bin Results/stage5p_052b_hardware/joint_save_1/config_after_save.bin --output <new-json>
python -B Tools/stage5_unified/manifest_integrated.py --root Results/stage5p_052b_hardware
```

The 82-artifact SHA list is `Results/stage5p_052b_hardware/manifest.json`.
Raw `frames.txt`, `samples.csv`, `events.json`, failed-host records,
preflash and rollback images remain intact. The user-provided operation
messages are preserved verbatim in `manual_observations.json`; their
message times are not substituted for physical edges.
