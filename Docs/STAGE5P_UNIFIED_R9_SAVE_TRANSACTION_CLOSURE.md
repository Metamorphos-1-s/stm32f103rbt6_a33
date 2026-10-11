# Stage 5P Unified-R9 mode request and SAVE transaction closure

Baseline: `278f16c0ef060c2371fce85de408595be2a02c6c`

## R8 BUSY evidence level

R8 preserved parsed command results and state snapshots but did not preserve the
complete raw mailbox TX/RX frames and generation readback surrounding the BUSY
response. Therefore the exact BUSY source is **UNCONFIRMED**. It is not valid
to attribute it solely to profile switching. R8 did show that the host sent
SAVE after the pair failure, and that SAVE persisted the then-current OFF
request; the R8 failure and rollback conclusion remains unchanged.

## Host transaction correction

`Tools/stage5_unified/r9_save_transaction.py` now enforces:

1. pair command result and asynchronous runtime readback;
2. fresh generation and readback of ACTIVE+STATIC/checkweigh OFF;
3. no SAVE on BUSY, rejected pair, timeout or readback mismatch;
4. one SAVE only after all preconditions;
5. SAVE completion only when saved revision equals revision and dirty is zero.

The first implementation also exposed and fixed a host parsing defect: the A13
active diagnostic block is 32-bit metrics (two registers each). Host tests pass;
the existing Host Debug/Release regression remains 47/47.

## Exact candidate and preflight

The exact 0x0524/0x010C candidate was installed app-only and Verify succeeded:

- 96,620 B;
- SHA-256 `CBAB6D5E294D36F62231F9A233DF9E915E9F599D4B4826E6DC993A95035FFBF1`;
- V3 configuration SHA unchanged during installation.

Preflight was 0x0520/0x0109, 10 Hz/filt1/strength3, calibrated, OFF+SHADOW,
offset zero, revision/saved 19/19, dirty/fault/overrun zero. The application
backup SHA was
`F85404B5D551EB44CB9B585793CA1935D6D06D7DC4608B9F986956B9FAA84A6B`; the V3
configuration backup SHA was
`856BD8F5C14760561FC4BFEC4274FE0C5480C19BC4422046617C2450F8439733`.

## Successful SAVE and power-cycle evidence

The corrected transaction produced:

- pair ACTIVE+STATIC: result `OK`, readback application ACTIVE/mode STATIC,
  checkweigh OFF;
- SAVE: exactly one request, result `ACCEPTED`;
- asynchronous completion: saved revision equalled revision and dirty cleared;
- after physical power cycle: 0x0524/0x010C, revision/saved `20/20`, active
  slot B, storage sequence 20, calibration and communication parameters intact;
- startup R5 request restored ACTIVE+STATIC, with offset initially rebuilt from
  the valid sample stream rather than treated as a persisted offset. Observed
  offset was approximately `-1025` to `-1040 µg`; no load was applied.

No second SAVE was issued.

## Scope-limited results

| Item | Result |
| --- | --- |
| Exact image identity/Verify | PASS |
| Profile transaction sequencing | PASS |
| ACTIVE+STATIC request and checkweigh OFF | PASS |
| One-shot SAVE and async completion | PASS |
| Physical power-cycle persistence | PASS |
| V3 calibration/address/unit retention | PASS |
| Physical 500 g load/unload | NOT RUN by scope |
| Non-zero-offset DOSING freeze | NOT RUN by scope |
| Menu/PLC navigation and concurrency | NOT RUN |
| Battery percentage/idle dimming | NOT RUN |
| Low-voltage physical stimulus | NOT RUN |
| Single-epoch resource qualification | INCOMPLETE |
| Cross-record nine-path engineering evidence | PASS |
| A13B long-duration efficacy | INCONCLUSIVE |

The device remains on the verified 0x0524 engineering candidate with the
persisted ACTIVE+STATIC request. This is not a formal Release, long-duration,
cross-sensor or metrology certification.
