# Stage 5P-A13E-R: diagnostic expiry fix, software PASS, target resources INCOMPLETE

Date: September 28, 2026. Baseline remote
`stage5pa13e-guarded-active-engineering` at
`72a2283ec3073d61834a5c2d13a9a5d1ad1c19da`.
Branch `stage5pa13er-guarded-active-closure`, isolated worktree
`D:/Documents/stm32f103rbt6_a33_stage5pa13er`. Source and prospective
target plan were frozen **before** COM5/SWD at
`fbec3aed80ce8cf9db452e1ba6914bf7c9a8d848`.
The final commit/remote verification is delivered in the user-facing handoff.

**Result: A13 GUARDED ACTIVE ENGINEERING BETA NOT READY.**
Software/static admission PASS; prospective new target resource qualification
**INCOMPLETE** after a safe `INVALID_STATE` admission rejection. The recorder
retains `FAIL` for its aborted run, with every raw frame intact. These are
distinct: the observed Feed/loop/stack limits passed for the *covered* paths,
but required path coverage was only 31/511 and a rejected command stopped
the run. No 0x0520 ACTIVE firmware was burned; no one may claim the Beta READY
conclusion. Historical A13E SOFTWARE GATE FAIL, A13D FAIL and A13B efficacy
INCONCLUSIVE remain historically unchanged; A13D-R resource PASS remains
restricted to its older tested diagnostic build.

## Diagnosis and minimal correction

Unaltered historical failure evidence is under
`Results/stage5pa13e/software/parity_a13cr/` (first strict error at seq85212).
`LeaveDosing` calculated recent-step eligibility from the *internal* event,
then invalidated it but failed to clear published
`snapshot.obvious_step_sequence`, reporting old load sequence80441. The
new A13E-only fix clears expired **public metadata after** the recent-event
decision, plus public metadata on reset, fault/LIMITED, initial DOSING and
time/sequence gap. It does not erase an internal event before its decision or
change offset/reference/gates/rate/thresholds/Python. A nonzero-offset C test
checks DOSING exit, fresh unload re-detection, fault, reset and OFF.

Crucially the fix is guarded by `A33_ENABLE_STAGE5PA13E_ACTIVE`, so ordinary
0x051D was rebuilt byte-identical: 93,588 B SHA-256
`DDF57FB131E5EF675D94B08A5A8752EB0FB588BA5230DEA6F34C57D6B2F62DDD`.
Old math object SHA EFCAA070EC912A2746C89E7C27DAEB39829E9FF953DBF3468038E22B63AC38EC.
A13E/diagnostic math objects are identical to each other SHA
EF86E64F47B87F9900BD36108C1132447C1FB74CF2F23B30D83F073A21784F5F.
This difference is deliberately limited to public timestamp expiry.
Standard Release remains byte-identical to baseline at 80,888 B, SHA
C181AE5B916A69C6B120E712D52B575AA62E46B9F9F0C12786FFD3C8C2100CD9.
Old R5 mathematical and V3 format source paths were not modified.

## Final software gates (PASS)

Final A13E-enabled C/Python replay uses same state across real STATIC→DOSING→STATIC:
A13C-R **10,774**, A13B **88,765**, A9 **107,491** samples: each **0 mismatch**
including corrected, offset, states, reasons, gate/rebuild counts, obvious,
robust, quiet, reference-lock and first-correction sequences. Six synthetic
traces (step/cycles/pulse/motion/gap/dosing) all PASS. These older datasets
are implementation regressions, *not* independent efficacy holdout.
Host Debug and Release: **46/46 each**, /W4 /WX; focused real-module test covers
volatile menu confirmation/cancel/timeout, PLC generation/ABA and BLE refusal,
checkweigh STATIC/DYNAMIC refusal, boot restoration of stored old ACTIVE request,
net/gross/display-authority consistency, nonzero SHADOW admission, DOSING freeze,
OFF jump metadata, ZERO/TARE, fault/time gap and profile invalidation.
Strict ARM Debug/Release and resource Debug/Release builds pass
`-Wall -Wextra -Werror -fstack-usage -fcallgraph-info=su` with unchanged comm
buffers, 1024 B declared stack and 256 B indirect allowance. The complete
nested SysTick/priority5 IRQ gates from the last source change are:

| Build | Linker RAM | Conservative complete stack | Static collision |
|---|---:|---:|---:|
| ACTIVE Debug | 19,432 B | 1,552 B | **520 B** |
| ACTIVE Release | 19,384 B | 1,448 B | **672 B** |
| Resource Debug | 19,432 B | 1,552 B | **520 B** |
| Resource Release | 19,384 B | 1,448 B | **672 B** |

520 B clears the 512 B bound by only **8 B**; this is not a comfortable product
margin. Portable exact maps, ELFs and all `.su/.ci` plus CMakeCache and compile
commands are under `Results/stage5pa13er/software/builds/`. The offline analyzer
reran against those **archived** paths, not a temporary builder.

Both engineering BINs are **NOT approved for customer/daily firmware**:

| Image | Identity | Size | SHA-256 | Flashed? |
|---|---|---:|---|---|
| `active_0x0520.bin` | 0x0520/0x0109 | 94,620 B | 0CED588DF5C0C1AE1A3635A376002EE8F9479DCC579F5F96FF9607894C8DC4F6 | **NO** |
| `resource_0x0521.bin` | 0x0521/0x010A | 96,324 B | AFB6144A384B7CC611C05B727FCA5E855DE51E4E0F473F7EA63A13EB294C5BED | **YES, restored** |

Software and resource contract details:
`Docs/STAGE5PA13E_FROZEN_ACTIVE_CONTRACT.md` (historic draft) and
`Docs/STAGE5PA13ER_FROZEN_RESOURCE_PLAN.md` plus frozen JSON.

## Fresh hardware preflight, aborted resource run

COM5 preflight **2026-09-28 11:59:43 UTC** showed real 0x051D/0x0105,
10 Hz/filt1/strength3, calibration raw zero41868/raw span485780/500 g,
R5 OFF+SHADOW/offset0, checkweigh OFF, valid V3 format3, revision/saved19/19,
dirty/fault/overrun/SAVE0. Full **126,976 B** app and **4,096 B** config were
backed up *this run*, hashes respectively
`6279F17AB9E987BF8156F5F9F805B8DB41CDBDA2951603DC1B55CE559D4B34B5`
and `856BD8F5C14760561FC4BFEC4274FE0C5480C19BC4422046617C2450F8439733`.
Both V3 slots valid: A seq19 active; B seq18. Only app pages **[0 123]** were
erased, CubeProgrammer Verify, full prefix and FF-tail readback; configuration
unchanged. New diagnostic full app SHA
`B12F5933D39BFAB1367463AE099E9CF34804FA1436C07D64F716CF5C38B4620A`.
0521 boot was OFF+SHADOW, offset0, fault/overrun/dirty0.

One COM5 owner continuously collected 1,800 polls from **12:01:13.128409 to
12:03:46.772560 UTC** (153.640 s monotonic) while five blocks were read.
Empty SHADOW+STATIC command returned OK at 12:02:17.245977 UTC; OFF, HOLDOFF,
REFERENCE_FILL, OBSERVATION_FILL and TRACKING genuinely ran. At approximately
12:03:34 UTC offset was 0 and the command was queued later. By the last three
recorded frames immediately before ACTIVE request, offset was **−205, −210,
−215 µg**. At **12:03:46.810566 UTC** the explicit pair command (application1,
STATIC2, expected generation3) returned **INVALID_STATE (4)**. The source
deliberately rejects SHADOW→ACTIVE when offset is nonzero to prevent a sudden
formal-weight change. This is a strongly supported explanation, **inference**:
there is no firmware rejection-reason register to prove it was the unique
cause. No adjustment or unsafe forced activation was attempted.

The recorder preserved the original failure and immediately stopped/restored.
One host poll skip **969→971** is explicitly recorded, not filled in; MCU
produced=consumed=engine=timed=**1,863**, FIFO0 and no MCU loss. Fault,
overrun, read errors, dirty and SAVE0, revision/saved19/19. Observed HCLK72MHz,
largest Feed **610,620 cycles = 8.480833 ms**, App_Run+scan max **12.711069 ms**,
loop interval **12.711792 ms**, runtime untouched RAM **1,364 B**. These
measurements apply ONLY to the covered OFF and empty STATIC states; mask31
of required511, with **zero** FAST_TRACKING, STEP_PENDING, STEP_SETTLING and
DOSING calls. A13E ACTIVE itself did not enter; no 500 g placement/removal was
requested this stage. No target resource PASS may be inferred.

Raw `Results/stage5pa13er/hardware/resource_measurement/snapshots.jsonl` SHA
`F9975FB109AC26A1BA77158317AB2632C2ABEFFAA29CDD5FEB826C3779373931`,
`polls.csv` SHA
`EDC013634C50CE3CBBBE3E6604F25112C802586917E3C96A6C7CA93949F73DDD`.
Preserve `summary.json` status **FAIL** (the recorder aborted) alongside
`resource_review.json` qualification **INCOMPLETE**. Neither is changed to PASS.

## Recovery and explicit NOT RUN

Immediately after capture, only pages[0 123] were restored from this run's
application backup and verified; config never written. Full readback application
and config match the pre-flash backup **byte for byte**, hashes unchanged as above,
tail all FF. Post-restore COM5 terminal at **12:04:03.631574 UTC**:
**0x051D/0x0105, OFF+SHADOW, offset0, checkweigh OFF, revision/saved19/19,
dirty/fault/overrun/SAVE0**, original calibration and filter unchanged.

NOT RUN: full nine-path resource gate; uninstrumented 0520 target timing/high
water, firmware0520 burn/Verify, ACTIVE/STATIC 500 g loading, ACTIVE/DOSING
nonzero offset, real checkweigh conflict, panel/PLC authority continuity,
physical fault injection and physical power-cycle default-OFF. No 12 h test,
40 Hz/filter matrix or new efficacy holdout. A13B efficacy remains INCONCLUSIVE.

Minimal next resource *plan*, not executed: a **new** controlled measurement
must reach ACTIVE with exactly zero offset by an explicit OFF reset before
enable, then cover missing STEP/FAST/DOSING paths and meet the original strict
thresholds. Keep this aborted run unchanged as a separate negative record;
do not treat its pre-rejection maximum as all-path qualification. No 0520
installation may occur until that separate complete resource gate passes.

## Offline reproduction (no device access)

```powershell
python -B Tools/stage5pa13e/replay_implementation.py --runner <rebuild_host_runner> --output <new-output>
python -B -m unittest discover -s Tools/stage5pa13er -p test_resource_contract.py
python -B Tools/stage5pa13er/review_failed.py --input Results/stage5pa13er/hardware/resource_measurement --output <new-review.json>
python -B Tools/stage5pa13er/evidence.py
```

Host builds use `cmake -S Tests/host -B <out>` and CTest Debug/Release.
ARM builds use `Stage5PA2DCalibration` preset plus both A13C/A13E switches ON,
and A13DR resources ON only in diagnostic build; Debug -Og, Release -Os,
strict C flags above. Run `Tools/stage5mr5c/analyze_ram_stack.py` and
`Tools/stage5pa13d/nested_stack_gate.py` against archived build maps/ELFs
and `.su/.ci`. Complete build logs, raw UART polls, SWD Verify/readback logs,
software/error diagnostics and SHA manifest are in `Results/stage5pa13er/`.
No merge, tag, PR, Stage 5M-F or product ACTIVE activation.
