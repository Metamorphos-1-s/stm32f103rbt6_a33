# 0x0529 zero-indicator read-only evidence

Date: 2026-10-13 Asia/Shanghai

## Scope

No key, mode, profile, R5, checkweigh, ZERO/TARE, SAVE, reset, SWD or
configuration operation was performed. The device remained on the 0x0529
image. The earlier user report that the lamp remained off and the panel moved
between -0.02 and -0.01 g was recorded as a visual event, but no visual event
was inferred from register values.

## Identity and preflight

The first read-only snapshot at `2026-10-13T15:09:56.754986Z` reported:

```text
Firmware/Map: 0x0529 / 0x010F
Profile: 40 Hz / filt1 / strength3
R5: ACTIVE + STATIC, state TRACKING, offset -30064 ug
Checkweigh: OFF
fault/overrun/dirty: 0/0/0
revision/saved: 22/22
```

This is current-device evidence. No configuration was modified by this
investigation.

## Capture

The non-blocking recorder ran for approximately 4.6 minutes at a nominal
1-second host interval and produced 278 samples with zero read errors. Each
sample retained raw FC03 frames for realtime/diagnostic, display-condition,
R5 and configuration blocks, along with UTC, host monotonic time, MCU uptime
and sample sequence. The recorder used only small reads; it did not request a
large holding-register block.

Observed ranges:

| Field | Result |
| --- | ---: |
| official stable | 278/278 true |
| public zero status bit | 0/278 set |
| authoritative net | -44,260 to +20,073 ug (-0.04426 to +0.02007 g) |
| display count | -3 to 0; first/last -1 |
| conditioned display | -30,000 to 0 ug |
| display anchor | -30,000 to 0 ug |
| display condition | LOCKED for all samples |
| display release reason | 9 (`DISPLAY_RELEASE_SOURCE_CHANGE`) in all samples |
| fault/overrun | 0/0 throughout |
| sample sequence | 57,292 to 69,245; no MCU sequence gaps in the captured records |

The host poll interval was approximately 1.1 seconds, so adjacent records
are not adjacent 40-Hz device samples. The largest observed sequence jump was
44. This is a host observation gap, not evidence of device sample loss.

## Source contract

The 0x0529 source computes the zero status from authoritative `net_mass_ug`,
not from `display_count` or the conditioned display. It requires
`STABILITY_STATE_STABLE` and a magnitude no greater than one display division
(the configured g display maps this to approximately 10,000 ug). The physical
zero LED mirrors `WEIGHT_STATUS_ZERO` in `DisplayController_BuildIndicators`.
The public map exposes the status flags, but does not expose the LED driver
output or the internal candidate/confirmation timer. The display-condition
registers expose the locked/anchor/release state, not the zero confirmation
timer.

The patched source also introduces a one-second zero-indicator candidate
confirmation and hysteresis. That timer/state is not directly observable in
the public registers. At 40 Hz, the host data cannot prove its sample-by-sample
continuity because polls are about 1.1 seconds apart.

Offline classification of the captured values against the approximately
±10,000 ug entry range found 100/278 records inside. The longest *observed*
consecutive run was 8 records, but the records span host polling gaps; this is
not a proof that the underlying device stream failed or passed a one-second
confirmation. The evidence therefore supports that the status bit did not
re-enter during the capture, while the exact internal rejection reason remains
unobservable.

## Conclusion

Supported facts:

1. official stable was continuously true at every recorded poll;
2. the public zero status bit never re-set;
3. authoritative net repeatedly approached or entered the nominal ±0.010 g
   range, but the host did not observe a continuous one-second window;
4. the panel remained locked around -0.01 g, so panel quantization/hold value
   is not a valid substitute for authoritative net;
5. no profile/R5/fault/overrun/reset change occurred.

The most defensible classification is **EVIDENCE INSUFFICIENT TO DISTINGUISH**
between failure to satisfy continuous zero-entry confirmation and an internal
zero-indicator state/timer issue. The public map cannot directly discriminate
those cases. No firmware or configuration fix was implemented in this turn.

The minimum next evidence would be a target-side diagnostic counter/timestamp
for zero-entry candidate and confirmation state, or a recorder that captures
adjacent 40-Hz samples without gaps. Do not change thresholds or clear the
current offset based on this capture.
