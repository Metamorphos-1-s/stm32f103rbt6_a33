# Stage 5P-A2D Focused Hardware Validation

## Status

The RAM-fix candidate passed the software gates and completed the focused
calibration/menu board session. At the user's direction, firmware 0x051C was
retained and the temporary brightness 4 was restored to 3. Calibration Flash
persistence and direct-long menu SAVE behavior passed. The calibration `SAUE`
panel text was not visually confirmed, and the 10 ms `APPLY` page was not
captured; these display observations remain limited rather than fabricated.
This is an engineering candidate, not a formal release or a new metrology
qualification.

The original two-commit A2D patch failed the 19,464 B Debug RAM ceiling by
8 B and was never flashed. Its failure evidence remains in
`Docs/STAGE5PA2D_SOFTWARE_GATE_RESULT.md`. The follow-up patch, SHA-256
`04C9F37DD5364B9C520E2CC81E27E2434E8321C40BD982512811EFACC2DDD1FB`,
was applied as `4ba54f3` without rewriting that history.

## Software And Image

| Gate | Result |
|---|---|
| Host Debug / Release CTest | 41/41 PASS each |
| ARM Debug / Release / strict warning builds | PASS |
| Debug RAM | 19,464 B, at ceiling |
| Conservative stack / collision margin | 1,504 B / 536 B (floor 512 B) |
| Linker stack / heap | 1,024 B / 0 B |
| 0x051C Debug and strict BIN | 108,760 B; `6D88021729A47B8317919D3DF0A14D36228E5AB161CFC338BD7D2D381A99ADE3` |
| 0x051C Release BIN | 92,428 B; `F02458F073766D3A9E2C7F278DFFDF258277067D653D05593853EF4BF966D114` |

The exact Debug BIN above was programmed over SWD and verified. Only application
sectors 0-106 were erased. Map remains 0x0104, Schema 2, Persistent Format 3.

## Preflight And Backup

The first two read-only COM5 probes found 0x0517 with dirty=1,
revision/saved=10/8, and R5 LIMITED reason SEQUENCE. The user explicitly chose
to discard those unsaved RAM changes. Before reset, the complete 124 KiB
application region, the 4096-byte V3 configuration region and the active
Modbus configuration were backed up. A software reset then reloaded the saved
configuration: clean 8/8, R5 OFF+SHADOW, limited/fault/overrun/SAVE=0.

The saved configuration SHA-256 before flash was
`A615A475C2D7B5D64126EAEB3AAE9E3D094348FA4C4AEC7BDB8EF0010D4A3BB4`.
The full 124 KiB application backup SHA-256 was
`0BFEC08EF3E3E2BF8D00209E95F3BF0203C30134EDCF75BF19E83FB4B0C28297`;
its 107,084-byte prefix matches the frozen 0x0517 BIN exactly.
Both V3 slots were valid (A/7, B/8), active B. Immediate postflash readback
was byte-identical with the same SHA. No SAVE occurred during flashing.

## Calibration And Power Cycle

The user confirmed an empty scale, entered CAL, pressed FUNCTION for zero,
observed the displayed standard value 500.00 g, then placed the 500 g standard
and pressed FUNCTION. The user reported `CAL SP`, then `donE`, followed by an
automatic return to the weighing page. No `LoAd` or extra confirmation key was
reported. `SAUE` was **not independently observed** during calibration; the
read-only recorder instead confirms one SAVE and the V3 committed result.
The 10 ms `APPLY` display was not captured by the 0.5 s read-only poller.

The calibration recorder captured 1,027 rows in 514.5 s, with zero read errors.
The load began near 2026-09-24 18:52:48 UTC and the sole SAVE completed near
18:52:59 UTC. Revision/saved advanced once from 8/8 to 9/9; dirty/fault/
overrun ended at zero. Active V3 slot A/9 had valid CRC and commit. Calibration
readback: raw zero -44003, raw span -487921, span mass 500,000,000 ug,
calibration sequence 3. The deliberate SAVE changed configuration SHA-256 to
`D8BFD7D0145C7EE608B9DA3A8576C031A8E60B73D1A10B52309C2B41EB10EC3A`.

Median windows, using raw gross rather than single display points:

| Window | Samples | Median gross |
|---|---:|---:|
| Pre-calibration empty | 120 | -0.021400 g |
| Post-calibration 500 g stable | 120 | 500.025906 g |
| Post-calibration unloaded stable | 120 | 0.002253 g |
| Loaded-minus-empty after calibration | | 500.023653 g |

The user performed one real physical power cycle with the scale unloaded.
The next probe reported uptime 37.3 s, 0x051C, clean 9/9, active slot A/9,
fault/overrun=0, and the same calibration fields. Post-cycle 500 g and empty
windows were officially stable, six records each, with medians 499.985358 g
and 0.015769 g; the loaded-minus-empty span was 499.969589 g. No extra SAVE
was reported after the reboot.

## Direct-Long Menu And Cleanup

The menu recorder captured 1,336 rows with zero read errors. The user first
changed brightness candidate 3 to 4 and cancelled it with TARE: no SAVE or
revision change was observed. A new brightness edit directly long-pressed
FUNCTION without short confirmation; the operator saw `SAUE` then `donE`,
and the recorder showed exactly one SAVE, clean 10/10.

The same direct-long path saved R5 OFF->SHADOW+STATIC (11/11) and restored
OFF+SHADOW (12/12). It saved Checkweigh OFF->STATIC (13/13) and restored OFF
(14/14). The operator reported `SAUE` then `donE` for each. All five post-cycle
menu SAVEs advanced exactly one revision, and no fault/overrun/read error was
observed. R5 offset/reference and formal Checkweigh outputs ended at zero/off.

The pre-disposition configuration was active slot B/14, both slots valid
and committed, with SHA-256
`D3F095E71FDFD4D28BEA6C7F6C56632D295179187403A166704E6ADEDFC083D2`.
Against the pretest active V3 payload, only six bytes differed: calibration raw
zero/raw span, calibration sequence, and brightness 3->4. The packed R5 and
Checkweigh request byte was zero in both records.

The user chose to retain 0x051C and restore brightness 3. The local direct-long
SAVE showed `SAUE` then `donE` and advanced to clean revision/saved 15/15,
SAVE count 6 since the physical reboot. The final 4096-byte configuration SHA
is `D2FB83C7F981C963A6CE540CBA606A94C6CA53A58F0A0F2EF24C05FC504AF1D4`.
Both slots remain valid/committed, active A/15 and previous B/14. Final A/15
payload SHA-256 is
`433B60D7AE95ED1E63ECD74FD232222B88871982B6F38CD2AE546A04ADA235E0`,
identical to the post-calibration A/9 payload: the only final payload changes
against the pretest active record are five calibration bytes (raw zero/span and
sequence). Brightness is 3, and the packed R5/Checkweigh request byte is 0.
Final Modbus read reports 0x051C, Map 0x0104, 10 Hz/filt3/strength3,
R5 OFF+SHADOW, Checkweigh OFF, offset/reference/evaluation zero, clean 15/15,
fault/overrun zero. No extra physical power cycle was performed after the
brightness restoration; the earlier real power cycle verified calibration
persistence before the menu tests.

The final brightness recorder retained one Modbus response timeout at
2026-09-24 19:24:19.202 UTC, during the SAVE maintenance window. Reading
resumed by the next sample, the save completed, and device fault/overrun
remained zero. This is reported as one transient host read error, **not** zero.
The calibration recorder had zero errors; the earlier menu recorder also had
zero errors. Original logs, all raw samples, failed host attempts, configuration
images, and the preflash 0x0517 application backup are retained.

No 12-hour drift, eight-combination rate/filter, full Checkweigh matrix,
physical sensor fault, external DRDY timing, ASan/UBSan, cross-sensor, or
formal metrology qualification was rerun or claimed.
