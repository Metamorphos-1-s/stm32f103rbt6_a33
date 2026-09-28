# Stage 5P-A13E-R2 controlled ACTIVE engineering closure

Date: 2026-09-28. Baseline `10f43c320b4a4d886aa6e6171f70eb9b0bb0a8b2`, branch
`stage5pa13er2-controlled-active`. Previous A13E-R failure and INCOMPLETE
records remain under the prior `Results/stage5pa13er/` tree and were not changed.

## Qualification result

**A13 GUARDED ACTIVE ENGINEERING BETA: FUNCTIONAL SHORT-RUN PASS; RESOURCE
DIAGNOSTIC PASS; POWER-CYCLE AND LONG-DURATION EFFICACY DEFERRED.**

This is limited to the current sensor, 10 Hz, filt1/strength3, default OFF,
volatile ACTIVE operation. It is not a formal metrology, cross-sensor, 40 Hz,
other-filter, slow-feed or A13B efficacy qualification.

## Software and resource gates

The A13E-R metadata-only fix and final software evidence are reused without
changing the A13C math. A13C-R 10,774, A13B 88,765, A9 107,491 and six synthetic
replays remain 0-mismatch; Host 46/46 Debug and Release remain PASS. Ordinary
0x051D remains SHA `DDF57FB131E5EF675D94B08A5A8752EB0FB588BA5230DEA6F34C57D6B2F62DDD`.

The new 0x0521 resource run used a fresh backup and correct zero-offset ordering:
OFF+SHADOW was requested and read back offset=0, then the latest generation was
used for ACTIVE+STATIC. Nine paths all covered (mask511), 8,103 target calls
with produced=consumed=engine=timed, FIFO0, invalid0. Six host poll gaps were
retained without interpolation. HCLK72 MHz maxima:

| Path | Calls | Max cycles | Max ms |
|---|---:|---:|---:|
| OFF | 2,335 | 408 | 0.00567 |
| HOLDOFF | 258 | 132,173 | 1.83574 |
| REFERENCE_FILL | 676 | 608,435 | 8.45049 |
| OBSERVATION_FILL | 597 | 70,470 | 0.97875 |
| TRACKING | 1,690 | 430,538 | 5.97969 |
| FAST_TRACKING | 1,117 | 432,637 | 6.00885 |
| STEP_PENDING | 15 | 98,199 | 1.36388 |
| STEP_SETTLING | 42 | 123,000 | 1.70833 |
| DOSING | 1,373 | 120,255 | 1.67021 |

App_Run max 13.86475 ms, loop interval max 13.86547 ms, runtime untouched RAM
1,364 B; fault/overrun/read errors/dirty/SAVE/revision all remained safe.
The diagnostic measurements are not exact uninstrumented 0x0520 timing.

## 0x0520 hardware functional run

Fresh 0x051D preflight and backup preceded the resource install. Resource image
0x0521 booted OFF+SHADOW and was fully restored before 0x0520 installation.
The uninstrumented image was 94,620 B, SHA
`0CED588DF5C0C1AE1A3635A376002EE8F9479DCC579F5F96FF9607894C8DC4F6`;
full 126,976-B application readback SHA after install was
`F85404B5D551EB44CB9B585793CA1935D6D06D7DC4608B9F986956B9FAA84A6B`,
with the exact 0x0520 prefix and zero non-FF tail. Config SHA stayed
`856BD8F5C14760561FC4BFEC4274FE0C5480C19BC4422046617C2450F8439733`.

The corrected streaming recorder captured 2,890 samples. It recorded:

- boot OFF+SHADOW and zero-offset ACTIVE+STATIC transaction, application/mode/generation consistent;
- a 500 g load in ACTIVE+STATIC, with applied offset rising from zero to a nonzero value;
- ACTIVE+DOSING transaction and 673 samples with offset exactly constant at 18,025 µg;
- 500 g unload while DOSING, return to ACTIVE+STATIC and post-unload samples;
- no fault, overrun, dirty or revision change.

The direct pair evidence is in `active_validation_final/events.jsonl` and the
逐样本 data in `active_validation_final/samples.jsonl`. A separate direct OFF
transaction after capture read back application=0, mode=OFF, offset=0, apply=0,
generation8, revision/saved19/19 and safe counters (`off_direct.json`).
The applied offset was deliberately not claimed as a metrology result; the
active register stream does provide formal gross and applied offset, while the
uncompensated active metric was not included in the first recorder schema.
Therefore exact same-sample `formal gross = uncompensated - offset` is **PARTIAL
DIRECT EVIDENCE**, not a full PASS assertion. Formal gross/net and raw/filter
fields remained continuously readable.

## Safety and deferred items

PASS: zero-offset entry protection, explicit generation transaction, volatile
ACTIVE/STATIC and ACTIVE/DOSING, DOSING offset freeze, OFF immediate recovery,
configuration immutability, resource limits and exact application/config backup
recovery. The first R2 recorder ordering defect and all original failed records
are retained; no failed evidence was rewritten.

NOT RUN: physical power-cycle default-OFF, full same-sample uncompensated-vs-
formal equation audit, formal Checkweigh ACTIVE conflict on 0x0520, long-duration
efficacy, 40 Hz/other filters/cross-sensor/slow feed. A13B remains
`EFFICACY INCONCLUSIVE`; historical A13D remains FAIL.

Final device state at handoff: **0x0520 / Map0x0109, OFF+SHADOW, offset=0**;
no SAVE, dirty=0, fault=0, overrun=0, revision/saved19/19. No automatic ACTIVE
or persistence was enabled. This engineering Beta remains user-controlled and
limited to the stated profile.
