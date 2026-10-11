# Stage 5P Unified-R5 peak errata and path-directed closure

Baseline: `13bad7212f3263d4585c40ae904e159a276a086d`

## Peak errata

R4's original peak summary used an intermediate polling frame. Recomputed
over all 2,715 rows and the final cumulative snapshot:

- Feed: `672128` cycles = `9.335 ms` at 72 MHz;
- App_Run: `1027997` cycles = `14.278 ms`;
- loop interval: `1028049` cycles = `14.278 ms`.

The Feed peak is path index 2 (`REFERENCE_FILL`), sequence 1347, epoch
`351272961`. All values remain below the frozen limits, but Feed has only
approximately 0.665 ms headroom. The original R4 report is preserved.

## Mode-semantics correction

The A13C enum is `OFF=0`, `DOSING=1`, `STATIC=2`. Earlier host captures sent
`mode=1` while labelling it STATIC, so they actually exercised DOSING. This
R5 host correction is recorded without changing MCU code or the 0x0524 image.

## Directed captures

The first corrected STATIC run started while the platform already held 500 g.
It reached STATIC/TRACKING and covered the static fill paths, but no physical
step event was marked. Coverage ended at `0x1F`; no STEP_PENDING/SETTLING
qualification was claimed.

The second run used the same 500 g-loaded baseline and was intended to capture
an unload step. The recorder ended naturally before a user unload event was
received. It nevertheless recorded `STEP_PENDING` and ended with coverage
mask `0x05F`; `STEP_SETTLING` and DOSING remained uncovered. This is useful
development evidence only, not a complete nine-path gate.

Both runs had valid diagnostic flags, no fault/read error/overrun, and physical
produced/consumed/FIFO conservation. Both automatically restored the exact
pre-test 0x0520 application and V3 configuration. No 0x0524 image was burned.

## Current conclusion

Corrected peak scoring: PASS against timing limits, with narrow Feed margin.
Three-path qualification: INCOMPLETE. Complete `0x1FF` path coverage was not
achieved, so the 0x0524 focused delivery, SAVE, power-cycle, UI and battery
tests remain NOT RUN. A13B long-duration efficacy remains INCONCLUSIVE.
