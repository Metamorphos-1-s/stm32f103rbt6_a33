# Stage 5P Unified-R7 focused delivery result

Baseline: `d74873da54cc4291bbc547327e2ce768ed9955aa`

## Software and image gates

- Candidate: `0x0524 / 0x010C`;
- BIN: 96,620 B;
- SHA-256: `CBAB6D5E294D36F62231F9A233DF9E915E9F599D4B4826E6DC993A95035FFBF1`;
- Host Debug/Release regression: 47/47 each;
- fresh ARM Release strict build: PASS, generated the exact candidate SHA;
- fresh ARM Debug attempt: FAIL at `-O0` link (FLASH overflow 51,080 B),
  retained as a build-configuration failure; frozen Debug resource evidence
  from prior stages remains unchanged;
- cross-record engineering resource evidence: PASS;
- single-epoch resource qualification: INCOMPLETE, unchanged.

## Preflight, flash and rollback

Preflight found the device at `0x0520/0x0109`, OFF+SHADOW, 10 Hz/filt1/strength3,
valid calibration, revision/saved 19/19, dirty/fault/overrun 0, offset 0 and
approximately 0.20 g. Fresh application and V3 backups were made:

- application SHA: `F85404B5D551EB44CB9B585793CA1935D6D06D7DC4608B9F986956B9FAA84A6B`;
- configuration SHA: `856BD8F5C14760561FC4BFEC4274FE0C5480C19BC4422046617C2450F8439733`.

The exact 0x0524 candidate was installed app-only and Verify succeeded. Boot
readback was `0x0524/0x010C`, with the configuration SHA unchanged and
OFF+SHADOW offset 0.

## Focused hardware result

A short core recorder entered ACTIVE+STATIC correctly (`mode=2`) and recorded
safe idle/static behavior. No 500 g placement confirmation arrived during the
recording window, so no valid 500 g load/unload span, DOSING freeze, or same-
sample compensated-weight relationship was established. The recorder ended
normally; no fault, overrun or SAVE occurred. The 0x0524 candidate was not
qualified as a delivery image.

The candidate was replaced from the fresh backup. Final readback is
`0x0520/0x0109`, OFF+SHADOW, offset 0, revision/saved 19/19, dirty/fault/
overrun 0, with application and configuration bytes equal to the backup.

## Gate table

| Gate | Result |
| --- | --- |
| Candidate identity and SHA | PASS |
| Host regression | PASS |
| ARM Release strict build | PASS |
| ARM Debug fresh build | FAIL/CONFIGURATION LIMIT; historical gate retained |
| Cross-record nine-path engineering evidence | PASS |
| Single-epoch resource qualification | INCOMPLETE |
| 0x0524 boot/config preservation | PASS |
| 10/40 Hz focused functional evidence | INCOMPLETE |
| 500 g load/unload and DOSING freeze | NOT RUN |
| Menu/PLC SAVE and power-cycle migration | NOT RUN |
| Battery/low-voltage/idle-dimming | NOT RUN |
| Final rollback | PASS |
| A13B long-duration efficacy | INCONCLUSIVE |

No formal Release, long-duration, cross-sensor or metrology qualification is
claimed. The original `.pyc` changes were not staged or modified.
