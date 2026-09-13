# Stage 5O Conveyor Weighing Requirements

Status: requirements only; no implementation in Stage 5L.

For discrete products, candidate states are `IDLE_ZERO`, `ITEM_ENTER`, `CAPTURE`, `RESULT_VALIDATE`, `RESULT_LATCH`, `ITEM_EXIT`, and `WAIT_NEXT`.

The available sample count is:

```text
N = sample_rate * useful_item_dwell_time
```

At 10 Hz, a 0.5 s useful dwell yields only five nominal samples before filtering and settling losses. At 40 Hz it would yield 20 nominal samples, but Stage 5L did not validate 40 Hz: the observed processed rate was 16.496 Hz with a raw outlier and shifted baseline. No 40 Hz capacity claim is currently allowed.

Requirements:

- Item entry/exit should be provided by a sensor or PLC state where practical, not inferred only from weight slope.
- Capture windows, minimum valid samples, invalid/outlier policy, result latch, and next-item clearing must be bounded.
- Fast and precision estimates may coexist; result validation selects an explicitly qualified value.
- Timing evidence must include DRDY period, processed sequence, overrun, CPU load, communication latency, and item dwell.
- Zero tracking and drift learning are disabled during item presence and process activity.
- Missed entry/exit, overlapping items, overload, reverse motion, and communication loss require deterministic recovery.

Discrete dynamic checkweighing and continuous bulk-material belt integration are different products. Continuous bulk flow requires belt speed, material-flow state, integration over time, totalization, and loss-in-weight/error handling; it must not reuse the discrete item state machine as if the requirements were equivalent.

