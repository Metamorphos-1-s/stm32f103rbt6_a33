# Stage 5M-A Current Pipeline Audit

## Source facts

The production path is:

```text
CS1237 signed 24-bit raw
-> driver FIFO
-> MeasurementBridge and RawMeasurement validity
-> WeightFilter in raw-count domain
-> calibration conversion using filtered raw and zero offset
-> optional runtime-drift subtraction (disabled in the frozen product)
-> tare subtraction to net mass
-> StabilityDetector on filtered net mass
-> gross overload check
-> compatibility division conversion
-> DisplayConditioner / Modbus / BLE / alarm consumers
```

Filter 0 is pass-through. Filter 1 is a moving average with a configured four-sample window in the characterization run. Filter 2 is IIR `output += round((input-output)/2^strength)` with strength 1 in the run. Filter 3 is median-of-three followed by the same IIR with strength 3 (`alpha=1/8`); its first two samples are not ready.

The stability detector stores filtered net mass, computes peak-to-peak over the configured eight-sample window, enters candidate at or below the enter threshold, remains stable below the exit threshold, and requires the configured hold time. It has no sustained-slope or monotonic-trend test. This makes a real slow fill appear stable whenever change inside each short window remains inside the threshold long enough.

Manual ZERO changes the raw zero offset and resets runtime drift and stability, but keeps the filter history. TARE changes the net offset and resets stability, but keeps filter history. Calibration apply replaces calibration and resets runtime drift/stability, but the current implementation retains filter raw history. Unit/display changes update conversion metadata and do not reset filter state. Filter/profile reconfiguration constructs a new filter and resets stability/drift. The Stage 5M reference therefore defines explicit reset reasons without changing current product behavior.

The filter advances once per accepted sample and does not use timestamp delta. A missing input means no update; a late sample receives the same coefficient. Stability hold uses unsigned MCU milliseconds and handles wrap, but a large timestamp jump can satisfy elapsed hold once the range is quiet. Production sample sequence increments only after WeightEngine accepts the sample.

Alarm evaluation receives the same `WeightSnapshot`, selects configured net or gross operational mass, and honors its stable flag. Modbus exposes display, net/gross/tare, raw and filtered raw from the same snapshot/display conditioner. BLE FAST contains conditioned display plus operational net/gross/tare; BLE SLOW contains raw, filtered raw and drift fields. The numeric display uses the DisplayConditioner output. These consumers do not yet have public `fast_mass` or adaptive state fields.

## Historical measurements

The frozen Stage 5L metric source reports: filt0 0.01273 g / 0.301 s / 2.104 s; filt1 0.01110 g / 0.441 s / 2.314 s; filt2 0.00929 g / 0.359 s / 3.385 s; filt3 0.00544 g / 1.669 s / 8.278 s for static standard deviation / load 10-90% / stable time. The filt3 median prefilter and `alpha=1/8` tail account for its slow transition, while stable is delayed further by the eight-sample range and hold.

## Inference and limits

The observed 80% stable ratio during approximately 0.0466 g/s filling is consistent with the detector's local-range-only design; it is not evidence that the input was stationary. Host FC03 CSV files omit device samples, although G1/G2 show the device continued processing. Current filter internals therefore cannot be reconstructed bit-for-bit from raw CSV. Existing filter baselines are recomputed from device-published output columns; candidate replay uses the observed raw subsequence without interpolation and records omitted source sequences separately.

Cold-start baseline differences cannot be attributed to temperature, mechanics or electronics because synchronized environmental evidence is absent. Stage 5M-A does not subtract or learn this drift.
