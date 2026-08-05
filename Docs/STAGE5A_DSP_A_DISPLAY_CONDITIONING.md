# Stage 5A-DSP-A Display Conditioning

## Status and baseline

- Product baseline: `2cf8f3739e9ec9ed6c05dfb76e5c196f32d57a85` (`origin/main`).
- Development branch: `stage5a-dsp-a`.
- Software status: implemented; host and ARM results are recorded below.
- Hardware status: **NOT TESTED ON HARDWARE**. Unit tests are not a board-level stable-display test.

This stage reduces visible last-digit movement without changing metrology data. It does not improve sensor accuracy, claim 0.01 g metrological stability, implement standard-weight attraction, or establish Class III compliance.

## Measurement background

The prior 6 kg load-cell investigation measured about 885.07 counts/g sensitivity, about 16.50 counts RMS with the ADC input shorted, and about 17.3 counts RMS with the stable sensor after detrending. That is about 0.0196 g RMS. Typical 0.06 g movement is therefore consistent with roughly three standard deviations of measured noise. The initial empty-load anomaly was dominated by slow drift. Those findings motivate display conditioning; the diagnostic firmware and its capture artifacts are not part of this product branch.

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

The release threshold is:

`max(display_division_ug * 8, stability_exit_threshold_ug * 2)`

Multiplication saturates, invalid division falls back to 0.01 g, the result is never zero, and a valid capacity caps it at 1% of capacity. Three consecutive valid samples beyond the threshold release a lock; one isolated spike does not. Loss of WeightEngine stability, overload, calibration, or a disallowed application state releases immediately.

## Forced tracking events

The display state is reset after zero, restore-zero, tare, clear-tare, net/gross view change, unit change, profile/config/filter/CS1237 reconfiguration, calibration begin/apply/cancel, storage restart or factory-default application, explicit reset, overload, and FAULT entry. A reset immediately exposes current authoritative mass and clears the old anchor and counters.

The existing STABLE lamp continues to mean WeightEngine stability. It does not mean display lock. `stable=true, locked=false` is valid during the candidate interval. Lock state is available through Modbus.

## Filter configuration

All existing modes remain supported: `NONE`, `MOVING_AVERAGE`, `IIR`, and `MEDIAN3_IIR`, including the existing persisted `filter_strength` fields. New-device and factory-default high-precision configuration is 10 Hz, `MEDIAN3_IIR`, strength 3. Saved user configuration is not overwritten. The high-speed profile remains 40 Hz with no filter by default.

Profile mode and strength remain configurable through active/staging Modbus registers (`0122/0123` and `0130/0131`). An `FStr` panel menu item was deliberately deferred to avoid expanding the current menu in this stage. If added later, it should expose moving-average window length or IIR shift strength and use the existing filter validator.

## Modbus semantics

Map version is `0101`. No existing address moved.

- `0000-0001`: final current-panel display count, after conditioning and unit/division conversion.
- `0006-000B`: authoritative net/gross/tare converted display counts, unchanged.
- `0010-001B`: authoritative signed 64-bit net/gross/tare micrograms, unchanged.
- `01E0-01F0`: read-only display-condition telemetry, including signed 64-bit conditioned and anchor masses.

All multi-register values follow the configured high-word-first/low-word-first setting. Modbus cannot write an anchor, lock state, or authoritative mass. Schema V2, V1-to-V2 migration, and Flash A/B addresses remain unchanged.

## Verification

Host tests cover the three-state lifecycle, nine-sample median, insufficient samples/time, unstable fallback, lock hold, isolated spike rejection, three-sample deviation release, immediate reset conditions, negative and boundary masses, timestamp wrap, threshold saturation, 0.02 g disturbance, 0.1 g step release, application/display integration, menu LONG handling, Modbus compatibility, word order, and read-only enforcement.

All formal ARM presets build without errors: Debug, Release, and BoardDiagnostics. Compared with the baseline built using the same toolchain, Debug adds 3,120 B Flash and 136 B RAM, Release adds 1,424 B Flash and 128 B RAM, and BoardDiagnostics adds 3,120 B Flash and 136 B RAM. Linker region results are Debug 104,560 B Flash / 10,808 B RAM, Release 56,632 B / 10,816 B, and BoardDiagnostics 102,344 B / 10,800 B. The linker still ends application Flash at `0x0801F000`; Release loaded data ends at `0x0800DD40`, and configuration slots remain `0x0801F000-0x0801F7FF` and `0x0801F800-0x0801FFFF`.

Board validation remains pending. Recommended board tests use a normally calibrated 6 kg sensor, the 500 g weight, 10 Hz, `MEDIAN3_IIR` strength 3, simultaneous panel observation, and polling of authoritative and display telemetry.

## Follow-up

Stage 5A-DSP-B may evaluate optional standard-weight assistance as a separate, default-off feature; it must not alter authoritative mass. Stage 5C BLE can now expose two explicit fields: true authoritative mass and optional panel display mass. Remaining work is the optional `FStr` panel editor and physical board verification.
