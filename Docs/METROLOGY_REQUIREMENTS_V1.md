# Metrology Requirements V1

## Authoritative values

- Physical mass is signed 64-bit micrograms (`MassValueUg`).
- Calibration converts raw counts directly to micrograms with checked integer rational arithmetic.
- Gross, tare, net, stability, zero and overload decisions use physical mass, not display counts.
- kg/g/lb values are views of one snapshot; unit switching does not alter zero, tare, filter, stability or calibration state.

## Display

- Exact factors: 1 kg = 1,000,000,000 ug; 1 g = 1,000,000 ug; 1 lb = 453,592,370 ug.
- Per-unit enable, decimal places and division digit 1/2/5 are validated against six display digits.
- Rounding is symmetric half-away-from-zero followed by division quantization.
- Positive maximum is 999999 counts; supported negative minimum is -99999 counts.
- The six-digit limit constrains values that must be displayed or edited. It
  does not constrain 32-bit legacy persistence projections, which use the same
  exact integer conversion and 1/2/5 quantization without a panel-width limit.

## Modes

GENERAL is the normal configurable mode. CLASS_III_REFERENCE is an engineering reference mode, not a certification claim. It requires kg/g, `e=d`, no more than 10000 intervals, initial zero <=20% Max and combined semi-auto/AZT <=4% Max. Min is `20e` and display overload is `Max+9e`.

Load-cell rated capacity, sensitivity and safe load are metadata with independent known flags. Mechanical overload/safe load remains distinct from legal display overload. Calibration supports rising or falling raw counts and publishes derived sensor direction.

## Defaults and verification

Development defaults are Max 10 kg, e 1 g, kg 3 decimals, g 0 decimals, lb 3 decimals, precision 10 Hz and speed 40 Hz, both gain 128. These are not final product values.

NOT TESTED ON HARDWARE. Noise, response, settling, stability thresholds, zero ranges, real sensor direction and legal-metrology suitability require measured hardware evidence and applicable conformity assessment.

## Local panel editing and calibration

- Panel calibration locks the active kg/g/lb unit and its decimal/division
  configuration at entry. The edited count is display-only; the session stores
  the checked `MassValueUg` result from `UnitConverter_CountToMass`.
- Calibration starts from the valid existing span mass or Max, uses 1e/10e/100e/
  1000e steps, and requires `0 < span_mass_ug <= capacity_ug` plus an exact
  six-digit display round trip.
- Capacity, zero range and GENERAL overload are physical-mass edits. Division
  (1/2/5) and decimal places (0..5) change only `unit_display[active_unit]` and
  never reinterpret calibration, zero or tare.
- Filter and stability belong to the selected weighing profile. Individual ADC
  rate/gain editing stays read-only until asynchronous apply can be included in
  the configuration transaction.
- A successful local edit or calibration applies to RAM, increments revision
  and marks configuration dirty. It does not write Flash. The separate SAVE
  operation writes the inactive A/B slot and is the only persistence claim.
- Unit uses a candidate editor: FUNCTION enters, STAR/HASH select the previous/
  next enabled unit, FUNCTION confirms, and TARE cancels. Selection alone does
  not change active configuration; Class III reference mode excludes lb.
- Capacity and overload are independent physical parameters. A lower capacity
  must not silently lower overload. An edit that cannot be rendered in the
  active unit reports a unit-range condition without starting a transaction.
- A display-unit change recomputes only compatibility display counts. It must
  not reset calibration, raw/filter history, stability, zero, tare, physical
  masses, sample timestamp, sample sequence, publication state, or faults.
