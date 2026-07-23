# Calibration Model

`calibration_model.c` builds, validates, and applies a two-point integer
calibration. Both positive and negative sensor directions are supported.

The conversion is:

```text
effective_zero = raw_zero + zero_offset_raw
weight = (filtered_raw - effective_zero) * span_weight
         / (raw_span - raw_zero)
```

Division uses symmetric nearest rounding with exact halves away from zero.
Raw span magnitude must be greater than 1000 counts; this is only a software
safety threshold and is NOT VERIFIED ON SCALE HARDWARE. Calibration data is
not written to Flash in Stage 3, and the default configuration remains invalid.
