# Stage 5P-A13B pre-capture frozen software and qualification contract

## Identity and scope

Starting remote `stage5pa13-auto-static-creep-development` commit: `14a711882eba2a05a0f805507485177728150ee1`; Git tree: `c11577b63ba638b274ac06b045863341da37876a`. New independent branch: `stage5pa13b-independent-10hz-validation`. No product R5 change, C implementation, new firmware, SWD, flash, SAVE, ZERO, TARE or calibration.

Research candidate file `Tools/stage5pa13/auto_static_review.py` Git blob `148fa72aa1438291945ba6eb367d5bce971773d8`; underlying 10 Hz state machine `Tools/stage5pa12/sample_clock_review.py` blob `f81953a141706b3118f5637cdfd53e5d26dd3846`. The adjudicator checks both blobs at runtime before it will replay any CSV. A13 and A12 policy source files are unmodified.

Frozen candidate: `auto_static_350_5m`, STATIC obvious >=2 g sample step followed by robust step and quiet confirmation; reference = 300 observed samples (30 s), observation = 200 samples (20 s), 10 mg deadband, maximum 35 ug/device sample (nominal 350 ug/s) for at most 3000 samples (5 minutes) from event, then baseline 5 ug/sample; absolute offset cap 500000 ug. DOSING freezes offset, and leaving DOSING re-establishes reference. This validation drives the controller in STATIC using only each arriving gross, sequence and MCU timestamp; it does not feed human markers or retrospectively inferred edges to the controller.

The panel display division **d = 0.01 g**, while the legal verification division **e = 1 g**. These are not interchangeable; this research cannot confer legal metrology qualification.

## Frozen input and safety gates, BEFORE new CSV is opened

CLI: `python Tools/stage5pa13b/independent_replay.py --input CSV --expected-sha256 64_HEX --environment environment.json --summary summary.json --firmware 0x051C --events events.jsonl --data-role INDEPENDENT_HOLDOUT_POST_FREEZE --output qualification.json`.

- Verify SHA-256 first. Missing sequence, MCU timestamp not strictly advancing or >250 ms between samples, host monotonic gap >0.5 s, capture read errors or missed sample sequences, MCU restart, missing mandatory fields or mixed firmware/Map produce `INVALID/INCOMPLETE` before replay; no interpolation.
- Require direct acquisition of 10 Hz (`sample_rate=0`), active filt1/strength3, valid 500 g calibration, matching saved revision, R5 SHADOW/OFF and measured offset 0. Do not change hardware settings to make it pass. Dirty/config revision changes, fault, overrun, overload, SAVE increase or unexpected mode/offset produce `FAIL`. Primary ADC/shadow sample sequences may differ by at most one due asynchronous Modbus blocks and are kept distinct.
- Require exactly two real load/unload cycles. Initial empty baseline >=600 s; each loaded phase 1800-3600 s; each unloaded recovery >=1800 s. Otherwise `INVALID/INCOMPLETE`. Human event log is read only AFTER the complete causal controller replay. The 250 g inferred gross crossing is scoring-only, not a control input.
- Require exactly four STATIC obvious-step gates, each aligned to its physical transition within 30 seconds, with no extra speedup. At the physical crossing, offset must be unchanged from the immediately prior sample and the corrected mass step must equal the raw step. Pre-5-minute to post-15-45-second load/unload span attenuation must not exceed one **display division d = 0.010 g**; the immediate sample step itself must be exact. Limit absolute offset to 0.5 g and maximum 10-second offset movement to 0.0035 g. Record every gate, quiet confirmation, reference-lock sample, first correction sample and every reset/cancellation reason.
- Phase reference = median from 15-45 seconds AFTER inferred transition. 5-minute check = median from the full 300-360 second interval, clipped at the next transition; no point extrapolation. For absolute measured raw movement >=0.040 g, corrected movement must have positive >=50% improvement and absolute residual <=0.020 g or that phase is `FAIL`. Movement below 0.040 g gives `INCONCLUSIVE`, never an effect PASS. Missing complete window gives `INVALID/INCOMPLETE`. Any safety failure is overall `FAIL`; all stimulated stages and safety passed gives `PASS`; otherwise safety PASS with efficacy INCONCLUSIVE.
- Additionally record loaded/unloaded raw and corrected step spans, initial empty baseline, and **absolute** unload zero values; no claim that a relative reference lock automatically zeros the scale. No real minimum fill-rate qualification is authorized; synthetic DOSING freeze is NOT RUN for a physical fill.

Failures are retained as evidence; no post-hoc parameter changes or replay against a tuned candidate may be called independent validation. Even if qualified for this sensor/condition, the next gate is fixed-point C, RAM/stack audit, and SHADOW-only device tests, never firmware flashing in A13B.

## Pre-capture software gate

`Tools/stage5pa13b/test_independent_replay.py`: frozen contract and d/e, exact 40 mg stimulus boundary, 50%/20 mg tests, SHA mismatch, sequence/MCU/host gap, wrong filt configuration, device fault, plus an extra artificial auto-gate. `Tools/stage5pa13/test_auto_static_review.py` and `Tools/stage5pa12/test_sample_clock_review.py` additionally check timing, static auto-step and synthetic DOSING freeze. The A9 development CSV may be used to smoke-test this adjudicator only: it is marked `OPENED_DEVELOPMENT_SMOKE` and is NOT the A13B independent CSV. Software and preflight outcome are recorded before starting any new capture.
