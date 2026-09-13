# Stage 5N Checkweigh and Alarm Requirements

Status: requirements only; no implementation in Stage 5L.

Candidate modes:

- `STATIC_CHECK`: current precision-oriented stable confirmation.
- `DYNAMIC_CHECK`: bounded fast-path evaluation for moving product.
- `FAST_TRIGGER_STABLE_CONFIRM`: fast threshold crossing followed by precision confirmation.

Requirements:

- Alarm timing must not be completely limited by the display's strongest filter or hold state.
- Each mode must declare its weight source, latency bound, confirmation rule, hysteresis, latch duration, and invalid/fault behavior.
- Threshold comparisons use canonical micrograms and explicit word order; display rounding is not the alarm source.
- Overload and invalid-weight states dominate LOW/OK/HIGH decisions.
- Static confirmation requires stability; dynamic triggering requires a bounded motion-qualified window.
- Fast trigger and stable confirmation results must be separately observable.
- Configuration ownership, SAVE behavior, and Map `0x0104` remain unchanged until a separately reviewed contract change.
- Tests must cover both threshold directions, hysteresis, noisy boundary crossings, load/unload asymmetry, filter-state transitions, stale data, and communication concurrency.

The current Stage 5L data shows that `stable` may remain asserted for much of a 0.0466 g/s real fill, so stable alone is not a sufficient process-state discriminator.

