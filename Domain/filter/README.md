# Weight Filter

`weight_filter.c` implements four deterministic raw-count filters:

- `FILTER_MODE_NONE`: direct output; ready after one sample.
- `FILTER_MODE_AVERAGE`: 2..32 sample moving average using an `int64_t` sum.
- `FILTER_MODE_IIR`: first-order integer IIR; strength is a right shift 1..8.
- `FILTER_MODE_MEDIAN3_IIR`: three-sample median followed by the same IIR.

Illegal parameters are rejected. Reset clears all history. The module operates
only on raw counts and performs no calibration, quantization, or stability work.
The production default remains `FILTER_MODE_NONE` because noise behavior is
NOT VERIFIED ON SCALE HARDWARE.
