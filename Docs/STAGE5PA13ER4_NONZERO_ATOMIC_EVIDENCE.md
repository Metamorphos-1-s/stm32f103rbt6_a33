# Stage 5P-A13E-R4 nonzero-offset atomic response supplement

Date: 2026-09-29 Asia/Shanghai (capture UTC labels are 2026-09-28).
Baseline `9b2e4baa17fb698ff3f3b3239690bb1902665e51`.

## H1 result

**PASS for the requested nonzero-offset atomic H1 evidence.** This supplement
does not change R3's H2/H3 results, R2's resource `511/511 PASS`, A13B's
`INCONCLUSIVE` efficacy result, or any firmware/algorithm/parameter/calibration.

The unique evidence directory is:
`Results/stage5pa13er4/20260928T173629Z_nonzero_atomic/`.
The recorder opened `frames.jsonl` before the first preflight read, refused
existing output directories, wrote every Modbus TX/RX frame before parsing, and
closed files in normal completion. It used fresh mailbox tokens and checked
response token/command ID/generation/post-state. The previous R3 stale-token
episode was not reused.

### Direct raw evidence

- Raw RTU frames: **3,536**; FC03=3,530, FC16=3, FC06=3.
- CRC and response-length errors: **0**.
- Atomic 0x0340–0x0367 reads: **3,282**.
- Nonzero-offset atomic reads: **240**.
- STATIC nonzero samples: **240**.
- DOSING samples: **198**.
- DOSING offset values: exactly `-100 µg` for all 198 samples.
- Atomic gross equation mismatches: **0**.
- Atomic net/tare equation mismatches: **0**.
- Candidate/R5/authority cross-block comparison is only accepted when the
  sequence brackets match; the single-block 0x0340 arithmetic is authoritative.
- Device sequence observations include duplicate host polls and three forward
  gaps; no interpolation or fabricated sequence was used. Each raw atomic
  response remains independently valid.

For every atomic response the decoder proves:

```text
formal_gross_ug = uncompensated_gross_ug - applied_offset_ug   (when apply=1)
formal_net_ug   = formal_gross_ug - tare_ug
```

The recorder uses signed 32-bit offset fields and signed 64-bit weight pairs;
decoder unit tests cover negative offsets, CRC corruption, wrong block length,
new token generation, duplicate/gap accounting and numerical mismatch rejection.

### Sequence and timeline

Preflight was 0x0520/Map0x0109, 10 Hz/filt1/strength3, Checkweigh OFF,
OFF+SHADOW/offset0, calibration valid, revision/saved19/19, dirty/fault/
overrun/SAVE0. A fresh token entered ACTIVE+STATIC with generation 2→3.
Natural STATIC tracking then produced nonzero offset samples. The same raw
recorder switched to ACTIVE+DOSING using generation 3; post-state generation4,
mode DOSING and offset `-100 µg` were recorded. 198 DOSING atomic samples all
froze that value. The recorder switched OFF+SHADOW using a fresh token and
captured terminal application=0, mode=OFF, apply=0, offset=0, reference=0.

No user load/unload was required in R4; the existing physical load state was
left untouched. No ZERO, TARE, calibration, SAVE, SWD, flash or Checkweigh
operation occurred. The natural offset was small; this is a correctness and
atomicity supplement, not efficacy qualification.

## Final terminal

The capture terminal was 0x0520/Map0x0109, OFF+SHADOW, offset/reference zero,
Checkweigh OFF, dirty/fault/overrun/SAVE0, revision/saved19/19, calibration
valid, 10 Hz/filt1/strength3. R3's final power-cycle/config evidence remains
unchanged. The current sensor and physical load should be treated as present
or absent only according to this final terminal readback; no assumption is made
about the scale platform after capture.

## Qualification boundary

H1 is now complete for same-sample raw arithmetic and nonzero DOSING freeze.
This does not add long-duration drift efficacy, 40 Hz, other filters,
cross-sensor, formal metrology or A13B qualification. The combined statement
may now say the current sensor/10 Hz/filt1/strength3 default-OFF ACTIVE safety
contracts are closed for H1/H2/H3 plus R2 resource evidence, while those
broader items remain deferred.
