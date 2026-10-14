# Stage 5P panel and R5/checkweigh integration candidate

## Scope and identity

Base: `8537097e2be8ccce1aad71027e9ac76a97755695` (`0x0529`).
Development branch: `stage5p-panel-checkweigh-integration`. Candidate build
flag: `A33_ENABLE_STAGE5P_R5_CHECKWEIGH_INTEGRATION=1`. The public register
contract is unchanged, so the repaired, **unflashed** firmware is `0x052B`,
Map `0x010F`. The first `0x052A` hardware candidate was rejected and rolled
back. Neither is a certified Release.

Final strict ARM Release build: BIN 97,756 B, SHA-256
`5177DEA1EE55189D5D11466FCFCE54F05E529B525DC49BB7311310A16570A10E`;
ELF SHA-256
`387D71639A5279792AC9655086E40F1AA8145CE85C93121B56A01BEC6D86A7C2`.
Flash use 97,756/126,976 B; RAM 19,416/20,480 B; `_ebss=0x200047D8`.
The static-to-stack-top distance is 2,088 B; subtracting the established
1,536 B conservative nested-IRQ, exception and indirect-call budget leaves
552 B collision margin (gate 512 B).
The 134 generated `.su` files are compile-time evidence, not a measured
runtime high-water mark. The old source, built with this feature disabled,
reproduced the frozen `0x0529` BIN SHA-256
`85216447B5CB1701AFCADD947891111298FAA52E4F05DF89B97552A46FBD5786`.
V3 payload remains 281 B; no communication buffer, stack reservation or
persistent layout was reduced.

## Behavior and code

| Contract | Implementation | Gate |
| --- | --- | --- |
| R5 ACTIVE and formal STATIC/DYNAMIC coexist | Config validation, menu, PLC command, runtime admission and boot restore allow both requests; V3 codec unchanged | Host focused PASS; 0x052A hardware RAM-only entry observed, but candidate rolled back; joint SAVE/boot NOT RUN |
| STATIC formal net | Same-sample formal net from WeightEngine enters the existing stable/confirmation shadow path | Host PASS; hardware OK at empty load; threshold crossings NOT RUN |
| DYNAMIC fast net | Fast calibrated gross minus **applied** offset (zero for SHADOW), then tare; checked signed subtraction | Host PASS; hardware DYNAMIC OK at empty load; load threshold crossings NOT RUN |
| DOSING | R5 offset freezes; formal STATIC is PENDING/off, DYNAMIC continues | Host PASS; hardware nonzero offset STATIC PENDING and DYNAMIC OK PASS |
| Rejected input | New candidate counts raw and metrology rejections and routes any new count through guarded fault/reacquisition on the next main-loop pass | Real CS1237 queue -> bridge -> guarded output Host test PASS; complete App main-loop hardware path NOT RUN |
| Zero lamp | Official stable + authoritative net; enter <=2d, hold <=3d, ~1 s qualification, independent of NET/GROSS page | Host 10/40 Hz PASS; physical LED re-light USER OBSERVATION NOT RUN |
| Panel LEDs | Schematic/grid mapping: top NET, GROSS, tare, stable, zero, battery; bottom LOW, OK, HIGH, unused, g/kg, lb | Host mask PASS; physical mapping USER OBSERVATION NOT RUN |
| Rate | 50 ug/100 ms early, 25 ug/100 ms long; 40 Hz admits one of four samples; 0.5 g cap and DOSING freeze retained | Opened-data parity PASS; new independent efficacy NOT RUN |
| Manual ZERO default | 10 g new-device default; lamp thresholds independent | Host 9/11 g boundary PASS; existing V3 retains 60 g |

The two images supplied by the owner corroborate GRID1..GRID6 left-to-right,
with `SG_UP` wired to LED7/9/11/13/15/17 and `SG_DN` to
LED8/10/12/14/16/18. The separate formal alarm GPIO/buzzer path was not
remapped. No new display filter or heap allocation was added.

## Software evidence

LLVM-MinGW Clang 23.1.3 focused Host Debug and Release CTest each pass
`stage5p_unified_defaults_and_startup`, `stage5mr5e_d1d_display_logic`
and `stage5nb_output_safety_logic` (3/3). Tests exercise default ZERO limits,
40 Hz zero re-entry after tare while viewing GROSS, physical LED mask, old
request compatibility, dynamic fast net with tare, DOSING PENDING/freeze, and
same-sample gross/net equations. A new C test uses the actual CS1237 host
sample queue and MeasurementBridge to reject an invalid sample, then checks
the production `MeasurementBridge_RejectedSince` API and guarded output
revocation/reacquisition. The complete `App_UpdateAlarmOutputs` target loop
and its GPIO timing still require 0x052B hardware verification. The full
historical 47-test build is
**INCOMPLETE**: an unrelated existing `s_original_unit` unused-but-set
Clang `-Werror` warning stops Stage4A. ARM strict Release passes. ARM Debug
and runtime target stack high-water are **NOT RUN**.

Replayed opened A13B (88,765), A9 (107,491), and A13C-R (10,774) 10 Hz
samples with the 50/25 ug policy. Each C/Python comparison had zero
mismatches; maximum single-sample offset change was 50 ug, maximum observed
10 s change 4,950 ug, DOSING offset changes zero and obvious-step loss zero.
These are development/regression data, **not** independent efficacy evidence.
The new theoretical 10 s early-rate ceiling is 5,000 ug (0.005 g), not the
former 3,500 ug. A13B long-duration efficacy remains **INCONCLUSIVE**.

## Focused hardware evidence, 2026-10-14

Preflight: `0x0529/0x010F`, 40 Hz/filt1/strength3, calibration raw
zero/span `41868/485780` with 500 g span, R5 ACTIVE+STATIC and nonzero
volatile offset, checkweigh OFF, revision/saved 22/22, dirty/fault/overrun 0.
The run's complete 126,976 B application backup SHA-256 was
`AB6746226EA9D392687024D225634484513EF82A3DDBB84F805C9F9B8181D225`;
the 4,096 B V3 backup SHA-256 was
`A762D9FC4370704CFA3378AB2756A318BF69DE239426FF2FF6E9158C30EAF104`.
Slots A/B were valid with CRC/commit and sequence 21/22, B active. Both save
`zero_range_ug=60000000`, so this sample still has a **60 g** manual ZERO
range. No migration or SAVE was performed.

An earlier incomplete `0x052A` prototype was tested and rolled back to the
exact `0x0529` backup. Its raw failure evidence remains under
`Results/stage5p_integrated/hardware/preflash_20261014`, `flash_052a`,
`flash_052a_final`, `r5_checkweigh_coexistence_20261014.json`, and
`rollback_0529_20261014`. These are not final candidate PASS evidence.

The completed `0x052A` BIN, 97,692 B SHA-256
`560414743E616ABC08EB3DF9E147E17E8CB139C460B9968CE2A5066AF3A6EAC5`,
was flashed after an initial HotPlug erase rejection (which
left the old application bytes intact). A validated Normal/SWrst erase of
application pages `[0 123]` then program/Verify succeeded. Full application
readback matched the candidate BIN prefix with an all-FF tail. Configuration
SHA immediately after flashing and after rollback matched the backup;
pages 124-127 were never written. The first failed erase, its COM5 timeout,
the unchanged application readback and subsequent recovery are preserved.

At 40 Hz, RAM-only alarm limits `[-1, +1] g` and formal STATIC request
were accepted while R5 remained ACTIVE with `+11612 ug` offset. STATIC gave
formal OK. ACTIVE+DOSING froze this nonzero offset and made STATIC formal
PENDING/off. Switching formal checkweigh to DYNAMIC then gave valid OK while
DOSING continued. A host response-token collision caused an initial false
"OK" for that DYNAMIC request; raw frames show `last_command=36` instead of
34. It is retained as a host evidence failure, and the corrected script
requires fresh token, matching command ID and mode readback before scoring.
No SAVE was sent; RAM revision advanced 22->26, saved stayed 22.

The independent, read-only five-minute trace captured 750 rows, zero read
errors, zero host gaps >2 s, no sequence regression, fault or overrun.
Every atomic public response satisfied `formal_gross = uncompensated_gross -
applied_offset` and `net = gross - tare` with zero ug error. The observed
uncompensated range was 0.358179..0.482078 g: **no 500 g edge occurred**.
Host polling at 0.4 s necessarily skips physical 40 Hz sequence values;
those host omissions are not evidence of MCU lost samples. A controlled MCU
reset discarded unsaved test changes; final probe recovered revision/saved
22/22, dirty/fault/overrun 0 and checkweigh OFF. It also cleared the
volatile offset as expected; R5 resumed ACTIVE+STATIC from zero. The final
configuration SHA still matched the preflash backup. A subsequent source
audit found that a rejected ADC sample could leave the old formal class
active until the 250 ms stale timeout. This violates the new immediate
invalid-input contract. **The 0x052A result is FAIL_ROLLED_BACK**, regardless
of its successful empty-load subtests. The full 126,976 B `0x0529` backup
was reflashed and Verify/readback gave SHA-256
`AB6746226EA9D392687024D225634484513EF82A3DDBB84F805C9F9B8181D225`;
the final 4 KB config remained
`A762D9FC4370704CFA3378AB2756A318BF69DE239426FF2FF6E9158C30EAF104`.
Final COM5 readback: `0x0529/0x010F`, 40 Hz/filt1/strength3,
R5 ACTIVE+STATIC/offset 0 after reset, checkweigh OFF, revision/saved 22/22,
dirty/fault/overrun 0.

The repair has separate identity `0x052B` and only software evidence. It
has **not** been flashed or observed on the target MCU.

## Qualification boundary

| Item | Result |
| --- | --- |
| Focused Host Debug/Release and opened-data C/Python parity for 0x052B | PASS |
| Strict ARM Release Flash/RAM/static stack margin | PASS |
| Rejected-input bridge-to-guarded-output C test | PASS |
| Complete 0x052B main-loop/GPIO safety integration on target | NOT RUN |
| 0x052A 40 Hz empty-load coexistence and 750 same-response weight equations | Observed PASS subtests, but 0x052A overall FAIL_ROLLED_BACK |
| 0x052B focused real-device validation | NOT RUN |
| 500 g load/unload, LOW/HIGH transitions and step preservation | NOT RUN |
| Physical zero/tare/unit/checkweigh LED observations | NOT RUN |
| Joint R5+checkweigh request SAVE and real power restore | NOT RUN |
| Target runtime stack/time for new build | NOT RUN |
| Full historical 47 Host CTest | INCOMPLETE |
| A13B long-duration efficacy; formal metrology, cross-sensor qualification | INCONCLUSIVE; NOT RUN; NOT RUN |

The final device is the exact, verified `0x0529` baseline, not the new
candidate. `0x052B` still needs focused target rejected-input safety,
physical transition and dual-request recovery checks before any engineering
delivery claim. Do not enable persisted formal checkweigh in combination
with R5 on this unit.
