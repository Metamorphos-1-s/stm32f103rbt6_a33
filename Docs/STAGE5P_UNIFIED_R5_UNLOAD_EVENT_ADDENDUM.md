# R5 unload-event addendum

The user's later confirmation states that a 500 g unload was performed during
`resource_capture_final`. The recorder did not receive a separate human event
line, so the physical edge is treated as data-inferred and the user statement
as retrospective confirmation, not as an exact timestamp.

The continuous CSV contains an unambiguous one-sample edge:

- UTC bracket: `2026-10-11T06:55:17.248457Z` to
  `2026-10-11T06:55:17.338676Z`;
- device sequence: `846 -> 847`;
- authoritative gross: `455.679279 g -> 0.387464 g`;
- full observed step: `-455.291815 g`.

The path counters around the edge show STEP_PENDING increasing from 50 to 51.
The final cumulative counts were:

```text
OFF=500, HOLDOFF=251, REFERENCE_FILL=494, OBSERVATION_FILL=199,
TRACKING=180, FAST_TRACKING=0, STEP_PENDING=65, STEP_SETTLING=300,
DOSING=0, INVALID=0
```

The resulting coverage mask is `0x0DF`: TRACKING, STEP_PENDING and
STEP_SETTLING are now evidenced. FAST_TRACKING and DOSING remain uncovered,
so the nine-path resource qualification remains `INCOMPLETE`. No 0x0524
image was installed and the device was restored to `0x0520 OFF+SHADOW`.
