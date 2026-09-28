# Stage 5P-A13D-R target resource validation

Conclusion: **A13D-R TARGET RESOURCE PASS** (2026-09-28).
This is a prospective diagnostic resource qualification, not efficacy or metrology approval.
A13D remains TARGET RESOURCE GATE FAIL: host call 1590 was not recorded.
Its original files and frozen zero-gap contract are unchanged. A13B remains
EFFICACY INCONCLUSIVE; owner-deferred high-stimulus data remains deferred.

## Baseline and prospective evidence

Independent worktree: `D:/Documents/stm32f103rbt6_a33_stage5pa13dr`.
Branch: `stage5pa13dr-cumulative-resource-validation`.
Baseline: `5dd51a472aabd4a14a2b69701dbcc70baf687b4d`.
Contract/software commit, before device access: `5ab919ff475bcbd58412581f49b89ad5289f11fa`.
The final evidence commit is the commit containing this report (`git rev-parse HEAD`);
the handoff gives its full hash and verified remote status. No merge, tag or PR.
Original worktrees, .pyc, temporary captures and prior failure evidence were not changed.

The frozen plan/rules are `Docs/STAGE5PA13DR_FROZEN_EVIDENCE_CONTRACT.md`
and `Results/stage5pa13dr/frozen_rules.json`. MCU cumulative peaks, peak sequence,
per-path counts, complete call sequence and lowest touched stack address survive
mode commands and missing host polls. Ten path buckets include a forbidden INVALID
bucket. A synchronous 122-register read at 0x0380 includes both generation values;
main-loop writers cannot interleave with that Modbus read, IRQs do not write stats.
Odd/mixed generations, incoherent peak metadata, counter overflows or restarts invalidate.
Host snapshots are not assembled from independent cumulative requests. Earlier four
general blocks are explicitly labelled as earlier, not the same device sample.

Exact target counts must conserve produced/consumed/engine and timed samples; FIFO
cannot accumulate. Host heartbeat <=5 s plus MCU uptime delta tolerance 250 ms after
uptime >=5 s constrains resets. New rules allow an honestly reported host polling gap
only when target cumulative and conservation/reset evidence stays intact. They do not
recover missing individual call times or retroactively relax A13D's rule.

All Feed call sites, including OFF/readiness/invalid paths, are wrapped; pure A13C
mathematics is unchanged. Statistics are 136 B leased from the existing 281 B payload
scratch (284 B aligned union). No communication buffer or stack/256 B indirect allowance
was reduced. After App_Init, ConfigStore Init/Load/SAVE/factory requests refuse before
mutation while scratch is leased; normal non-diagnostic builds retain ordinary behavior.
An initial Host lease regression found SAVE verification could overwrite scratch. Its
failed `software/host_debug_ctest.log` is preserved; rejection was fixed before hardware,
and the retry and final 45/45 tests passed. No hardware failure was discarded.

## Software/static evidence (CONSERVATIVE INFERENCE)

Host Debug and Release: 45/45 each; contract Python: 10/10. Tests cover actual C peak
accumulation/overflow and storage lease; simulated serial commands hide the true sample-2
peak yet final sample-3/4 reads retain it. Injected unseen timeout, stack511 B, reset,
target sample loss, missed timed invocation and mixed peak/generation are rejected.
R5, D1-D display, checkweigh, V3 dual-slot/power-cut and persistence regressions pass.
Strict ARM Debug/Release GCC 13.3.1 builds pass (`-Wall -Wextra -Werror`, stack/callgraph
output). Actual compile commands are archived: Debug -Og/-g3, Release -Os/-g0.

| Build | Linker RAM including 1024 B stack reservation | Complete conservative stack | Collision margin |
|---|---:|---:|---:|
| Diagnostic Debug | 19,416 B | 1,552 B | 536 B |
| Diagnostic Release (flashed) | 19,360 B | 1,448 B | 696 B |

Complete bounds add main chain, largest priority-5 ISR, hardware exception frames,
possible SysTick preemption/nesting, alignment and unchanged 256 B indirect allowance.
The legacy one-IRQ Debug margin 584 B is NOT the complete bound: fresh nested analysis
gives 536 B. See final gate JSON, map, .su/.ci and final analysis directories. Static
analysis bounds possible paths not all exercised in this short read-only hardware run.

Ordinary rebuilt 0x051D / Map0105: 93,588 B, SHA-256
`DDF57FB131E5EF675D94B08A5A8752EB0FB588BA5230DEA6F34C57D6B2F62DDD`.
Its identity is unchanged. A13C math object SHA is identical in ordinary/diagnostic
Release builds: `EFCAA070EC912A2746C89E7C27DAEB39829E9FF953DBF3468038E22B63AC38EC`.
Independent diagnostic 0x051F / Map0108: 95,316 B, SHA-256
`0EDAB26C37EC90D7166E963D74F4905F00704CBFFA148D82BBD8A794A9E1B7FD`.
Old 0x051E/0107 last-call semantics are not reused.

## Fresh device, flash and timeline

COM5 preflight 2026-09-28 09:33:25.757185 UTC: actual 0x051D/0105, 10 Hz,
filt1/strength3, valid raw_zero41868/raw_span485780/span mass500000000 ug,
revision/saved19/19, dirty/fault/overrun0, R5 OFF+SHADOW/offset0, checkweigh OFF.
Full current-run backup: application126976 B, V3 config4096 B; both slots valid,
active A sequence19, B sequence18. Configuration was not changed or saved.

Applications were erased with CubeProgrammer's validated interval `-e '[0 123]'`,
not `-e 0 123`. Pages124-127 were not erased or written. Diagnostic Verify plus upload
proved correct prefix, all-FF tail and unchanged configuration. SWD checks/reset were
outside the timed acquisition; NO SWD halt/read occurred during continuous capture.

| Event | UTC 2026-09-28 |
|---|---|
| Capture begins (event) | 09:36:09.473575 |
| Empty STATIC command succeeds | 09:38:52.084395 |
| Load instruction recorded | 09:42:39.339476 |
| User load confirmation recorded | 09:44:05.595239 |
| DOSING command succeeds | 09:46:15.892932 |
| Return STATIC succeeds | 09:50:33.997204 |
| Unload instruction recorded | 09:51:50.634496 |
| User unload confirmation recorded | 09:52:26.216509 |
| Capture ends | 09:54:02.272767 |
| Exact ordinary restoration terminal | 09:54:19.247084 |

Human confirmations are not precise physical edge times. Continuous sample traces show
load/unload and the corresponding target STEP/settling/count progression. Three gates
include load, DOSING-exit reinitialization and unload; they are not three physical loads.
We did not repeat A13B efficacy, nonzero freeze qualification or a long-duration test.

## Target hardware results (DIRECT MEASUREMENT of diagnostic Release)

Normal continuous five-block Modbus load: 0/32,0x20/28,0x1C0/10,0x280/40,0x380/122.
12,518 raw host polls; sample snapshots 09:36:09.556617–09:54:02.263765 UTC;
monotonic span1072.703 s. Host sequence-gap events0, read errors0; duplicate polls remain.
Target timed sequences1–10702, total10702, produced=consumed=engine=10702, invalid0,
final FIFO0, fault/overrun0 throughout. All nine required buckets covered, INVALID0.
No reset, cumulative-stat invalidation, SAVE or revision change; dirty remained0.

Actual HCLK72,000,000 Hz. Cycle maxima and their sequence identities are MCU retained:

| Path | Calls | Maximum cycles | Maximum us | Peak sample |
|---|---:|---:|---:|---:|
| OFF | 1616 | 431 | 5.986 | 1267 |
| HOLDOFF | 518 | 143444 | 1992.278 | 4221 |
| REFERENCE_FILL | 1200 | 604317 | 8393.292 | 2066 |
| OBSERVATION_FILL | 796 | 77296 | 1073.556 | 4646 |
| TRACKING median | 1898 | 430086 | 5973.417 | 3444 |
| FAST_TRACKING | 1986 | 496459 | 6895.264 | 9594 |
| STEP_PENDING | 29 | 106616 | 1480.778 | 4174 |
| STEP_SETTLING | 82 | 128798 | 1788.861 | 9626 |
| DOSING | 2577 | 77161 | 1071.681 | 7836 |

Feed max8.393292 ms <=10 ms; complete measured App_Run+scan max867086 cycles
=12.042861 ms <=25 ms; loop-start interval max867137 cycles=12.043569 ms <=25 ms.
For 100 ms physical periods, frozen limits reserve90 ms outside Feed and75 ms outside
App_Run respectively (not additive; Feed is inside App_Run). No backlog or target loss.

Runtime CONTROL0 confirms thread MSP, no PSP. Paint starts after App_Init, uses a naked
leaf with IRQ exclusion and excludes current MSP-64/live frames, static data and Flash.
Startup/App_Init BEFORE paint is not measured. Monotonic stack pattern scan retained
lowest touched0x20004D0C, static end0x200047A0, top0x20005000: observed untouched RAM
**1388 B**, top-to-deepest756 B. Both runtime1388 and complete static Release696/Debug536
exceed512 B. This is a workload high-water mark, not proof of every future ISR/path.

The empty DWT bracket max20 cycles (0.278 us) was not subtracted. Feed timings include
read/wrap/post-call state classification and any IRQ during the timed interval; cumulative
bookkeeping after stopping Feed timer is not included in Feed, but is included in the
complete loop. Loop maximum includes scan and wrapper body but excludes final metadata
store/return after its final cycle read; the start-to-start interval includes that residual.
SWD restoration pauses occur outside capture. Source/disassembly and equal math objects
support comparison, but wrappers, memory layout, compiler context and IRQ/communication
load differ. These are NOT exact timing measurements of the unmodified ordinary BIN;
its exact target timing remains NOT RUN. No Host time substitutes for MCU time.

## Exact restoration and terminal

The recorder closed COM5 before SWD. Restored this run's full126976 B backup with Verify,
then uploaded full application/config and compared EVERY byte. Ordinary prefix matched,
remaining application tail contained0 non-FF bytes. Configuration SHA before/after:
`856BD8F5C14760561FC4BFEC4274FE0C5480C19BC4422046617C2450F8439733`.
Full application before/after SHA:
`6279F17AB9E987BF8156F5F9F805B8DB41CDBDA2951603DC1B55CE559D4B34B5`.
Terminal0x051D/0105, same10Hz/filt1/strength3/calibration/V3/activeA19; revision/saved19/19,
dirty/fault/overrun/SAVE0, R5 OFF+SHADOW, candidate OFF/offset0, checkweighOFF.
The diagnostic was not left installed, candidate ACTIVE was never enabled.

## Evidence and reproduction

All paths below are repository-relative, rooted at the independent worktree above.
`Results/stage5pa13dr/hardware/measurement/polls.csv` SHA:
`35137CBF889DEC863F6D90C905A742FACED9F88AE267A2D29CBB85F5B1F78369`.
`hardware/measurement/snapshots.jsonl` SHA:
`DD3037BA8434D0FF0F5F0439AFBD800BB3C93F3A7CABFF13E88F13C85D3BEB44`.
Events/requests preserve instructions, confirmation times and mode command results.
`hardware/resource_review.json` replays the frozen Checker on every original snapshot;
`hardware/measurement/restore/terminal.json` and binary/log files prove exact recovery.
Manifest records every formal artifact SHA/size, including preflight/backups, flash,
failed/software logs, final maps, stack/callgraphs, compile commands, ELFs and math objects.
Disposable Host projects/objects and Ninja caches remain untracked; originals not deleted.

Read-only offline reproduction (no device opened):

```powershell
python -B -m unittest discover -s Tools/stage5pa13dr -p test_contract.py -v
python -B Tools/stage5pa13dr/review.py --input Results/stage5pa13dr/hardware/measurement --output D:/Downloads/a13dr_review.json
python -B Tools/stage5pa13dr/evidence.py
ctest --test-dir Results/stage5pa13dr/software/host -C Debug --output-on-failure
ctest --test-dir Results/stage5pa13dr/software/host -C Release --output-on-failure
```

Build commands/options are in CMakeCache/compile_commands and logs. Fresh reproduction
uses Stage5PA2DCalibration inherited preset plus A33_ENABLE_STAGE5PA13C_SHADOW=ON,
A33_ENABLE_STAGE5PA13DR_RESOURCES=ON, old A13D switchOFF and strict C flags above;
toolchain `cmake/gcc-arm-none-eabi.cmake`. Ordinary comparison sets DR switchOFF.
Fresh Host: cmake -S Tests/host -B <new-host-directory>, build Debug/Release, run CTest.
Maps/.su/.ci feed `Tools/stage5mr5c/analyze_ram_stack.py`, then
`Tools/stage5pa13d/nested_stack_gate.py`; final gate JSON distinguishes static/runtime.

PASS: prospective software contract, strict builds/regressions, complete static margins,
all resource paths, MCU cumulative timings, runtime watermark, conservation and exact recovery.
NOT RUN: unchanged ordinary BIN exact dynamic timing, ACTIVE, true continuous dosing
qualification, 40Hz/other filters/cross-sensor/absolute-zero/metrology qualification.
INCONCLUSIVE: A13B efficacy, unchanged. ASan/UBSan not run in this MSVC Host environment.
Next permitted recommendation: a SEPARATE stage may design default-off, manually enabled,
immediately escapable controlled ACTIVE engineering use; none was implemented here.
