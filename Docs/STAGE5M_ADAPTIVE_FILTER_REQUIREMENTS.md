# Stage 5M Adaptive Filter Requirements

Status: Stage 5M-A offline reference evaluated; no acceptable candidate selected and no product integration authorized.

## Evidence-driven goals

Production mode 3 / strength 3 produced the lowest observed empty net standard deviation (0.00544 g) but approximately 1.65 s 10-90% response and 8.25 s to sustained stable. Modes 0-2 responded in 0.22-0.44 s and stabilized in 2.08-3.39 s, with higher noise. A future design should preserve the precision path without forcing every dynamic response through its full delay.

```text
raw ADC
-> bounded spike reject
-> fast path
-> precision path
-> motion detector
-> hysteretic adaptive state transition
-> measurement/display consumers
```

Candidate states are `ZERO_IDLE`, `STABLE`, `MOVING_SLOW`, `MOVING_FAST`, and `SETTLING`.

Requirements:

- Large changes use a fast filtered path, never completely unfiltered raw data.
- Near-stable data transitions gradually to the precision path.
- Stable state uses the precision path and preserves current noise performance.
- State transitions require hysteresis, minimum dwell time, and bounded reset behavior.
- Filter state must be explicitly initialized on load/unload, profile, calibration, zero, tare, and fault transitions.
- Automatic zero tracking is outside Stage 5M-A and is not implemented. Any future proposal requires a separate explicitly configurable and bounded stage.
- A PLC/digital `FILL_ACTIVE` or `PROCESS_ACTIVE` input must inhibit drift learning and zero tracking.
- Telemetry must expose state, selected path, transition reason, dwell time, and both fast/precision values in a diagnostic build.
- No known-mass attraction or special 500 g snapping is allowed; only normal division quantization is permitted.
- Acceptance must include noise, t10/t90, settling, repeated load/unload, creep, hysteresis, and slow-fill false-stable tests.

Stage 5M-A screening thresholds are frozen only for offline comparison. They are not production thresholds. The 40 Hz product path remains contained and raw-baseline causality remains unresolved.
