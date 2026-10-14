# Stage 5P 0x052C SAVE continuity closure

## Decision and boundary

**Focused SAVE continuity: PASS. Post-power persistent request readback: PASS.
Startup transient/PENDING timing: INCOMPLETE.** The prior 0x052B
**FAIL_ROLLED_BACK** and 0x052A **FAIL_ROLLED_BACK** remain unchanged. This
branch starts at `e9de573cbcd73e3c10e2be8b7623b47ee3bb852f` and uses
the separate `stage5p-panel-checkweigh-052c-save-continuity` branch. The
new candidate was not evaluated for long-duration correction efficacy.

The owner confirmed a real physical power cycle after the SAVE. The
power-cycle recorder timed out at 09:07:36 UTC, before that action, so it
does **not** prove the reset edge or the first valid post-reset sample.
A new 10:34:20 UTC read-only COM5 snapshot and subsequent V3 readback
prove the post-cycle steady state, not the startup transient. No physical
500 g matrix was repeated. The target `valid=false` injection remains
**NOT RUN** because there is no safe public injection path.

## Root cause and repair

In 0x052B, a successful plain SAVE completed storage maintenance through
`PersistenceManager_Process()` and unconditionally called
`MetrologyManager_RestartAfterStorage()`. That rebuilt the engine, stopped
R5 with the profile exit path and did not re-admit the saved request.
The 478 historical OFF+SHADOW readings are preserved in the parent report;
their precise target-side exit reason was not directly observed.

0x052C distinguishes a pure persistence commit from a candidate SAVE that
actually changes metrology, calibration or stability settings. A pure SAVE
preserves the live offset/reference and guarded request. If the storage
pause breaks sequence/time continuity, the existing offset is retained
while the reference and observation windows reacquire. A genuine
measurement rebuild re-admits the final accepted request and requires
fresh qualification. Formal checkweigh output is gated during maintenance
and reacquisition. Mode/config commands are BUSY while the asynchronous
storage transaction owns the device; completion checks the revision and
candidate config before marking it saved. There is no implicit second
SAVE. The mathematical compensation parameters and V3 281 B payload
layout are unchanged.

## Software gate

| Gate | Result |
| --- | --- |
| 0x052B with new flag OFF | Exact historical BIN SHA reproduced: `5177DEA1EE55189D5D11466FCFCE54F05E529B525DC49BB7311310A16570A10E` |
| Focused Host Debug/Release | PASS, 4/4 each, including actual product C storage and metrology modules |
| New C cases | Pure SAVE at 40/10 Hz with nonzero offset; DOSING/DYNAMIC; alarm reacquisition; changed-filter candidate rebuild; failed SAVE/BUSY; OFF/SHADOW |
| Full strict Host suite | INCOMPLETE: unrelated Stage4A `UI/menu_controller.c` `s_original_unit` unused-but-set warning is fatal under Clang `-Werror` |
| Strict ARM Release | PASS; 98,644/126,976 B Flash, 19,416/20,480 B RAM |
| Conservative stack collision margin | 552 B, above 512 B minimum; static map/call-chain result, not a runtime high-water measurement |
| A13B/A9/A13C-R historical C/Python comparison | Existing 0-mismatch result retained; not a new holdout or long-duration qualification |

The candidate is Firmware `0x052C`, Map `0x010F` (public register contract
unchanged), BIN SHA-256
`E87C5FCFDCC4FE8800725741AFE39BA8378FDE917A7C45A4453BA4CBE8826DD6`,
ELF SHA-256
`BBFE6183A5F17BCEF6D0E846B1FF3CFEDB55BD1346CAC866A5B4671786F0C17E`.
No dynamic allocation or communication-buffer shrink was introduced.

## Target SAVE sequence

Fresh preflight read 0x0529/0x010F, 40 Hz/filt1/strength3,
ACTIVE+STATIC, checkweigh OFF, revision/saved 22/22 and
dirty/fault/overrun 0. The 126,976 B application backup SHA was
`AB6746226EA9D392687024D225634484513EF82A3DDBB84F805C9F9B8181D225`;
the 4,096 B V3 backup SHA was
`A762D9FC4370704CFA3378AB2756A318BF69DE239426FF2FF6E9158C30EAF104`.
Both slots were committed and CRC valid (A seq21, B seq22 active). The
saved manual ZERO range remained +/-60 g. Only application pages 0-123
were erased; the exact candidate was programmed and verified. Full
application readback matched the BIN prefix and all-FF tail. V3 SHA after
flashing was identical to its preflash SHA.

A RAM-only checkweigh STATIC request and 100/400 g limits moved revision
22 to 24 without SAVE. Before the one planned SAVE, R5 was ACTIVE+STATIC
with +9,148 ug offset, checkweigh STATIC, revision/saved 24/22, dirty 1,
slot B seq22. The mailbox returned ACCEPTED at 08:51:54 UTC. At
08:51:54.063922 UTC, the first observed completed row showed
revision/saved 24/24, dirty 0, slot A seq23, unchanged +9,148 ug offset,
R5 ACTIVE+STATIC and checkweigh STATIC. A transient checkweigh PENDING
was observed and later qualified to LOW.

The archived 08:49:35-08:54:54 UTC trace has 3,191 valid host polls,
zero read errors, one SAVE command and one completion. In the 1,806
post-SAVE polls spanning 180 seconds, there were zero R5 pair mismatches,
zero checkweigh-mode mismatches, zero offset changes, zero dirty rows and
zero fault/overrun rows. Across all 3,191 atomic authority responses,
`formal_gross = uncompensated_gross - applied_offset` and
`formal_net = formal_gross - tare` had zero error. There were 3,187 host
polling sequence gaps because six-block Modbus polling does not capture
every 40 Hz sample. These are **not** evidence of MCU sample loss, and
the run does not independently qualify full physical sequence continuity.

After the owner's physical power-cycle confirmation, the fresh read-only
COM5 snapshot at 10:34:20 UTC showed 0x052C/0x010F,
40 Hz/filt1/strength3, valid calibration 41868/485780 with 500 g span,
R5 ACTIVE+STATIC/TRACKING, checkweigh STATIC, revision/saved 23/23,
dirty/fault/overrun 0, slot A seq23. The offset was +3,190 ug at this
later snapshot; it is a volatile post-boot reading, **not** proof that the
pre-power +9,148 ug offset was stored in Flash. The power-cycle recorder
had expired before the action, so immediate offset=0 and initial
checkweigh PENDING were not directly captured. Restart had caused the
runtime revision to begin at the saved revision 23; pre-power RAM
revision 24 and V3 sequence 23 are different counters.

Post-cycle read-only V3 SHA was
`05CB5BAFB2E722702F8B132F1FAC6C273BA273A7A1A3ABD50F6FDCA72CECFBC1`.
Slot A seq23 and slot B seq22 both have valid CRC/commit, with A active
and matching Modbus. The saved payload differs only in the planned
checkweigh request/limits and sequence metadata; calibration, battery,
communications, R5 request, filter, unit and +/-60 g ZERO range remain.
The byte-level saved-payload audit passed: only offsets 208-210, 216-218,
223-224, 235 and 280 changed, and the prior slot B is byte-identical.
The final 10:44:31 UTC read-only check after SWD access still showed
0x052C, ACTIVE+STATIC, checkweigh STATIC, revision/saved 23/23,
dirty/fault/overrun 0. Its naturally varying offset was -8,021 ug;
no cleanup reset or mode change was performed.

## Scope and reproduction

| Item | Status |
| --- | --- |
| Pure SAVE with nonzero offset and joint requests, 180 s runtime continuity | PASS |
| One SAVE, double-slot integrity, no surprise revision/SAVE | PASS |
| Physical power-cycle request readback and steady runtime | PASS: user-confirmed power cycle plus post-cycle device/config readback |
| First post-reset offset=0, pending-to-qualified timing | INCOMPLETE: recorder expired before user action |
| 10 Hz/filt1 target sequence | NOT RUN; focused C test PASS |
| 0x052C physical 500 g load matrix, same-sample DYNAMIC fast-net | NOT RUN / INCOMPLETE; 0x052B history is not 0x052C proof |
| Target valid=false injection | NOT RUN |
| Full strict Host suite | INCOMPLETE |
| A13B long-duration efficacy | INCONCLUSIVE, unchanged |
| Cross-sensor and formal metrology qualification | NOT RUN |

Focused Host: configure `Tests/host` in separate Debug and Release Ninja
directories with LLVM-MinGW Clang, then build the four focused targets
and run `ctest -R 'stage5p_save_continuity_logic|stage5p_unified_defaults_and_startup|stage5nb_output_safety_logic|stage5mr5e_d1d_display_logic'`.
The ARM Release build enables the 0x052B integration flags plus
`A33_ENABLE_STAGE5P_SAVE_CONTINUITY=ON` and strict warnings with stack
usage. `arm-none-eabi-objcopy -O binary` reproduces the archived BIN.
Recompute the target trace with:

```text
python -B Tools/stage5_unified/analyze_save_continuity_052c.py Results/stage5p_052c/hardware/save_continuity_1
python -B Tools/stage5b_hw/parse_config_slots.py Results/stage5p_052c/hardware/config_after_power.bin --modbus-active-slot A --modbus-sequence 23
python -B Tools/stage5_unified/manifest_integrated.py --root Results/stage5p_052c
```

The raw RTU frames, CSV, command events, SWD backups/readbacks, build
logs and SHA manifest are under `Results/stage5p_052c/`. None of the
historical 0x052A/0x052B failure data was removed or reclassified.
