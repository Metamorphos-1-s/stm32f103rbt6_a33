# Stage 5A-DSP-A Display Conditioning

## Status and baseline

- Product baseline: `2cf8f3739e9ec9ed6c05dfb76e5c196f32d57a85` (`origin/main`).
- Development branch: `stage5a-dsp-a`.
- Software status: implemented; host and ARM results are recorded below.
- Hardware status: **NOT TESTED ON HARDWARE**. Unit tests are not a board-level stable-display test.

This stage reduces visible last-digit movement without changing metrology data. It does not improve sensor accuracy, claim 0.01 g metrological stability, implement standard-weight attraction, or establish Class III compliance.

## Measurement background

The prior investigation used a 3 kg rated load cell, not a 6 kg sensor. It measured about 885.07 counts/g temporary response, about 16.50 counts RMS with the ADC input shorted, and about 17.3 counts RMS with the stable sensor after detrending. That is about 0.0196 g RMS. The estimated 3 kg full-scale response is about 2,655,210 counts; this is an estimate from the empty/500 g observations, not a formal calibration result. Typical 0.06 g movement is consistent with roughly three standard deviations of measured noise. The initial empty-load anomaly was dominated by slow drift.

## Data semantics

Authoritative mass consists of `gross_mass_ug`, `net_mass_ug`, and `tare_mass_ug` in `WeightEngine`. It remains continuous and is used by calibration, zero, tare, overload checks, the Modbus signed 64-bit mass registers, and future BLE true-weight data.

Tracking display mass is the currently selected authoritative net or gross mass before a stable anchor is accepted. Conditioned display mass may temporarily hold a robust anchor. It is used only by the six-digit display, the current-panel Modbus value, the new display telemetry, and a future optional BLE display field.

The paths are:

`CS1237 raw -> WeightFilter -> WeightEngine -> authoritative mass`

`authoritative selected mass -> DisplayConditioner -> unit conversion -> division quantization -> TM1628 formatting`

Calibration and zero/tare logic never consume conditioned display mass. Calibration raw capture bypasses this display-only layer.

## State machine

`DisplayConditioner` is a HAL-free, allocation-free, integer-only domain module in `Domain/measurement`.

- `TRACKING`: output follows authoritative mass. A valid WeightEngine stable indication starts a candidate.
- `CANDIDATE`: output still follows authoritative mass. Only stable samples are collected. Loss of stability returns immediately to tracking.
- `LOCKED`: output holds the anchor while authoritative mass continues updating.

The anchor is the median of the latest nine stable samples. At least nine samples and the active profile's `stability_hold_ms` are required. A zero hold value falls back to 1000 ms. Unsigned subtraction provides correct `uint32_t` timestamp-wrap behavior.

The release threshold is now strictly display-division based:

`display_division_ug * 8`

Multiplication saturates, invalid division falls back to 0.01 g, the result is never zero, and a valid capacity caps it at 1% of capacity. Three consecutive valid samples beyond the threshold release a lock; one isolated spike does not. Loss of WeightEngine stability, overload, calibration, or a disallowed application state releases immediately.

The former formula also included twice the stability-exit threshold. A saved 4 g development threshold therefore produced an 8 g display-release threshold, which explains why a sustained 1 g load could change authoritative mass without releasing the panel lock.

Successful ZERO and successful TARE in net view create an explicit display-only zero anchor. WeightEngine recomputes its snapshot synchronously, so no pending operation state is required. The first subsequent valid sample captures a separate authoritative release reference once; this prevents normal post-operation settling from being measured against the visual zero itself. The reference then freezes. The initial grace remains 1000 ms, while continuous instability has a separate bounded 3000 ms timeout so an 8-sample stability window can rebuild before the anchor is abandoned. Three consecutive samples beyond the normal threshold relative to the frozen reference still release immediately. The authoritative gross, net, and tare values continue to update normally.

## Forced tracking events

Successful ZERO and successful TARE in net view request the operator zero anchor described above. Entering and leaving the menu without applying a change preserves an existing lock. Restore-zero, clear-tare, TARE in gross view, net/gross view change, unit change, profile/config/filter/CS1237 reconfiguration, calibration begin/apply/cancel, storage restart or factory-default application, explicit reset, overload, and FAULT entry force tracking. Forced tracking immediately exposes current authoritative mass and clears the old anchor and counters.

The existing STABLE lamp continues to mean WeightEngine stability. It does not mean display lock. `stable=true, locked=false` is valid during the candidate interval. Lock state is available through Modbus.

## Filter configuration

All existing modes remain supported: `NONE`, `MOVING_AVERAGE`, `IIR`, and `MEDIAN3_IIR`, including the existing persisted `filter_strength` fields. New-device and factory-default high-precision configuration is 10 Hz, `MEDIAN3_IIR`, strength 3, stability enter 0.05 g, exit 0.10 g, and hold 1000 ms. The development default represents a 3 kg rated sensor and a 3 kg configured scale, with 60 g General-mode zero range. Sensor rated capacity and user-configured CAP remain distinct fields.

Saved configuration is preserved except for one restricted compatibility case: an exact match for the former high-precision development tuple (10 Hz, gain 128, `MEDIAN3_IIR`, strength 3, window 8, enter 2 g, exit 4 g, hold 500 ms) is normalized in RAM to the new stability values. If that exact tuple also has zero range 0 in General mode, zero range becomes 60 g. CAP, OL, unit/display settings, calibration, and communications are not changed. The existing dirty/migration-pending mechanism requires an explicit SAVE before this becomes persistent.

Profile mode and strength remain configurable through active/staging Modbus registers (`0122/0123` and `0130/0131`). An `FStr` panel menu item was deliberately deferred to avoid expanding the current menu in this stage. If added later, it should expose moving-average window length or IIR shift strength and use the existing filter validator.

## Modbus semantics

Map version is `0101`. No existing address moved.

- `0000-0001`: final current-panel display count, after conditioning and unit/division conversion.
- `0006-000B`: authoritative net/gross/tare converted display counts, unchanged.
- `0010-001B`: authoritative signed 64-bit net/gross/tare micrograms, unchanged.
- `01E0-01F0`: read-only display-condition telemetry, including signed 64-bit conditioned and anchor masses.

All multi-register values follow the configured high-word-first/low-word-first setting. Modbus cannot write an anchor, lock state, or authoritative mass. Schema V2, V1-to-V2 migration, and Flash A/B addresses remain unchanged.

## Verification

Host tests cover the three-state lifecycle, nine-sample median, insufficient samples/time, unstable fallback, lock hold, a 0.2 g isolated-spike rejection, +/-0.03 g noise, sustained 1 g and 500 g release, immediate reset conditions, negative and boundary masses, timestamp wrap, threshold saturation, operator-anchor grace, application/display integration, menu LONG handling, Modbus compatibility, word order, and read-only enforcement.

All formal ARM presets build without errors: Debug, Release, and BoardDiagnostics. Compared with HF1 baseline `eed02b7` built using the same toolchain, Debug adds 956 B Flash and 24 B RAM, Release adds 540 B Flash and 16 B RAM, and BoardDiagnostics adds 964 B Flash and 24 B RAM. Linker region results are Debug 105,516 B Flash / 10,832 B RAM, Release 57,172 B / 10,832 B, and BoardDiagnostics 103,308 B / 10,824 B. The linker still ends application Flash at `0x0801F000`; configuration slots remain `0x0801F000-0x0801F7FF` and `0x0801F800-0x0801FFFF`.

Board validation remains pending for this hotfix. Recommended tests use the normally calibrated 3 kg sensor, the 500 g weight, 10 Hz, `MEDIAN3_IIR` strength 3, simultaneous panel observation, and polling of authoritative and display telemetry.

## Follow-up

Stage 5A-DSP-B may evaluate optional standard-weight assistance as a separate, default-off feature; it must not alter authoritative mass. Stage 5C BLE can now expose two explicit fields: true authoritative mass and optional panel display mass. Remaining work is the optional `FStr` panel editor and physical board verification.
