# Stage 5M-R5E-D1 Display Pipeline Audit

## Audited path

The actual product path is:

```text
CS1237 raw sample
 -> WeightFilter (active profile MEDIAN3_IIR, strength 3)
 -> CalibrationModel_ConvertMass
 -> uncompensated_gross_mass_ug
 -> R5 reference-lock offset (R5 snapshot corrected gross)
 -> WeightEngine gross/net/tare authoritative snapshot
 -> DisplayConditioner display_mass_ug
 -> unit conversion and 0.01 g display quantization
 -> TM1628 panel
```

`WeightEngine` and the Modbus/BLE snapshot expose authoritative gross/net. The
panel is separately fed by `DisplayConditioner_GetSnapshot()->display_mass_ug`.
Therefore a panel value can remain at an old anchor while authoritative
compensated gross has already moved; this does not by itself indicate an R5
calculation error.

R5 receives the official calibrated, filtered `uncompensated_gross_mass_ug`
with the official timestamp and sample sequence. R5 output is applied to the
WeightEngine snapshot before the display conditioner. The display conditioner
does not feed R5, stability, PLC, Modbus, BLE or alarm calculations.

## Display hold behavior

The display conditioner keeps a locked anchor after an operator ZERO/TARE or a
normal stable display. The release threshold is computed from the active display
division as `8 * display_division_ug`; with g, two decimals and division digit 1
this is `8 * 0.01 g = 0.080 g`. It is a display hysteresis, not legal e (`1 g`),
not the R5 deadband (`0.010 g`) and not the 0.05 g stability-enter threshold.

When the authoritative display source is unstable, the conditioner retains the
old anchor and reports release reason `UNSTABLE`. Once deviation exceeds the
threshold, or a forced release event occurs, it tracks the current source. A
large load/unload step therefore releases promptly; a slow R5 correction below
0.080 g can remain visually stale. The panel's stale anchor does not alter
authoritative PLC/Modbus/BLE weight or R5 input.

R5 `corrected_gross_ug` can differ from authoritative gross by the fixed
post-R5/tare or snapshot scheduling pipeline (observed differences around
0.001127 g); the invariant to test is `corrected = uncompensated - offset`.

Automatic R5 rebase exposes only the latest reason/count in the current public
snapshot. D1 captures the count before and after each controlled interval and
uses host samples to timestamp count increments; no MCU event ring is added.

No display fix is integrated in D1-A. Firmware remains 0x0512 until a complete
controlled diagnosis proves the issue is display-only and a separate offline
candidate passes its gates.
