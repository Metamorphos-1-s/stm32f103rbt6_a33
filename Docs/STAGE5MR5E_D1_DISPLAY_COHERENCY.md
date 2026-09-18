# Stage 5M-R5E-D1 Display Coherency

## Conclusion

**STAGE 5M-R5E-D1 DISPLAY HOLD ISSUE CONFIRMED; NO ACCEPTABLE LOW-RAM
DISPLAY CANDIDATE; FIRMWARE REMAINS 0x0512; STAGE 5N ENTRY DEFERRED.**

D1-A completed the controlled diagnosis without changing the R5 algorithm,
parameters, calibration, configuration or firmware path. Three ACTIVE+DOSING
load/unload cycles passed the mechanical/step repeatability gate. The known
panel behavior is consistent with a stale `DisplayConditioner` anchor masking an
authoritative compensated value, but no D1-B display candidate was implemented
or flashed. A future display fix requires its own offline replay and a new
Beta; D1 does not silently change the display release policy.

## Baseline and snapshot

Start commit: `dfca872a58a460d5bf2c51dc05b106e88c4da29e` on
`stage5mr5e-d1-display-coherency`. Firmware remained `0x0512`, Map `0x0104`,
Schema 2 and Persistent Format 3. The task-start configuration SHA was
`A615A475C2D7B5D64126EAEB3AAE9E3D094348FA4C4AEC7BDB8EF0010D4A3BB4`, reflecting
the user's legal calibration and revision `8/8`.

The authorized read-only SWD configuration dump reset the MCU but did not erase
or write Flash. After reset the device returned OFF+SHADOW, offset/reference/
evaluation zero, fault/overrun/dirty/SAVE zero and revision/saved `8/8`.

## Display root cause evidence

The audited path is:

```text
official filtered/calibrated uncompensated gross
 -> R5 corrected gross
 -> WeightEngine authoritative gross/net
 -> DisplayConditioner anchor/lock
 -> 0.01 g quantization -> panel
```

The display conditioner uses `8 * display_division_ug`, i.e. `0.080 g`, as its
release threshold. It retains the old anchor while the official source is
unstable. A captured incident showed authoritative compensated gross near
`500.015929 g` while the display lock anchor was `500.068067 g`; the difference
was `0.052138 g`, below the `0.080 g` release threshold, so the panel remained
`500.07 g`. This is display hold behavior, not evidence that
`corrected != uncompensated - offset`.

The current D1 controlled test deliberately used DOSING after reset, so offset
was zero and no automatic R5 rebase could occur. It proves step/repeatability
and DOSING safety; it does not claim to re-qualify nonzero-R5 residual efficacy.

## Controlled D1-A results

All three runs were 720.0 s with 720 records, zero read errors, reconnects and
host gaps. Offset was exactly `0 ug` in every sample; automatic rebase count was
unchanged at zero; correction rate, fault, overrun, dirty and SAVE were zero;
revision/saved revision stayed `8/8`.

| Run | Loaded uncomp/corrected | Empty uncomp/corrected | Span |
|---|---:|---:|---:|
| 1 | 499.864840 / 499.864840 g | -0.158813 / -0.158813 g | 500.023653 g |
| 2 | 499.860335 / 499.860335 g | -0.159939 / -0.159939 g | 500.020274 g |
| 3 | 499.846819 / 499.846819 g | -0.161066 / -0.161066 g | 500.007885 g |

Maximum span difference is `0.015768 g`, below the `0.020 g` engineering gate.
Loaded repeatability is `0.018021 g`; empty repeatability is `0.002253 g`.
For every run, corrected and uncompensated step amplitudes are identical, so
DOSING loss is `0 ug`.

## Final device state

The device remains intentionally in the user-tested `ACTIVE + DOSING` state;
Codex did not switch OFF because that would clear volatile offset and the task
requires confirmation before doing so. Final captured state: Firmware `0x0512`,
offset `0 ug`, reference `0`, evaluation `0`, automatic rebase `0`, fault/
overrun/dirty/SAVE `0`, revision/saved `8/8`. Configuration SHA before/after
the read-only snapshot is identical to
`A615A475C2D7B5D64126EAEB3AAE9E3D094348FA4C4AEC7BDB8EF0010D4A3BB4`.
No post-test SWD configuration read was performed because it would reset this
active DOSING state without explicit user authorization; the test-start SHA is
therefore the preserved configuration baseline, not a claimed post-test
readback.

No 0x0513 was created, no firmware was flashed, and no display fix was
integrated. R5 algorithm blobs, R5D/R5E evidence and the standard Release are
unchanged. An explicit later decision is required for a low-RAM display-hold
candidate or an `EMPTY_CONFIRMED` workflow; D1 does not implement automatic
zeroing or Stage 5N-A.
