# Stage 5P-A13E-R3 safety contract closure

Local date: 2026-09-29 (Asia/Shanghai); captures have 2026-09-28 UTC labels.
Baseline `befb8835144a1d3b4b101d7c12f960c5f81c10be`, branch
`stage5pa13er3-safety-contract-closure`.

## Result

**FINAL: H1 INCOMPLETE; H2 default-OFF behavior observed PASS;
H3 reciprocal exclusion behavior PASS; final dirty cleanup PASS after user
physical power-cycle. Overall INCOMPLETE, NOT a three-gate safety-closure PASS.**
The scope remains current sensor, 10 Hz/filt1/strength3,
volatile/default-OFF engineering use. A13B efficacy is still INCONCLUSIVE;
formal metrology, 40 Hz, other filters and cross-sensor qualification remain open.

## H1 same-sample relation

The public 0x0340–0x0367 A13E block was audited: one atomic read contains
generation, application, sequence/time, applied offset, uncompensated gross,
formal gross/net, tare, mode/state and offset. The corrected host decoder uses
signed 32-bit offset fields and signed 64-bit weight pairs.

`Results/stage5pa13er3/h1_final/samples.jsonl` contains 1,698 atomic samples
across OFF, ACTIVE+STATIC and ACTIVE+DOSING. Every sample satisfies:

```text
formal_gross = uncompensated_gross - offset
formal_net   = formal_gross - tare
```

with **0 parsed-value mismatches**. ACTIVE+DOSING contains 283 samples and one constant
offset value, **45 µg**, proving freeze for this low natural-offset run. The
earlier parser failure (incorrectly treating 32-bit offsets as 64-bit) is
retained in the worktree evidence and was not used for qualification.
Host polling had duplicate/skip artifacts (87 duplicate adjacent sequence
observations and 3 forward gaps), so this is an atomic-value relation PASS,
not a continuous throughput qualification. No panel quantization was used.
These early records lack archived original Modbus responses. A separate
`raw_recovery/` capture saves requests/responses and parsed values, validates
signed offsets and sequence-brackets the additional candidate/R5/safety blocks.
It has 1,134 polls (50 same-sequence multiblock matches), but zero nonzero-offset
samples. Therefore the requested raw-response-complete nonzero STATIC/DOSING
gate remains **INCOMPLETE**; the prior parsed results cannot replace missing raw
frames or be labelled a complete evidence PASS.

An early `h1_dosing/samples.jsonl` was inadvertently truncated by reopening the
same output directory during stop. It is empty and invalid, and cannot be
recovered from console excerpts. The subsequent recorder rejects reuse of
existing output files; the corrected and final datasets remain separate.
The apparent OK-without-mode-change command reused token2100. Code in
ModbusCommandMailbox Execute returns an existing response without executing
when request_token equals response_token. Token3000 independently switched
mode correctly, but that console-only result is not a raw archived transaction.
Raw recovery chooses a token different from the current response and verifies
post-state. No firmware fault is inferred from the stale-response episode.

## H2 physical power-cycle

Before the cycle the device had physically entered ACTIVE+DOSING with no SAVE.
The user performed a real power removal and re-application. First recorded post-power
readback (`Results/stage5pa13er3/h2_post_power_cycle.json`):

- Firmware `0x0520`, Map `0x0109`
- R5 application=SHADOW, mode=OFF, offset=0
- Checkweigh=OFF
- revision/saved=19/19, dirty=0, fault=0, overrun=0
- calibration valid, 10 Hz/filt1/strength3, V3 format3

No ACTIVE restoration was observed. This is a physical power-cycle default-OFF
behavior PASS, not a SWD/software-reset substitute. Readback was at uptime
29,961 ms, not at the first sample after boot; the first 30 seconds and explicit
reference readback were not recorded. The JSON captures are parsed-only, not
archived raw response bytes. Those limitations are not hidden.

## H3 Checkweigh mutual exclusion

Initial parsed-only evidence: `h3_active_checkweigh.json`, `h3_checkweigh_conflict.json`.

1. With ACTIVE+STATIC, requesting Checkweigh STATIC returned
   `INVALID_STATE`; ACTIVE remained unchanged and Checkweigh stayed OFF.
2. With Checkweigh STATIC enabled while A13 was OFF, requesting ACTIVE+STATIC
   returned `INVALID_STATE`; Checkweigh remained enabled and ACTIVE did not enter.
3. Checkweigh was then returned OFF without SAVE. The RAM request temporarily
   made dirty=1/revision=21; a controlled physical power cycle restored the
   saved configuration. Final readback is dirty=0/revision/saved=19/19.

The initial records do not rule out all alternative admission reasons or retain
all before/after output snapshots. A NEW `raw_recovery/events.jsonl` and
`frames.jsonl` therefore provide complete prospective H3 request/response and
parsed pre/post conditions, independently of the initial snapshots:
ACTIVE rejected both STATIC and DYNAMIC Checkweigh requests (tokens5003/5004);
then Checkweigh STATIC was enabled (5006), ACTIVE rejected (5007) with fresh
generation, zero offset, proper Profile, valid calibration/weight and no fault,
overload, near-rail or LIMITED. Candidate state, formal output flags and dirty/
revision are retained. No SAVE, ZERO, TARE or calibration command was issued.
Cleanup Checkweigh OFF succeeded (5008); terminal RAM dirty=1, revision21/saved19
was then restored by the user-confirmed final physical power cycle. The final
raw-response terminal capture proves dirty=0, revision/saved19/19; no SAVE.
No alarm-output matrix was repeated. GPIO/output diagnostic flags are evidence;
physical lamp/buzzer observations from a user are NOT RUN.

## Final device and artifacts

Earlier direct readback (`final_probe.json`), BEFORE raw recovery: `0x0520 / Map0x0109`, OFF+SHADOW,
offset=0, Checkweigh OFF, dirty/fault/overrun=0, revision/saved=19/19.
The existing R2 resource PASS (`511/511`) and software evidence remain valid
and were not rewritten. No SAVE was issued. The 0x0520 image and configuration
identity remain those from R2; no firmware source or algorithm was modified.

After the FINAL user-confirmed physical cleanup power cycle, read-only
`terminal_after_cleanup_power/terminal.json` and `frames.jsonl` record
2026-09-28T16:53:10.589223Z (2026-09-29 00:53:10.589223 Asia/Shanghai):
0x0520/Map0x0109, OFF+SHADOW, offset=0, applied offset=0, apply=0,
reference=0, Checkweigh OFF, dirty/fault/overrun/SAVE=0, revision/saved19/19.
Calibration valid, raw zero41868/raw span485780/span mass500000000 ug,
10Hz/filt1/strength3, V3 format3, active A sequence19 are unchanged by readback.
No reader shares COM5, no recorder remains running and no ACTIVE remains enabled.
The 500g load was still present on final readback; do not mistake that for empty.

Firmware BIN identity is reused from R2 (not a fresh SWD checksum in R3):
94,620B SHA256 `0CED588DF5C0C1AE1A3635A376002EE8F9479DCC579F5F96FF9607894C8DC4F6`.
Historical configuration SHA is
`856BD8F5C14760561FC4BFEC4274FE0C5480C19BC4422046617C2450F8439733`.
R3 Flash/config byte hash measurement is **NOT RUN**: no SWD, no flashing,
no new binary, no persistent format/calibration/filter/algorithm edits. Saved
revision, active slot, config/endpoint reads and zero SAVE count support
unchanged saved state but are not a freshly measured Flash SHA.

Reproducible, read-only offline review:

```powershell
python -B -m unittest discover -s Tools -p test_r3_raw_closure.py -v
python -B Tools/r3_review.py --output D:/Downloads/r3_review.json
python -B Tools/r3_evidence.py
```

`qualification.json` verifies every archived RTU TX/RX CRC, response lengths,
atomic arithmetic, fresh command tokens, bidirectional rejection pre/post
generation/output/dirty state, exact signed units and safe terminal. All 1,134
raw-complete polls had zero offset; 50 also match all sequence brackets across
the R5/candidate blocks. Do not interpolate samples or fabricate nonzero evidence.
The three focused decoder tests pass. Manifest hashes every formal result/report
and host script; original parsed failures and the empty truncated file remain
explicitly present. Initial console-only snapshots have no retroactive raw archive.

Branch `stage5pa13er3-safety-contract-closure` is pushed independently; the final
full commit and remote ahead/behind check are delivered in the handoff (the
report belongs to that commit, not a self-referential embedded hash).
Only host evidence scripts/report/data changed; no `.pyc`, user snapshots or
unrelated worktrees were cleaned, restored or committed.

Minimal remaining evidence: a NEW raw-response-complete, correctly decoded
nonzero-offset ACTIVE+STATIC and ACTIVE+DOSING window. The product already
exposes the necessary fields; limitation is host archival/natural stimulus,
not absent public registers. No new parameters, artificial offset, 12-hour test
or repeated resource matrix is justified. This turn does not start another run.

This is an evidence-status report, NOT a completed engineering safety closure.
Long-duration drift efficacy remains `INCONCLUSIVE`; historical A13D remains
FAIL. No merge, tag or PR.
