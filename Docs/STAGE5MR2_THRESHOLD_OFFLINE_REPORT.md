# Stage 5M-R2 Threshold Offline Report

Status: PRELIMINARY OFFLINE CANDIDATE MODE GATED.

## Outcome

A pure automatic threshold classifier is not accepted. Real static data and very small dosing overlap in the observable rate domain: the 60-second apparent-rate median of the three static captures is about 0.30 to 0.34 g/h, while 0.001 g every 10 seconds is 0.36 g/h. With no declared minimum load increment, no threshold can guarantee both drift correction and preservation of every slow addition.

The viable simplified contract is therefore explicit mode gating:

- STATIC_COMPENSATION: slow drift correction may update.
- DOSING_NO_COMPENSATION: offset is frozen; no drift update is allowed.
- PLC or the front-panel menu enters DOSING_NO_COMPENSATION before dosing.
- Re-entering STATIC_COMPENSATION starts a 15-second hold-off and rebuilds the reference at the new weight.

The mode is a safety boundary, not a tuning hint.

## Candidate

The isolated Python candidate uses a 300-second non-overlapping observation window, 15-second robust endpoint medians, 0.008 g drift deadband, 0.2 g/h maximum drift rate, 30% distributed-direction requirement, a 0.020 g fast-step detector, and a correction ramp limited to 0.003 g/min.

Flat subwindows count against direction consistency. This prevents a short burst followed by a long plateau from being labeled as continuous drift. Compensation targets are anchored rather than cumulatively added, and are released when the raw value returns toward the accepted reference.

## Offline evidence

The source is frozen at commit de7f610cc5677b18337d477ec781e66217db2db0. Seven immutable Git blobs are read: two 60-minute cold-start controls, one 30-minute 500 g control, load and unload steps, and two interrupted drip-fill captures.

Static results from the corrected evaluator:

| Run | Input slope g/h | Corrected slope g/h | Input 60 s range g | Corrected range g |
|---|---:|---:|---:|---:|
| Cold start 1 | 0.023508 | 0.015067 | 0.038301 | 0.033795 |
| Cold start 2 | 0.167963 | 0.166838 | 0.134051 | 0.132925 |
| 500 g load | 0.016558 | 0.010445 | 0.058577 | 0.056324 |

The corrected block evaluator requires at least 80% coverage in each 60-second bucket. The old isolated last sample is no longer treated as a complete endpoint. A synthetic 30-minute constant tail uses the robust final 60-second median rather than the last sample.

Mode-gated load preservation covers:

- 12 real cases: four real load histories times initial offsets 0 and plus/minus 0.005 g.
- 252 synthetic cases.
- Increments from 0.0001 to 0.020 g.
- Pauses of 2, 5, and 10 seconds.
- Dosing durations of 60, 120, 240, and 600 seconds.
- A 30-minute static period after re-enabling compensation.

All mode-gated cases have zero offset change during dosing, preserve the pre-existing offset, and produce no post-enable chase on constant data. The maximum 10-second correction remains 0.0005 g.

## Rejected paths

Cumulative reference chasing failed because cold-start control 1 returns toward its initial value. A single accepted window left a false residual correction.

Anchored automatic classification improved that failure but still consumed short dosing bursts and some 0.010 to 0.015 g single changes when compensation remained enabled.

A sliding-window variant was rejected because repeated reference rebasing amplified two static slopes.

Local speed alone was rejected because static noise and 0.001 g per 10-second dosing overlap.

## Boundary and next gate

This branch contains offline tooling only. It does not link the candidate into Debug, Release, BoardDiagnostics, or any product image. It does not authorize a flash operation or hardware shadow run.

Before hardware work:

1. Port the selected state machine to bounded fixed-point C.
2. Prove Python/C equality on actual and synthetic samples.
3. Define runtime-only mode semantics, default-disabled behavior, PLC command, menu control, and read-only BLE visibility.
4. Add mode-owner and reboot behavior tests.
5. Re-run independent static captures not used for parameter selection.
6. Only after those gates pass, build a non-product shadow diagnostic.


## Remote verification

GitHub Actions run 34865886583 completed successfully for commit 23fdf397c3f9581443ec0d29daa755bacf718bbd.

- Isolated-scope check: PASS.
- Python unit tests: 5/5 PASS.
- Immutable evidence replay: PASS.
- All static, mode-gated load-preservation, frozen-offset, post-enable and 10-second update-budget gates: PASS.
- Uploaded artifact: stage5mr2-threshold-offline-report.
- Artifact SHA-256: 217efd4630c27568f18760b8e4e7bd2c2d17df5365c7a2fdf186aa063614e78c.

Final status for this branch: STAGE 5M-R2 MODE-GATED THRESHOLD OFFLINE CANDIDATE PASSED; FIXED-POINT C AND HARDWARE SHADOW PENDING.

No product source, product build target, persistent format, public protocol, firmware image, hardware configuration or device was changed. No merge, tag or pull request was created.
