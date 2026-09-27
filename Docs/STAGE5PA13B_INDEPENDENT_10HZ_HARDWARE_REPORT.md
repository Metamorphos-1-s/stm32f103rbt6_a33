# Stage 5P-A13B: independent 10 Hz STATIC-auto holdout

## Verdict and freeze

**SAFETY PASS; EFFICACY INCONCLUSIVE.** This is a genuine new 10 Hz independent capture AFTER the adjudicator/rules were committed and pushed. Three sufficiently stimulated load/unload phases pass their frozen early-effect gates; the fourth phase has only 22.527 mg of uncompensated 5-6 minute motion, below the pre-registered 40 mg stimulus. It is INCONCLUSIVE, not PASS, even though the hypothetical correction is smaller. The four-phase efficacy qualification as a whole is therefore NOT PASSED. Do not alter the policy using this dataset and then reuse it as independent validation. No product R5 code, C candidate, device configuration, firmware flash, ZERO, TARE or calibration was changed.

- Starting A13 remote HEAD `14a711882eba2a05a0f805507485177728150ee1`, tree `c11577b63ba638b274ac06b045863341da37876a`.
- A13B independent branch `stage5pa13b-independent-10hz-validation`. **Pre-capture freeze HEAD** `074af8c08746680b81cb614b232d1fbe265243ec`, tree `ec71da720917a350181775eae090342ac940eb6f`; the recorder saved this exact HEAD in its environment file. Fixed rule text: `Docs/STAGE5PA13B_FROZEN_SOFTWARE_GATE.md`.
- Locked A13 candidate Git blob `148fa72aa1438291945ba6eb367d5bce971773d8`, A12 sample-clock implementation `f81953a141706b3118f5637cdfd53e5d26dd3846`, and pre-capture independent adjudicator blob `4187fc6e7069acf7473216d730fff33fb24c54f7`.
- Frozen policy: automatic STATIC obvious step; 35 micrograms per 10 Hz sample (nominal 350 micrograms/s) for up to 3000 samples / 5 minutes; slower baseline thereafter, 10 mg deadband, offset limit 0.5 g. DOSING offset freeze exists in the research model but actual device remained OFF throughout this capture. Do not equate display division **d=0.01 g** with legal verification division **e=1 g**.
- Before opening new holdout data: A12 synthetic `6/6 PASS`, A13 synthetic `6/6 PASS`, A13B frozen tests `4/4 PASS`. Old A9 CSV was used as `OPENED_DEVELOPMENT_SMOKE` only. No new measurements entered policy selection or the fixed scoring thresholds.

## User confirmation, identity and configuration

The user explicitly confirmed corrected S+/S- wiring, empty scale and the 500 g standard before this capture. Exact historic rewiring/recalibration times are not independently logged. A COM5-only read-only check and, more importantly, the fresh *at-capture* `environment.json` / first samples verify firmware `0x051C`, Map `0x0104`, 10 Hz (`sample_rate=0`), active `filt1/strength3`, gain3, valid calibration raw zero `41868`, raw span `485780`, 500000000 micrograms calibration mass, calibration sequence5, Persistent Format3, slot1/sequence19, revision/saved `19/19`, dirty/fault/overrun0, R5 SHADOW+OFF/offset0. The separate initial `preflight_readonly.json` was obtained hours earlier; do not substitute its timestamp for the capture-time readback. The after-capture read-only 8-second postflight found the same configuration/calibration, R5 SHADOW+OFF/offset0, revision/saved19/19, dirty/fault/overrun0. Postflight had one missed sequence in its own short diagnostic; it is separate from, and was not appended to, the complete main record.

No synchronized environmental temperature field or operator thermometer reading was available: **TEMPERATURE NOT MEASURED**. This prevents causal separation of common thermal zero motion from load-dependent sensor/fixture recovery.

## Raw continuous acquisition and validity

Official session: `Results/stage5pa13b/20260927T154314Z_independent_10hz_500g/`. First sample **2026-09-27 15:43:14.397 UTC**, last **2026-09-27 18:11:25.477 UTC**; host monotonic span **8891.078 seconds**. 88,765 distinct observed device sample sequences, first1851622 / last1940386: zero sequence gaps and no duplicated *stored* sequence. The fast host polling saw 123,317 repeated sequences and skipped them; these are not missing ADC samples. First/last MCU timestamps247312988/256204770 ms, strictly monotonic, every adjacent device interval94-107 ms, no reset. Recorder read errors0, maximum host-record gap0.171 s (<0.5 s preregistered bound), no interpolated samples. Four complete physical 500 g crossings, exactly one auto STATIC gate each; stage lengths all satisfy the 10 minute pre-load empty, 30-60 minute loaded and >=30 minute unloaded gates. Main CSV SHA-256:

```text
6D11B9A3D44C6E8DC658EB1F41B99975EC339DD7DBD0D3E61791DCA75B777612
```

The 25,558,906-byte raw CSV records raw ADC, filtered ADC **counts** (not filtered mass), authoritative gross/net, conditioned/panel display, sample sequence and primary-block sequence, MCU millisecond timestamp, host monotonic and UTC label, stable bit, persistent and R5 fault/mode/offset counters. Primary/shadow Modbus blocks are not one atomic ADC conversion: their recorded sequence difference stays within the frozen one-sample tolerance. Device-side faults/overrun/dirty are always0, saved revision remains19/19 and SAVE request count remains10. The real R5 device mode is SHADOW+OFF with offset0; every nonzero candidate offset below is an **offline replay**, never actual display correction.

Three adjacent UTC *labels* show about one-second inconsistencies at a second boundary. Device time and host monotonic time are continuous (no physical sampling gap); all stage/event calculations use host monotonic and MCU sample clock, never UTC label differences. The logger builds seconds and milliseconds from separate wall-clock calls; this is a plausible explanation, not a proven host time-change root cause. Original labels remain untouched.

## Physical event timeline: independent host markers vs data edges

Operator START denotes when an instruction was delivered, COMPLETE when the user replied. Neither is a precise physical event time. Gross crossings through 250 g are inferred after the causal replay solely for scoring; brackets are at *host polling resolution*, not sensor DRDY timestamps.

| Phase | START UTC | Gross crossing UTC bracket | COMPLETE UTC | Physical stage duration |
|---|---|---|---|---:|
| Load 1 | 15:55:18.796 | 15:55:49.848-15:55:49.968 | 15:56:31.920 | 1945.673 s loaded |
| Unload 1 | 16:27:43.160 | 16:28:15.539-16:28:15.618 | 16:28:46.920 | 1936.062 s empty |
| Load 2 | 16:59:51.716 | 17:00:31.599-17:00:31.682 | 17:01:01.750 | 2362.078 s loaded |
| Unload 2 | 17:32:30.916 | 17:39:53.683-17:39:53.762 | 17:40:25.335 | 1891.758 s empty |

Pre-first-load empty baseline is approximately755.5 seconds. Especially for unload2, the instruction preceded the physical crossing by over seven minutes. The user message was never substituted for the edge. All absolute times in this table are **UTC on September 27, 2026** (local Asia/Shanghai date September 28 for the later phases).

## Fixed 5-6 minute effect gate: per phase, not average

Each row compares median uncorrected gross versus the OFFLINE candidate, both relative to that phase's complete 15-45 second median after the physical crossing. A stage needs |uncorrected change| >=0.040 g, then >=50% absolute reduction AND <=0.020 g corrected absolute motion.

| Phase | Uncorrected 5-6 min | Candidate 5-6 min | Relative reduction | Frozen phase result |
|---|---:|---:|---:|---|
| Load 1 | +0.082223 g | +0.015079 g | 81.66% | PASS |
| Unload 1 | -0.040548 g | -0.013490 g | 66.73% | PASS |
| Load 2 | +0.095740 g | +0.013185 g | 86.23% | PASS |
| Unload 2 | -0.022527 g | -0.009106 g | 59.58% (descriptive only) | **INCONCLUSIVE: stimulus <0.040 g** |

No point-to-point selection or slope extrapolation was used. `qualification.json` contains all raw/corrected early and endpoint medians and pre-5-minute span values, plus each mode/state sequence. For the real immediate gross crossing the model offset is exactly unchanged and the physical sample step is reproduced exactly. Across robust pre/post medians the raw/corrected spans in event order are (g): `+500.030411/+500.029197`, `-500.022527/-500.028941`, `+500.031537/+500.025995`, `-500.039423/-500.035929`; maximum absolute span difference0.006414 g, below the preregistered one-display-division tolerance0.010 g. A clean relative trend is NOT an absolute zero or legal-mass qualification.

## Safety, timing, cancellations and absolute unloaded zero

- Four, and only four, gates have `source=STATIC_OBVIOUS_STEP`; all four align with data-inferred edges; no spurious acceleration. Each identified step was followed by quiet confirmation ~5.27-5.48 s after the gross crossing; reference locked ~46.34-46.41 s afterward; first correction at ~66.26/76.99/66.26/82.20 s for load1/unload1/load2/unload2. Exact MCU sequence, timestamp and host monotonic times are recorded per gate in `qualification.json`.
- Maximum 10-second offline offset change **3465 micrograms = 0.003465 g** <=0.0035 g frozen ceiling. Maximum absolute offline offset **162878 micrograms = 0.162878 g** <=0.5 g. Actual on-device R5 offset remained zero. No real mode switch to DOSING was executed, so a synthetic DOSING freeze cannot be called a physical continuous-fill qualification.
- Rebuild reasons: four `STATIC_STEP_PENDING`, four `STATIC_STEP_FAST`. No `TIME_GAP`, `STATIC_STEP_RETURNED` or `STATIC_STEP_UNSETTLED` cancellation appeared; full reason/event list retained in JSON. No MCU reboot, real fault, SAVE or configuration change.
- Initial empty pre-load baseline median **+0.085603 g**. At unload1 final five minutes, measured OFF gross **+0.156563 g**, hypothetical corrected **+0.103318 g**; at unload2, measured **+0.177963 g**, hypothetical corrected **+0.080211 g**. Candidate deviations from initial empty baseline are +0.017715 g and -0.005392 g. **Absolute zero qualification NOT RUN**: the reference controller cannot infer a legal zero or actual calibration from its local lock alone. No ZERO was performed.

## Regression, evidence, remaining decisions

Before opening this new dataset: A12 synthetic6/6 PASS, A13 synthetic6/6 PASS and A13B frozen independent gates4/4 PASS. `Results/stage5pa13b/opened_a9_smoke.json` is historical A9 *development* smoke, not this holdout. The fixed, unchanged `Tools/stage5pa13b/independent_replay.py` was executed exactly once against the sealed new CSV with `--data-role INDEPENDENT_HOLDOUT_POST_FREEZE` and its complete expected SHA, saving the original `qualification.json` verdict. The CSV and events were not edited; no thresholds, policy code or fixed rules were modified after the pre-capture commit. Other safety/accuracy stages: target fixed-point C **NOT RUN**, C/Python sample equality **NOT RUN**, target RAM/stack **NOT RUN**, SHADOW-mode hardware compensation **NOT RUN**, actual DOSING/continuous feeding and minimum real filling rate **NOT RUN**, 40 Hz/other filters/cross-sensor/metrology **NOT RUN**.

The only defensible A13B disposition is **SAFETY PASS; EFFICACY INCONCLUSIVE** for this sensor and 10 Hz filt1/strength3 condition. Preserve this low-stimulus fourth phase as a real qualification limitation. A later naturally stronger unload stimulus on newly independent data, with exactly the same frozen candidate and rules, would be needed for four-stage efficacy PASS. Do not tune on this dataset and relabel it as new holdout. With full efficacy not established and real feed safety not tested, there is no authorization in A13B to implement product C, flash firmware or enable ACTIVE output; those remain separate gates.

Reproduce offline (read-only; no COM5):

```text
python Tools/stage5pa13b/test_independent_replay.py
python Tools/stage5pa13b/independent_replay.py --input Results/stage5pa13b/20260927T154314Z_independent_10hz_500g/samples.csv --expected-sha256 6D11B9A3D44C6E8DC658EB1F41B99975EC339DD7DBD0D3E61791DCA75B777612 --environment Results/stage5pa13b/20260927T154314Z_independent_10hz_500g/environment.json --summary Results/stage5pa13b/20260927T154314Z_independent_10hz_500g/summary.json --firmware 0x051C --events Results/stage5pa13b/20260927T154314Z_independent_10hz_500g/events.jsonl --data-role INDEPENDENT_HOLDOUT_POST_FREEZE --output Results/stage5pa13b/20260927T154314Z_independent_10hz_500g/qualification.json
```

Official raw CSV, manual event log, machine/environment and last-state summary, immutable qualification JSON and SHA-256 manifest are colocated in the session directory; separate one-shot preflight/postflight snapshots remain outside it. The original dirty `.pyc` changes, scratch sessions and older `frames.jsonl` in the existing A9 workspace were not modified or submitted. No merge, tag or PR.
