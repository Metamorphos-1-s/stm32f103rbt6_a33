# Stage 5P Unified-R3 resource epoch closure

Date: 2026-10-11
Baseline: `1bad2982128f9f88cf1d4482a59c1e811d007c17`

## Diagnostic correction

R2's 639-count mismatch was not treated as a harmless coincidence. R3 added a
post-profile-switch measurement epoch. The profile completion hook records the
previous diagnostic flags in the epoch value, preserves the stack watermark,
and starts new Feed/path counters. The Checker compares produced, consumed,
engine and FIFO as epoch deltas, so legal 40 Hz four-to-one compensation
admission is not mistaken for physical sample loss.

The host contract now explicitly distinguishes expected profile dirty state
from arbitrary dirty/revision changes, and has a separate explicit opt-in for
ACTIVE resource capture. Host contract tests: 11/11.

## R3 diagnostic software gate

New diagnostic identity: **0x0526 / Map 0x010E**.

- 98,412 B, SHA-256:
  `A482E1912CB34725553FD04A0E2EAA07BF7709E21D465D75DB579565A7C7001C`
- Release C objects use `-Os`; startup assembly retains `-O3 -DNDEBUG`.
- RAM: 19,384 B.
- Complete conservative stack: 1,448 B.
- Static collision margin: 672 B.
- Strict ARM build and stack analysis: PASS.

The 0x0524 delivery BIN was not changed or rebuilt; its required SHA remains
`CBAB6D5E294D36F62231F9A233DF9E915E9F599D4B4826E6DC993A95035FFBF1`.

## Device evidence

Fresh preflight and backup were taken from `0x0520/0x0109`, OFF+SHADOW,
10 Hz/filt1/strength3, revision/saved 19/19, dirty/fault/overrun zero. The
application backup SHA was
`F85404B5D551EB44CB9B585793CA1935D6D06D7DC4608B9F986956B9FAA84A6B`; the V3
configuration SHA was
`856BD8F5C14760561FC4BFEC4274FE0C5480C19BC4422046617C2450F8439733`.

The diagnostic image was installed app-only and profile 1 was selected using
the already proven mailbox command 7/`arg0=1`. The new epoch began after
40 Hz completion. A short OFF run showed flags=1, stack watermark 1,240 B,
produced=consumed, FIFO=0 and no fault/overrun. Coverage was incomplete until
mode requests were attempted.

When ACTIVE+STATIC was requested, the raw resource snapshot showed coverage
mask 257 (OFF plus STATIC), but the device revision became **21** while saved
revision remained 19. The frozen contract permits only the one profile change
19->20; revision 21 is an unexpected extra RAM configuration mutation. The
Checker therefore stopped with `unexpected profile revision` before accepting
ACTIVE/DOSING resource qualification. This is a real contract failure, not a
host gap or a relaxed diagnostic rule.

The run was automatically restored. Final readback:

- `0x0520 / 0x0109`, OFF+SHADOW, offset 0;
- revision/saved 19/19, dirty/fault/overrun 0/0/0;
- application and configuration matched the fresh backup byte-for-byte.

## Gate results

| Gate | Result |
| --- | --- |
| R2 epoch interpretation and host tests | PASS |
| R3 diagnostic Release image/resource build | PASS |
| 40 Hz profile completion | PASS in prior R2 evidence and re-established in R3 |
| R3 OFF epoch safety/counter check | PASS for captured interval |
| STATIC/DOSING complete resource qualification | FAIL/INCOMPLETE: unexpected revision 21 |
| Accurate 0x0524 flash/Verify | NOT RUN |
| 0x0524 functional, SAVE, power-cycle and UI gates | NOT RUN |
| Exact rollback | PASS |
| A13B long-duration efficacy | INCONCLUSIVE, unchanged |

R3 must not be called a resource-qualified delivery. The next fix must explain
and prevent the extra revision caused by the ACTIVE pair transition, or update
the product contract with an explicitly justified transaction that does not
rewrite persistent configuration. No 0x0524 installation is authorized from
this run.
