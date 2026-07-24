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

## Modes

GENERAL is the normal configurable mode. CLASS_III_REFERENCE is an engineering reference mode, not a certification claim. It requires kg/g, `e=d`, no more than 10000 intervals, initial zero <=20% Max and combined semi-auto/AZT <=4% Max. Min is `20e` and display overload is `Max+9e`.

Load-cell rated capacity, sensitivity and safe load are metadata with independent known flags. Mechanical overload/safe load remains distinct from legal display overload. Calibration supports rising or falling raw counts and publishes derived sensor direction.

## Defaults and verification

Development defaults are Max 10 kg, e 1 g, kg 3 decimals, g 0 decimals, lb 3 decimals, precision 10 Hz and speed 40 Hz, both gain 128. These are not final product values.

NOT TESTED ON HARDWARE. Noise, response, settling, stability thresholds, zero ranges, real sensor direction and legal-metrology suitability require measured hardware evidence and applicable conformity assessment.
