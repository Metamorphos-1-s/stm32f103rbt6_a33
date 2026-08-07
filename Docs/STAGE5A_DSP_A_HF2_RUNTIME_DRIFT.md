# Stage 5A-DSP-A-HF2

Baseline: `71cb18bdbe9901ac9007046daac60c4878cc5767` (`stage5a-dsp-a-hf1`)

This hotfix contains three bounded changes:

- The A33 hardware profile is 3,000,000,000 ug. Product configuration accepts
  `CAP > 0`, `OL >= CAP`, and `OL <= 3 kg`. The persistence decoder remains
  structurally tolerant of old records. Startup normalization changes only the
  exact legacy `CAP=3 kg, OL=10 kg` pair when the old or HF1 high-precision
  profile fingerprint is present. It sets the existing dirty/migration flags;
  it never writes Flash by itself. The OL menu shows the range warning but opens
  an editable 3 kg recovery value, so cancel leaves the active record unchanged.

- Operator ZERO/TARE display anchors are fixed at 0 ug. The first subsequent
  sample is never used as a new reference. Deviation is released after three
  samples above the existing release threshold, including a rapid approximately
  1 g load within 100 ms.

- `RuntimeDriftCompensator` is a provisional engineering-mode, HAL-free,
  volatile additive compensation. It is disabled by default and can only be
  enabled through the explicit command mailbox command. It uses 5 min ARMING,
  60 s windows with at least 300 stable samples, 10,000 ug deadband, 500 ug
  per-window step limit, +/-500,000 ug cumulative limit, and a 100,000 ug load
  change guard requiring three consecutive samples. Load changes freeze the
  learner and preserve the current offset. ZERO, calibration, profile/filter
  reconfiguration, reference-invalidating faults, and power-up clear the
  volatile state. TARE/CLEAR TARE preserve the offset, discard the old window,
  and re-arm on uncompensated gross even while tare remains active. Transient
  sample, UI/display and communication faults freeze and preserve the offset.

The operational mass path is:

`raw -> calibration/zero offset -> uncompensated gross -> runtime offset -> operational gross -> tare -> net -> display conditioner`.

The uncompensated gross, runtime offset, compensation state, reference, error,
elapsed timers, and sample count are observable in read-only Modbus registers
`0x0200..0x021D`. Existing addresses are unchanged; map version is `0x0102`.
The offset is not writable as a normal register.

The compensation is explicitly **PROVISIONAL RUNTIME DRIFT COMPENSATION / ENGINEERING MODE / NOT METROLOGICALLY VALIDATED**. The 12-hour synthetic result is a software safety test only, not a metrological claim.

## Verification

- `cmake --preset Debug`
- `cmake --build --preset Debug --clean-first`
- Visual Studio host build with `cmake -S Tests/host -B Tests/host/build-vs -G Ninja`
- `ctest --test-dir Tests/host/build-vs --output-on-failure`: 7/7 passed
- 12-hour microgram synthetic drift: final offset 230,000 ug, final residual
  10,000 ug, maximum hourly offset change 20,000 ug; unload entered FROZEN and
  retained the offset.

HF2-R1 replaces the per-sample truncated online average with a checked int64
sum/count window average and defines explicit reset/freeze reasons. See
`STAGE5A_DSP_A_HF2_R1_REVIEW_FIXES.md`.

Hardware status: software implemented; not tested on hardware in HF2-R1.
