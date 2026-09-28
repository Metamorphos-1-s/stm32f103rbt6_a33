# Stage 5P-A13C-R — frozen 0x051D boot-fix SHADOW retest

## Decision and frozen boundary

**FOCUSED SHADOW FUNCTIONAL/OBSERVED SAFETY PASS**, only for the current
sensor, 10 Hz, filt1/strength3 and the precisely identified repair BIN below.
This is not ACTIVE approval, full efficacy, absolute-zero or metrology
qualification. A13B remains **EFFICACY INCONCLUSIVE; FOUR-PHASE EFFICACY
QUALIFICATION DEFERRED BY OWNER**. The original A13C run remains
**FAIL_ROLLED_BACK**. No old record or old report was modified or reclassified.

- Starting remote branch `stage5pa13c-fixed-c-shadow-validation`, HEAD
  `5527d99046375531722443e576c162595dca2e6b`.
- Independent worktree `D:\Documents\stm32f103rbt6_a33_stage5pa13cr`, branch
  `stage5pa13c-r-focused-hardware`.
- Frozen Python blobs remain `148fa72aa1438291945ba6eb367d5bce971773d8`
  (A13) and `f81953a141706b3118f5637cdfd53e5d26dd3846` (A12).
  Target candidate source blob `c7014fe5110e823c6d2d3731a69db0d04e6d9f71`.
  No target C, frozen Python parameter, legacy R5 or V3 source change was made.
- Display division **d=0.01 g**, legal verification interval **e=1 g**.

| Artifact | Size | SHA-256 | Hardware meaning |
| --- | ---: | --- | --- |
| Historical failed `firmware_0x051D_a13c_shadow.bin` | 93556 B | `057BCABD847F120A2C78C9B2A39AB899D6579CAD3D52C1E44E2C09E8359A517D` | Prohibited; NOT flashed in A13C-R |
| Frozen `firmware_0x051D_off_bootfix_UNFLASHED.bin` | 93588 B | `DDF57FB131E5EF675D94B08A5A8752EB0FB588BA5230DEA6F34C57D6B2F62DDD` | Actually flashed and tested in A13C-R |

The repair file retains its historical name; `UNFLASHED` describes its status
at A13C, not its new A13C-R status. Reconstruction from the committed ELF
produced the exact repair BIN SHA above. Both binaries say firmware 0x051D /
Map 0x0105; version alone does not identify the tested content.

## Current-run preflight, backups and flash verification

Fresh COM5 read: 0x051C / 0x0104, 10 Hz/filt1/strength3, calibration valid,
raw zero **41868**, raw span **485780**, span mass **500000000 µg**,
revision/saved **19/19**, dirty/fault/overrun **0**, R5 **OFF+SHADOW**, offset 0,
checkweigh OFF. No SAVE, ZERO, TARE, calibration or configuration change.

This run backed up the complete **126976 B** application region and **4096 B**
configuration region. Application before SHA:
`08A5E26DB77914E4B41588F21AA51E1C5390606BDFD77A624B336945FA359A4F`.
Configuration before/final SHA:
`856BD8F5C14760561FC4BFEC4274FE0C5480C19BC4422046617C2450F8439733`.
Both V3 slots were valid; A sequence 19 active, B sequence 18. Device slot
code 1 means **A**, not B. Voltage read by ST-LINK: 3.29 V.

An initial host erase command `-e 0 123` selected two sector codes rather than
the whole interval. The first post-program read found **14261 non-FF bytes**
after the 93588 B repair BIN. This was a host flashing-scope defect; its dump
and logs are retained. Before any STATIC or physical test, the host command
was corrected to **`-e '[0 123]'`**, followed by writing the same SHA-verified
repair BIN, Verify and reset. Subsequent complete application reads prove:

- prefix 93588 B: **0 byte differences** versus the repair BIN;
- bytes 93588 through 126975: **all FF**;
- configuration pages 124–127: byte-identical to current-run backup;
- full padded application SHA after correct programming and at final read:
  `6279F17AB9E987BF8156F5F9F805B8DB41CDBDA2951603DC1B55CE559D4B34B5`.

The erroneous partial erase is not hidden or described as successful. The
failure binary was never involved. After the range correction, the recorder
was started immediately after reset in the same command flow.

## Actual startup and continuous recording

Startup capture began at **2026-09-28 04:03:00 UTC**, before a device sample
was available (sequence/MCU sample timestamp 0). It retained 150 polls over
about 15 seconds: candidate OFF state on every poll, offset 0, no fault,
overrun or dirty condition, no read errors. It observed 128 distinct sample
sequence values; this initial 10 Hz host polling did not capture every device
sequence, so it is not a full-rate parity dataset. Surrounding fresh diagnostic
reads establish limited=0 and application=SHADOW. The old read-only tool's
`status_flags` field swaps the two 16-bit words for nonzero flags; retain that
raw field unchanged, and do not use it as correctly decoded stability evidence.
The new focused recorder decodes flags as low word OR high word shifted 16.

An OFF-only recording then ran **04:11:05–06:11:05 UTC**, and reached its
two-hour timeout before the user's later empty confirmation. Its 147460 polls
covered **71873 distinct consecutive device samples**, 0 unobserved sequences,
0 errors. It automatically remained OFF+SHADOW. A fresh read preceded the
second recorder. The gap between these recordings is explicit; no concatenated
continuous trace is claimed.

Focused continuous run: **06:14:46.237596–06:33:09.562705 UTC**. Final raw CSV:
`Results/stage5pa13c_r/static_load_unload_continuous/samples.csv`, SHA
`499D2F14D2E2A04FEE27369209610089AA39D91CD39B1CBABD04A27B8D2CA838`.
It contains **21753 polls**, **11013 distinct consecutive sequences** 78916
through 89928, **0 missing sequence**, **0 read errors**, no interpolation.
Deduplication for scoring retains the first observation of each candidate
sequence, while the unchanged raw file includes repeated polls. All relevant
STATIC/DOSING samples were covered. Host monotonic and MCU clocks are retained;
UTC is a label, not substituted for the device clock.

Requests/events archive instruction-issued and user-confirmation-recorded
times separately. A confirmation may arrive after the physical edge. Filtered
weight ramp brackets inferred from the records:

| Physical operation | Candidate sequence bracket | MCU millisecond bracket | Meaning |
| --- | --- | --- | --- |
| Load | 80435–80441 | 8059012–8059613 | Six observed >=2 g filtered input substeps |
| Unload | 86450–86455 | 8661580–8662081 | Five observed >=2 g filtered input substeps |

These are sampled **filtered-weight ramp** bounds, not an assertion of exact
operator completion or exact raw ADC onset. Raw ADC, filtered ADC **counts**,
weight, panel and both sequence domains remain independently available in the
CSV. Temperature was **not measured**.

## State timing, DOSING and rate gates

Initial empty STATIC: 79155 HOLDOFF, 79305 REFERENCE_FILL, reference locked at
79604, 79605 OBSERVATION_FILL, **79804 TRACKING**. There was no automatic
fast gate in the empty baseline. Model sample times correspond to the observed
150 protection, 300 reference and 200 observation samples.

| Gate | Cause | Obvious/robust/quiet sequence | Reference lock | First actual correction | Boost deadline |
| --- | --- | --- | --- | --- | --- |
| 1 | Load automatic step | 80441 / 80450 / 80491 | 80899 | 81099 | 83450; observed to completion |
| 2 | Explicit DOSING exit | Mode returned at 85212 | 85661 | 86334 | 88212; superseded by the real unload, as frozen model requires |
| 3 | Unload automatic step | 86455 / 86466 / 86505 | 86915 | 87146 | 89466; observed to completion |

There were exactly **two physical automatic fast gates**, not three. The
third counted gate is the explicitly requested DOSING exit. No extra automatic
step or unexplained reference reset occurred. The 3000-sample fast eligibility
window starts at the robust edge/explicit exit; protection/reference/observation
time is inside that window. Load contributed 2351 TRACKING fast evaluations;
the explicit return contributed 590 before the unload superseded it; unload
contributed 2351. Cumulative fast evaluations **5292**. A fast evaluation need
not move offset when inside the deadband.

DOSING sequences **84057–85211**: **1155 consecutive samples**, actual offset
**85320 µg** at entry and on every sample, **0 violations**. The subsequent
STATIC rebase preserved that offset before reacquiring reference. This is real
nonzero-offset hardware evidence, not a synthetic initial condition.

Observed limits: maximum single consecutive STATIC-sample offset change
**35 µg**, maximum offset change over any observed MCU-time window <=10 s
**3465 µg**, maximum absolute offset **87710 µg**. All pass their frozen bounds.
Neither the ±500000 µg clamp at its boundary nor fault injection was physically
exercised. At each of the 11 large filtered ramp substeps, offset was unchanged
and corrected step equalled uncompensated step: **0 µg immediate step loss**.

The real captured input and actual observed modes were replayed through the
unmodified frozen Python controller using one state instance through STATIC,
DOSING and return. **10774 active samples, 0 mismatches** in corrected, offset,
state, gate/rebuild counts, boost count and reason. No user markers or post-hoc
edge guesses enter this replay. Host evidence-tool tests **3/3 PASS**, including
duplicate/gap handling, actual nonzero DOSING freeze and intentional mismatch
detection. Earlier A13C Host Debug/Release **42/42 PASS**, ARM Debug/Release/
strict-warning and historical sample parity are inherited source-frozen
software results, not newly rerun target builds in A13C-R.

## Official outputs and directly measured phase windows

All **6438 distinct matched realtime/candidate sequence pairs** have formal PLC
gross exactly equal to candidate **uncompensated** input: **0 differences**.
The candidate correction is never selected as formal output. Loaded final
30-second panel median was **500.41 g**, consistent with the 500.415623 g
formal-weight median, not the 500.330169 g candidate median. Empty final panel
median was **0.37 g**, while candidate median was 0.3041005 g. Display holding
and quantization still apply, so individual panel values are not authoritative
mass. Checkweigh remained OFF in actual periodic reads; its latest 1-second
poll fields are identified as not necessarily from the same sample. No alarm
ACTIVE or complete alarm qualification matrix was exercised.

| Window | Uncompensated median (g) | SHADOW median (g) | Offset median (g) | Panel median (g) |
| --- | ---: | ---: | ---: | ---: |
| Empty, last 30 s before load | 0.257934 | 0.257934 | 0 | 0.25 |
| Load, 15–45 s after end of filtered ramp | 500.275956 | 500.275956 | 0 | 500.27 |
| Load, minutes 5–6 | 500.384085 | 500.300264 | 0.083720 | 500.38 |
| Load, last 30 s | 500.415623 | 500.330169 | 0.085320 | 500.41 |
| Empty recovery, 15–45 s after ramp | 0.3981645 | 0.3104545 | 0.087710 | 0.40 |
| Empty recovery, last 30 s | 0.3655005 | 0.3041005 | 0.061400 | 0.37 |

These are separate robust medians, not algebraically substituted mass values.
Raw ADC and filtered counts medians appear in the machine-readable analysis.
The empty recovery lasted about 347.819 s; a complete minute 5–6 window is
**NOT AVAILABLE**. No missing interval is imputed. Window origins are sampled
ramp bounds, not exact manual event times. Robust load-span median is
**500.018022 g**. Robust unload-span medians are **500.0174585 g** uncompensated
and **500.0197145 g** candidate. Their **0.002256 g** difference is enlargement,
not loss, from changing offset in the 30-second pre-unload comparison window;
it must not be hidden or called instantaneous step loss. Actual immediate
substep preservation is separately proven above.

The nonzero empty weights and candidate residual are reported as measurements,
not passed absolute-zero qualification. Efficacy/zero conclusions are not
expanded beyond the owner-approved scope, and this short run is not substituted
for A13B's deferred fourth-phase evidence.

## Runtime evidence, cleanup and qualification limits

Prior target Debug map RAM **19416 B**, conservative collision margin **584 B**
(72 B above gate) remains static evidence. The exact installed Release map RAM
is **19360 B**. A terminal HotPlug MSP read returned **0x20004F90** (112 B below
RAM top 0x20005000). It was obtained **after** continuous Modbus capture ended,
not under full load, and is neither peak stack nor watermark. Target A13C
execution-time instrumentation is not present in this frozen image. Reading a
free-running DWT clock cannot establish per-call cost. Thus **target per-sample
cycles and loaded runtime stack high-water: NOT RUN**; the old 4.33 µs desktop
number is not reused as a target result. No extra diagnostic firmware, stack
painting, breakpoint or intrusive stress test was introduced.

Cleanup used the same serial owner to request volatile OFF, succeeded, then
read fresh diagnostics. Final application byte-read confirms exact repair BIN
prefix and FF tail; config region byte comparison unchanged. Final COM5:
**0x051D / Map 0x0105, OFF+SHADOW, candidate offset/reference 0, limited 0**, calibration
valid with original endpoints, 10 Hz/filt1/3, checkweigh OFF,
revision/saved **19/19**, dirty/fault/overrun **0**, volatile SAVE count **0**.
No candidate ACTIVE was ever requested. The run's final OFF reset increments
candidate rebuild count from 6 to 7 intentionally; historical gate timing
diagnostics remain retained, not cleared or relabelled as a fault.

| Gate | A13C-R outcome |
| --- | --- |
| Exact repair BIN/ELF identity and complete application-area erase/Verify | PASS after recorded host erase-syntax correction |
| OFF startup without LIMITED latch | PASS |
| Empty/static reference and observation timing | PASS |
| One load + one unload automatic gate and immediate step-through | PASS |
| Real nonzero DOSING freeze and STATIC return rebase | PASS |
| Observed rates, offset envelope, official mass isolation, config safety | PASS |
| Physical ±0.5 g clamp boundary or fault injection | NOT RUN |
| Per-call target cycles / loaded runtime stack watermark | NOT RUN |
| A13B full efficacy / absolute zero / metrology certification | INCONCLUSIVE / NOT RUN; unchanged |
| Other filters, 40 Hz, cross-sensor, real slow continuous dosing, ACTIVE | NOT RUN |

Next controlled ACTIVE work still needs target timing/stack margin under load,
explicit ACTIVE isolation/output acceptance, slow real feeding safety boundary,
and the deferred efficacy/absolute-zero qualifications before any broader
product or metrology claim. A focused engineering SHADOW PASS is not authority
to enable ACTIVE in this stage.

## Reproduce and locate evidence

Root `Results/stage5pa13c_r/` includes the current preflash application/config
backups, slot parsing, both erase attempt logs/readbacks, corrected programming
Verify logs, initial boot capture, OFF recordings, focused continuous CSV and
events, fresh postflight, final Flash/config reads and SHA manifest. Original
185k/88k/107k development and independent CSVs were not copied or retuned.
All `.pyc` changes and worktree temporary files remain outside the commit.

```powershell
Get-FileHash Results/stage5pa13c/software/firmware_0x051D_off_bootfix_UNFLASHED.bin -Algorithm SHA256
python -B Tools/stage5pa13cr/test_analyze_focused.py
python -B Tools/stage5pa13cr/analyze_focused.py --input Results/stage5pa13c_r/static_load_unload_continuous/samples.csv --output replay_review.json
python -B Tools/stage5b_hw/parse_config_slots.py Results/stage5pa13c_r/final_config/config_region.bin --json slots_review.json
```

These reproduction commands do not access or modify the device. Evidence
files are committed with exact-byte attributes, so SHA verification is not
altered by checkout line-ending settings. See `manifest.json` and
`qualification_summary.json` for source and artifact digests.
