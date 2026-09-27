# Stage 5P-A13C — fixed-point C and SHADOW engineering attempt

## Decision

**Hardware SHADOW gate: FAIL, safely rolled back.** The first 0x051D
engineering binary was verified and started, but immediately latched
`LIMITED/INVALID_INPUT` while still `OFF + SHADOW`. All 50 read-only failure
samples retained this condition. No 500 g operation, STATIC gate, DOSING
freeze, or ACTIVE test was performed. The original 0x051C application and
configuration were restored/verified byte for byte. After rollback a narrowly
scoped OFF/boot-readiness fix passed software tests, but its separate BIN has
**never been flashed**. Do not treat its host results as a hardware pass.

A13B's frozen conclusion remains `OFFLINE SAFETY PASS; EFFICACY
INCONCLUSIVE; FOUR-PHASE EFFICACY QUALIFICATION DEFERRED BY OWNER;
ENGINEERING C AND SHADOW WORK AUTHORIZED`. A13B and A9 records are regression
inputs here, not new efficacy holdouts. No R5 tuning, V3 migration, factory
reset, ZERO, TARE, calibration or SAVE was performed.

## Identity, connection and baseline

- Starting remote branch `stage5pa13b-independent-10hz-validation`, HEAD
  `b402288c827b67d43e567d5209619b935dfa3e01`; independent A13C branch
  `stage5pa13c-fixed-c-shadow-validation`.
- Frozen Python model: `Tools/stage5pa13/auto_static_review.py`, blob
  `148fa72aa1438291945ba6eb367d5bce971773d8`; sample clock reader
  `Tools/stage5pa12/sample_clock_review.py`, blob
  `f81953a141706b3118f5637cdfd53e5d26dd3846`. Neither changed.
- The only built engineering firmware is `0x051D`, engineering Map `0x0105`,
  product firmware and Map unchanged under the default-OFF compile switch.
  The existing V3 format and both 2 KiB configuration slots were unchanged.
- Scope: current 3 kg sensor, 500 g standard mass, 10 Hz, filt1/strength3.
  Display division `d=0.01 g` and verification interval `e=1 g` are distinct.
- Before flash, COM5 reported `0x051C / 0x0104`, 10 Hz/filt1/3,
  raw-zero 41868, raw-span 485780 at 500000000 µg, revision/saved 19/19,
  dirty/fault/overrun 0, R5 `OFF+SHADOW`, offset 0, checkweigh OFF, V3 slot A
  sequence 19. The device's slot code **1 means A**, not B. SWD serial
  `E1007200D0D2139393740544`, STM32F103 128 KiB, measured 3.29 V.

## Software implementation and regression

`Domain/measurement/a13c_shadow_compensator.[ch]` uses no heap or floating
point. The 300-entry signed-24-bit history is shared between reference and
observation after caching the exact reference median; a 60-entry 32-bit
window tracks step detection. Out-of-envelope values enter LIMITED rather
than silently quantize or overflow. Exact equivalence is established only for
the qualified data and tested engineering envelope, not arbitrary integer
mass inputs. The compatibility adapter replaces—not duplicates—the old R5
state in engineering builds. The effective engineering R5 request is always
SHADOW on boot; ACTIVE is rejected. In SHADOW the engine is explicitly fed
external offset `0` with `apply=false`, so candidate results cannot become
authoritative PLC, display, or alarm mass. Engineering mode transitions do
not persist request modes or reset checkweigh classifications. Separate
registers `0x0300..0x0324` expose actual candidate state, reason, sequence,
uncompensated/corrected gross, offset, and gate timings. The old R5 status
field translates states for compatibility, but its reason code is not
reinterpreted as an A13C reason.

Regression after the OFF/boot-readiness fix:

| Gate | Result |
| --- | --- |
| A13B 88,765 samples, exact SHA `6D11B9A3D44C6E8DC658EB1F41B99975EC339DD7DBD0D3E61791DCA75B777612` | PASS, 0 Python/C sample mismatches |
| A9 107,491 samples, exact SHA `E683609BA0DB7EA090D101CE38569A432EC319982E7D41A9D2176A62B623128C` | PASS, 0 mismatches |
| Six synthetic sequences: step, two cycles, return pulse, motion, sequence/time gap, nonzero-offset DOSING | PASS, 21,620 samples, 0 mismatches |
| Host Debug/Release CTest, including persistence, Modbus, D1-D and checkweigh | PASS, 42/42 each |
| A12/A13/A13B Python invariants | PASS, 6/6 + 6/6 + 4/4 |
| ARM Debug `-Og`, ARM Release `-O3`, strict Release `-Wall -Wextra -Werror` | PASS |
| 35 µg/sample, 3500 µg/100 samples, ±500000 µg offset, step-through and DOSING freeze | PASS, host/invariant and replay; hardware NOT RUN |
| MCU processing cycles and observed MSP watermark | NOT RUN; host-only CPU benchmark 100000 samples at 4.33 µs/sample is **not** a target timing measurement |

For the historical replays the maximum 10-second offset change was 3465 µg;
for the synthetic boundary it was 3500 µg. Gate/quiet/reference/correction
sample timing, candidate state, corrected mass, offset, rebase reason and
counts were compared **on each sample** by
`Tools/stage5pa13c/check_parity.py`. The OFF/boot-readiness fix adds an
explicit test that invalid/fault/overload/near-rail signals received while
OFF cannot latch LIMITED; it does not change STATIC or DOSING trajectories.

Fresh product-equivalent Debug target map: static RAM **19416 B**, versus
**18840 B** at A13B baseline (+576 B). `A13CCompensator` occupies 1336 B,
legacy R5 state was 848 B, engineering compatibility snapshot adds 88 B.
The conservative current call graph still bounds main at 1144 B, IRQ at
72 B, plus 256 B indirect-call allowance: 1504 B combined. Collision
margin **584 B ≥ 512 B**, versus 1160 B at baseline. Release RAM 19360 B.
This is static analysis, not an observed runtime high-water mark; no DMA,
Modbus, BLE buffer or reserved stack capacity was reduced.

The present-tree standard Release `0x0510` ELF SHA is
`CC49641AAD878F2AAACE2AECA6AD30DF278001C4877FB46E323A65921354C2E7`,
byte-identical to a clean build of starting A13B HEAD using the same ARM
toolchain and options. The historical `82E726F5…` artifact belongs to an
earlier release build and must not be substituted as the current baseline
hash. Legacy R5 source/model stays unmodified and the engineering switch
defaults to OFF.

## The observed hardware failure and safe rollback

- Preflash configuration 4096-byte SHA:
  `856BD8F5C14760561FC4BFEC4274FE0C5480C19BC4422046617C2450F8439733`.
  Preflash complete 0x051C application-region (126976 B) SHA:
  `08A5E26DB77914E4B41588F21AA51E1C5390606BDFD77A624B336945FA359A4F`.
- The **actually flashed** 0x051D candidate BIN was 93556 B, SHA:
  `057BCABD847F120A2C78C9B2A39AB899D6579CAD3D52C1E44E2C09E8359A517D`.
  ELF programming erased application sectors **0–91 only**, verified, then
  reset the MCU. Pre/postflash configuration SHA was identical.
- First postflash read at 2026-09-27 20:16:54 UTC: identity 0x051D/0x0105,
  revision/saved 19/19, dirty/fault/overrun 0, mode OFF, application SHADOW,
  candidate offset 0, but **state LIMITED, reason 14 INVALID_INPUT**. A
  subsequent five-second unmodified raw CSV had **50/50** entries OFF and
  LIMITED/reason14, 49 distinct sequences spanning 535–583 (one duplicate,
  no missing sequence in the observed span), no read errors, 50/50 matched
  realtime/candidate sample pairs. SHA of the immutable raw CSV:
  `9CD7FA62D36C7334FB987FCEB357BC6385B9851F77BC5D4C8336D4FBC34E6274`.
  Its `r5_application=0` column was populated as a known code constant by the
  early recorder version, **not measured per row**; reliable application
  evidence is the surrounding actual R5 register reads. Recorder source now
  leaves that fast-poll field empty. Retain the CSV unchanged.
- Root cause: early invalid calibration/ADC readiness calls
  `A13C_Feed(valid=false)` while the requested mode is OFF; the earlier
  implementation entered sticky LIMITED even though no candidate calculation
  should have been armed. A host test now reproduces this exact transition;
  source fix makes OFF inert. A final code audit also made the engineering
  boot-request restore explicitly read-only even if persistent R5 requested
  ACTIVE, so restoring it cannot dirty RAM configuration. This is a
  safety/status defect; the effect
  qualification remains unknown.
- After the failure, **no STATIC or weight test** was attempted. The complete
  original application backup was written back at `0x08000000` with Verify,
  then read back over SWD. Final application SHA matches the original exactly
  (`08A5E26D…FA359A4F`); final configuration SHA matches before exactly
  (`856BD8F5…8439733`). Last COM5 read: `0x051C / 0x0104`, 10 Hz filt1/3,
  R5 `OFF+SHADOW`, offset 0, limited 0, revision/saved 19/19,
  dirty/fault/overrun 0, original calibration preserved.
- The separately built **UNFLASHED** source-fixed candidate BIN is 93588 B,
  SHA `DDF57FB131E5EF675D94B08A5A8752EB0FB588BA5230DEA6F34C57D6B2F62DDD`.
  Its software gates above are PASS; **its hardware result is NOT RUN**. The
  previously failed BIN remains archived unchanged with its own SHA.

## Evidence and next gate

- Machine-readable gate/status: `Results/stage5pa13c/qualification_summary.json`.
- Preflight and backups: `Results/stage5pa13c/hardware/preflight_before_flash/`,
  `backup_preflash/`, `preflash_after_swd/`.
- Failed image/Verify/raw boot evidence: `Results/stage5pa13c/software/firmware_0x051D_a13c_shadow.bin`,
  `Results/stage5pa13c/hardware/flash_0x051D.log`,
  `postflash_readonly/`, `failed_boot_readonly/`.
- Rollback logs/byte readbacks/terminal COM5 read:
  `Results/stage5pa13c/hardware/rollback_0x051C.log`,
  `backup_postrollback/`, `postrollback_readonly/`.
- Reproducible host code/tests and final replay JSON in
  `Tools/stage5pa13c/`, `Tests/host/stage5pa13c/`,
  `Results/stage5pa13c/software/parity_*_off_bootfix.json`; static RAM/stack
JSON in `Results/stage5pa13c/software/arm_candidate_stack_analysis/`.

To replay independently from the committed branch, build the isolated host
runner and use the archived (not new) A13B/A9 inputs:

```powershell
cmake -S Tests/host/stage5pa13c -B build/a13c_host -G 'Visual Studio 17 2022' -A x64
cmake --build build/a13c_host --config Release
python Tools/stage5pa13c/check_parity.py --input Results/stage5pa13b/20260927T154314Z_independent_10hz_500g/samples.csv --sha256 6D11B9A3D44C6E8DC658EB1F41B99975EC339DD7DBD0D3E61791DCA75B777612 --runner build/a13c_host/Release/stage5pa13c_replay_runner.exe --output build/a13c_check_a13b.json
python Tools/stage5pa13c/check_parity.py --input Results/stage5pa9/20260927T060806Z_development_capture/samples.csv --sha256 E683609BA0DB7EA090D101CE38569A432EC319982E7D41A9D2176A62B623128C --runner build/a13c_host/Release/stage5pa13c_replay_runner.exe --output build/a13c_check_a9.json
```

ARM resource evidence used the CubeCLT 1.18.0/GCC 13.3.1 toolchain, the
product-equivalent `A33_ENABLE_STAGE5MR5_BETA`, `STAGE5NB_BETA`,
`STAGE5MR5E_D1D_BETA`, `STAGE5PA_PRODUCT`, `STAGE5PA1_CANDIDATE`,
`STAGE5PA1_MODBUS_OPTIMIZATION`, `STAGE5PA2_PRODUCT`,
`STAGE5PA2C_PRODUCT`, `STAGE5PA2D_CALIBRATION` switches all ON, and
`A33_ENABLE_STAGE5PA13C_SHADOW=ON` only for the candidate. Debug used
`A33_DEBUG_OPTIMIZATION=-Og`, `-fstack-usage -fcallgraph-info=su` and
`Tools/stage5mr5c/analyze_ram_stack.py`. Saved baseline/candidate map files
and analysis JSON are included with the evidence; the default Release
uses none of these engineering switches.

**Not run/deferred:** corrected image hardware boot, target CPU cycles and
runtime stack high-water, empty/loaded 500 g SHADOW transition timings,
hardware nonzero-offset DOSING hold and return, formal display/PLC/checkweigh
AB comparison under operation. A13B's fourth-phase efficacy is still
INCONCLUSIVE by owner decision; other filters, 40 Hz, cross-sensor, slow
real dosing, absolute zero and certified metrology remain outside A13C.
Any renewed physical validation must be a separately authorized attempt using
the precise unflashed BIN SHA, fresh preflight/backups and explicit failure
retention. Do **not** describe this A13C run as engineering SHADOW PASS.
