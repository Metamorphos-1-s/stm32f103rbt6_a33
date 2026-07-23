# Measurement Domain

`raw_measurement.c` retains Stage 2B HAL-free raw statistics.

Stage 3 adds:

- `weight_types.h`: `WeightValue`, status/action enums, and `WeightSnapshot`.
- `weight_math.c`: checked integer rounding, absolute value, and quantization.
- `metrology_config_validator.c`: strict metrology/stability validation.
- `weight_engine.c`: the unified filter, calibration, gross/net, zero/tare,
  quantization, stability, zero-status, and overload pipeline.

`WeightSnapshot` is the only formal weight data output. The engine updates for
every accepted raw sample and never pushes events or touches hardware.
