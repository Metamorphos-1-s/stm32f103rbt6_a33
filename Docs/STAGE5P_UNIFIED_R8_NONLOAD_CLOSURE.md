# Stage 5P Unified-R8 non-load closure

Baseline: `ac94ffc6cf45c6e4a49a00d23d17e3a817b9325c`

## Candidate and software

- Candidate: `0x0524 / 0x010C`
- BIN: 96,620 B
- SHA-256: `CBAB6D5E294D36F62231F9A233DF9E915E9F599D4B4826E6DC993A95035FFBF1`
- Host Debug/Release regression: 47/47 each
- ARM Release strict build: PASS and generated the exact SHA
- R7 fresh ARM Debug `-O0` link overflow remains a configuration FAIL; it is
  retained and not reclassified as a Release failure
- Cross-record nine-path engineering evidence: PASS
- Single-epoch resource qualification: INCOMPLETE

## Preflight and image preservation

Preflight found `0x0520/0x0109`, 10 Hz/filt1/strength3, calibrated,
OFF+SHADOW, offset 0, revision/saved 19/19, dirty/fault/overrun 0. The
application backup SHA was
`F85404B5D551EB44CB9B585793CA1935D6D06D7DC4608B9F986956B9FAA84A6B`; the V3
configuration SHA was
`856BD8F5C14760561FC4BFEC4274FE0C5480C19BC4422046617C2450F8439733`.

The exact 0x0524 candidate was installed app-only and Verify succeeded. Boot
readback was `0x0524/0x010C`, OFF+SHADOW, offset 0, with the configuration SHA
unchanged.

## Non-load sampling result

Profile switching was exercised without weights. The active profile and
sample sequence were read back under normal Modbus polling; no fault, overrun
or FIFO failure was observed. The run did not provide load/step or compensation
efficacy evidence. Physical 500 g load/unload, non-zero offset DOSING freeze and
weight-span qualification remain **NOT RUN** by explicit scope.

## Persistence hard stop

The profile transition was still asynchronous when the pair request was sent:

- ACTIVE+STATIC pair response: `BUSY`;
- a following explicit SAVE response: `ACCEPTED`;
- the saved request remained R5 OFF, not the requested ACTIVE+STATIC;
- revision/saved became `21/21`, SAVE count increased to 1.

This is a persistence-contract failure. No second SAVE or corrective tuning was
attempted. Menu/PLC persistence, real power-cycle migration, battery UI and
remaining functional tests were stopped.

## Recovery and final state

The exact pre-test 0x0520 application and V3 configuration were restored. Both
restored byte hashes equal the backups above. Final readback:

- `0x0520 / 0x0109`;
- OFF+SHADOW, offset 0;
- revision/saved 19/19, dirty/fault/overrun 0/0/0;
- calibration valid, Checkweigh OFF.

## Gate table

| Gate | Result |
| --- | --- |
| Candidate identity and Verify | PASS |
| Configuration unchanged by flash | PASS |
| 10/40 Hz no-load/profile readback | INCOMPLETE |
| 500 g physical load/unload | NOT RUN |
| Non-zero offset DOSING freeze | NOT RUN |
| Menu/PLC | NOT RUN |
| Planned SAVE and power-cycle migration | FAIL, stopped on BUSY/SAVE ordering |
| Battery/low-voltage/idle UI | NOT RUN |
| Exact rollback | PASS |
| A13B long-duration efficacy | INCONCLUSIVE |

This run does not establish 0x0524 delivery completion, formal Release status,
long-term drift efficacy, cross-sensor qualification or metrology certification.
