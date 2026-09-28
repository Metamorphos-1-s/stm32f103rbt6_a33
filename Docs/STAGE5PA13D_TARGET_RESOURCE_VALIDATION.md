# Stage 5P-A13D — target timing and runtime stack attempt

## Final decision

**A13D TARGET RESOURCE GATE FAIL**. The frozen zero-missing-sequence gate
failed at the OFF → STATIC command handoff: complete timed-call observations
advanced **1589 → 1591**, so timing/state evidence for **1590 is missing**.
The recorder stopped immediately and restored the current-run exact 0x051D
application with Verify and byte-comparison. No 500 g loading, further STATIC
sampling, DOSING test, threshold relaxation, or retry was performed after this
failure. The dataset/window is retained intact, not removed to manufacture PASS.

Evidence supports a **host acquisition gap**, not an ADC/sample-queue loss or
MCU reset. The final row's earlier general diagnostic block observed 1590,
but the final atomic timing block already held 1591. Produced, consumed and
engine sequence all reached 1591, FIFO remained zero, invalid/read-error/
overrun/fault remained zero, and MCU timestamps advanced normally by 200 ms
across the two recorded timed-call endpoints. This distinction does not waive
the pre-frozen rule: full timing coverage failed and resource qualification
cannot proceed. Complete worst-case path coverage is **NOT RUN**.

A13C-R's prior focused SHADOW PASS and A13B's **EFFICACY INCONCLUSIVE;
FOUR-PHASE EFFICACY QUALIFICATION DEFERRED BY OWNER** remain unchanged. This
stage neither implements nor authorizes ACTIVE or metrology claims.

## Identity and frozen software gate

- Start: remote `stage5pa13c-r-focused-hardware`, HEAD
  `4a6f3edfbab74a7fd7ba1ff35c768534c05d2704`.
- Independent worktree `D:\Documents\stm32f103rbt6_a33_stage5pa13d`, branch
  `stage5pa13d-target-resource-validation`.
- Measurement rules and initial software gate committed before device access
  at `900dfacea208ab13b9d7075bf2dbf7115c12facc`; final atomic interface frozen
  at `352be46` and acquisition boundary clarified at `6af262b` before final
  acquisition. Numeric budgets were never loosened after results.
- User's unmodified repair BIN: **0x051D / Map 0x0105**, 93588 B,
  SHA `DDF57FB131E5EF675D94B08A5A8752EB0FB588BA5230DEA6F34C57D6B2F62DDD`.
- Final diagnostic: **0x051E / Map 0x0107**, 94364 B,
  SHA `C0EF4AF2AE1D63FA7B2D414932A5C7B2221380AD2640137A5C67022AD708B7D5`.
- Separate OFF-only pretrial: 0x051E / Map 0x0106, 94292 B,
  SHA `DD81C2E7499E3F013350A4A06359AE7B98006FBDCE4ED7364D1214A48A092D19`.
  It was restored before the final atomic run; no concatenation is claimed.
  Early 48 B and 32 B software prototypes are archived **UNFLASHED**.

The A12/A13 Python models, A13C compensator C mathematics, old R5 source,
calibration and V3 format were unchanged. No communication/DMA/BLE buffer or
stack reserve was reduced. Host Debug and Release **43/43 CTest PASS**, including
R5, display, checkweigh and persistent storage regressions plus the new bounded
scanner/guard/ABI/wrap tests. Python five-block decoder tests **2/2 PASS**;
their fake packets are not hardware evidence. ARM Debug and Release strict
`-Wall -Wextra -Werror` builds passed. The initial MSVC constant-condition
warning failure remains logged; it was fixed using static assertions, not by
disabling warnings.

Actual compiler options: GNU ARM GCC 13.3.1 / CubeCLT 1.18.0, Debug `-Og -g3`,
Release **`-Os -g0`**, with `-fstack-usage -fcallgraph-info=su` and strict warnings.
Earlier inherited documents mentioning generic Release `-O3` must not replace
these actual compile commands. Rebuilt switch-OFF 0x051D matches the user's
exact BIN SHA. The baseline and diagnostic A13C mathematical object files have
the identical SHA **`EFCAA070EC912A2746C89E7C27DAEB39829E9FF953DBF3468038E22B63AC38EC`**.
Baseline/final caller and painter disassemblies, actual maps and callgraphs
are preserved. The wrapper, code placement, reading workload and scanning
still change the diagnostic image's timing; these measurements are **not exact
measurements of the untouched 0x051D image**.

## Audited stack bounds and safe watermark setup

Thread and IRQ execution use MSP; no RTOS/PSP switching is present in the
application. Actual CONTROL reads were 0. The painter checks CONTROL.SPSEL and
IPSR, disables maskable IRQs, uses only caller-saved registers, no push/calls,
and verifies current MSP <= _estack. After `App_Init` returns and before the
first App_Run, it paints aligned **[_ebss, current MSP−64)** with `0xA55A3CC3`.
It cannot touch static objects, the current main frame or its 64-byte exclusion,
or any Flash. Startup calls through App_Init are explicitly **outside** the
watermark; subsequent App_Run, scanner/bookkeeping and enabled IRQ usage are
included. No linked malloc/calloc/realloc/free/_sbrk implementation was found
in the diagnostic ELF.

The scan uses the exact shared host-tested bounded read-only function. Lowest
altered address only decreases; its byte offset is stored without rounding in
12 bits with four safety flags. Initial span >=4096 refuses painting. Empty
bracket overhead is stored exactly in 16 bits; overflow invalidates diagnostics.
Timed-call identity uses 28-bit sequence plus actual post-call state; overflow
also invalidates diagnostics. The reset-started diagnostic duration is bounded
to 7200 seconds, far below this representation limit.

ISR audit found priority-5 UART/DMA/TIM4 can interrupt priority-15 SysTick.
The old one-IRQ analysis omitted this nested SysTick layer. A13D keeps the full
256-byte indirect allowance and adds SysTick software chain plus its exception
frame and **8 B exception-alignment padding**; it does not hide nesting inside
an existing reserve. The 48 B and 32 B variants were reduced to **24 B** of
permanent diagnostics before final use. Actual complete bounds:

| Image / calculation | Static RAM incl. linker reservation | Conservative stack | Collision margin |
| --- | ---: | ---: | ---: |
| Frozen 0x051D Debug, inherited one-IRQ analysis | 19416 B | 1504 B | 584 B (historical, incomplete IRQ assumption) |
| Same Debug with audited nested IRQ + alignment | 19416 B | 1552 B | **536 B** (conservative inference from unchanged call chain) |
| Rebuilt frozen 0x051D Release, audited | 19360 B | 1448 B | **696 B** |
| Diagnostic Debug, audited | 19440 B | 1552 B | **512 B PASS** |
| Diagnostic Release actually installed, audited | 19384 B | 1448 B | **672 B PASS** |

The nominal declared stack reserve remains 1024 B; these are collision margins
against static RAM in the full 20 KiB area, not a claim that the conservative
chain fits inside that declaration. Static analysis is not a runtime watermark.

## Measurement method and frozen rules

`Docs/STAGE5PA13D_FROZEN_MEASUREMENT_PLAN.md` and
`Results/stage5pa13d/frozen_rules.json` specify:

- Configured 10 Hz nominal 100 ms: A13C timed-call maximum **<=10 ms**;
  full App_Run + scan elapsed and loop interval **<=25 ms**; >=75 ms nominal
  remaining service/IRQ opportunity. Actual sample timestamps remain recorded.
- Static collision margin and observed untouched RAM **>=512 B**.
- Every qualified timed-call sequence, normal clock, no interpolation. Producer
  minus consumer equals FIFO; engine/consumer/invalid counts agree. No sustained
  FIFO >1 for >1 s, fault, read-error, overrun, dirty or revision/SAVE change.
- Five fixed read blocks per poll: 0x0000/32, 0x0020/28, 0x01C0/10, 0x0280/40,
  **0x0300/112**. Final block atomically associates locked actual call-state/
  call-sequence/cycles with the authoritative engine sequence/timestamp.
  Live candidate snapshots after a mode-set are not mislabelled as the timed
  call's state. No input operator edge enters the measured controller.
- Qualification starts at the first positive weight-valid RUNNING sample whose
  last timed sequence matches authoritative engine sequence. Earlier startup
  polls remain raw but are not steady-stream coverage claims. Watermark covers
  all post-paint work including that warmup.

DWT is already active for ADC timing and was **not reset** by diagnostics.
Unsigned differences handle wrap. Actual read HCLK was **72000000 Hz**; no
assumed desktop MHz was used. The empty DWT bracket measured **20 cycles**,
0.277778 µs, retained **without subtraction**. Timed calls conservatively
include wrapping/read/prologue and enabled IRQ preemption. Post-read telemetry
bookkeeping and scan affect main-loop timing. Main-loop measurement includes
the scan; start-to-start interval also includes end bookkeeping. No SWD halt
or read occurs inside the final qualified interval.

## Fresh device backup and installation

Fresh preflight actually read 0x051D/0x0105, 10 Hz/filt1/strength3, gain128,
calibration raw zero **41868**, raw span **485780**, mass **500000000 µg**,
V3, revision/saved **19/19**, R5 OFF+SHADOW/offset0, checkweigh OFF, all safe
counters zero. Complete current application/config backups were made; no
historical 0x051C backup was used. ST-LINK reported 3.29 V.

- Application region 126976 B SHA before and after restoration:
  **`6279F17AB9E987BF8156F5F9F805B8DB41CDBDA2951603DC1B55CE559D4B34B5`**.
- Configuration 4096 B SHA before and after:
  **`856BD8F5C14760561FC4BFEC4274FE0C5480C19BC4422046617C2450F8439733`**.
- Both slots valid: active A/19, B/18; device slot code 1 is A.

Final diagnostic install used **`-e '[0 123]'`**, not `-e 0 123`, then write and
Verify. Full application prefix comparison: 0 differences against final
diagnostic BIN; its remaining application tail was all FF. Config region
was identical. All these SWD checks occurred **before** a fresh final reset and
capture. The earlier Map0106 OFF-only pretrial was ended/restored separately
before this check/reinstall sequence and is retained as supplementary data.

## Final failed acquisition and direct measurements

Final capture began **2026-09-28 07:51:50.743484 UTC**; qualified start sequence
4 at **07:51:51.891297 UTC**, MCU 1217 ms. User empty confirmation resulted in
the volatile STATIC request at **07:54:30.735988 UTC**, acknowledgement
**07:54:30.764097 UTC**. Failure latched at **07:54:30.853279 UTC**, immediately
ending the capture. Restoration was verified at **07:54:47.747274 UTC**.

Raw file `hardware/final_measurement/samples.csv`: **1936 polls**,
SHA **`7E32ECD20929517542E4EAB4C9611D8E853F51EF372B385FE01660841B669851`**.
Accepted prefix: **1586 unique timed samples**, sequences 4–1589. Including
the retained final failure row, raw qualified-domain unique observations are
**1587**, with one missing timed-call sample 1590 before observed 1591.
No acquisition retry occurred after this failure.

| Directly observed path | Observed unique calls | Max cycles / sequence | Time at read HCLK | Meaning |
| --- | ---: | --- | ---: | --- |
| OFF | 1586 | 378 / 1021 | **5.250 µs** | Direct diagnostic measurement, includes wrapping/possible IRQ |
| STATIC HOLDOFF | 1 | 477 / 1591 | **6.625 µs** | Retained failure-row measurement; not complete STATIC coverage |
| Reference / observation / TRACKING median | 0 | NOT RUN | NOT RUN | Stopped before these paths |
| Load/unload detection and fast TRACKING / DOSING | 0 | NOT RUN | NOT RUN | No physical load instruction issued |

Observed cumulative App_Run maximum **413346 cycles = 5740.916667 µs**;
loop interval maximum **413394 cycles = 5741.583333 µs**. These are actual
diagnostic counters, not Host 4.33 µs and not a prediction of missing paths.
The unknown timed call 1590 is not assigned a manufactured duration or state.

Runtime scan minimum: static end **0x200047B8**, lowest altered word
**0x20004D0C**, stack top **0x20005000**, hence **1364 B untouched** and **756 B
top-to-touched usage**. Flags remained valid and the lower sentinel unbroken
in all 1936 polls. This is genuine post-paint watermark evidence for the
**observed OFF/one-HOLDOFF workload only**, not a passed high-water qualification
for the missing median/step/DOSING paths.

Final gap row: earlier ordinary block sequence **1590**, atomic timed sequence
and produced/consumed/engine **1591**, FIFO **0**. Accepted timed endpoints are
1589/160013 ms and 1591/160213 ms. Driver read error/overrun/invalid/fault/dirty
never grew. The mode command adds a serial round trip to the five-block loop;
the evidence supports last-call timing being overwritten before host retrieval.
This is not proof of an algorithm timing or RAM violation, but is a failure
of the required zero-gap measurement gate. Removing this control window and
claiming continuous PASS is prohibited.

## Rollback, terminal state and next action

The single serial owner closed and invoked the validated current-run restore
helper. Complete application backup was written at 0x08000000 with interval
erase 0–123 and Verify; both complete application and config were uploaded
again. **Application byte differences=0, configuration byte differences=0**.
The restored prefix is the exact DDF57FB1… repair BIN and full tail FF.
Configuration did not need writing; no SAVE/ZERO/TARE/calibration was issued.
Fresh COM5 after the failure reads **0x051D / Map 0x0105, OFF+SHADOW, offset0,
limited0, fault/overrun/dirty0, revision/saved19/19**, original calibration,
10 Hz/filt1/3 and checkweigh OFF. Diagnostic firmware is not left installed.

| Gate | Outcome |
| --- | --- |
| Frozen math/object identity, Host/strict ARM, complete static RAM/callgraph | PASS |
| Diagnostic install/Verify/full-tail/config preservation | PASS |
| Observed OFF/HOLDOFF time and observed watermark bounds | Within bounds, partial evidence only |
| Required continuous timed-call coverage | **FAIL: missing 1590** |
| Reference/observation/TRACKING, step/boost/DOSING resource coverage | **NOT RUN** |
| Exact 0x051D restoration + safe terminal | PASS |
| A13B correction efficacy | **INCONCLUSIVE; unchanged** |
| Overall | **A13D TARGET RESOURCE GATE FAIL** |

Do not enter ACTIVE on this result. A next separately authorized A13D attempt
should first revise/test the **host mode-command handoff** to retain a genuine
atomic timing read before/after control traffic, or another statically safe
acquisition scheme. Keep the same timing/RAM/zero-gap limits. Do not interpolate
1590, reuse this incomplete stream as full coverage, tune the compensation,
shrink communication buffers, or discard this failed evidence.

## Evidence and reproduction

Root `Results/stage5pa13d/`: `frozen_rules.json`, `software_gate.json`, actual
strict maps/callgraphs/disassembly/build logs, unflashed prototypes, original
application/config backup, both diagnostic install identities, supplementary
OFF-only pretrial, final raw CSV/events, automatic rollback logs and complete
readbacks, `qualification_results.json`, terminal fresh read and SHA manifest.
Historical A13C-R/A13B artifacts are untouched; original `.pyc` and worktree
temporary files are not cleaned or committed.

```powershell
python -B Tools/stage5pa13d/test_capture_resources.py
python -B Tools/stage5pa13d/score_resources.py --directory Results/stage5pa13d/hardware/final_measurement --output local_rescore.json
python -B Tools/stage5pa13d/nested_stack_gate.py --analysis Results/stage5pa13d/software/arm_debug_atomic_analysis --callgraph-root Results/stage5pa13d/software/arm_debug --output local_static_gate.json
```

These commands do not access the device. Rebuilding static gates needs the
archived compile commands and CubeCLT compiler. See manifest for exact-byte
digests; diagnostic numbers and source-static bounds are explicitly separate.
