# Stage 5P Unified-R10C battery transaction hardware result

Date: 2026-10-12 Asia/Shanghai
Branch: `stage5p-unified-r10c-battery-transaction`

## Fixed evidence boundary

The following historical software conclusion remains unchanged:

```text
TRANSACTION C TEST: INCOMPLETE
FULL 47-TEST CTEST: INCOMPLETE
```

This is a single engineering sample result, not a replacement for the
missing Host mailbox/ConfigStore integration test.

## Preflight and backups

Preflight at `2026-10-12T18:38:02.308199Z` found `0x0524 / 0x010C`,
10 Hz/filt1/strength3, valid calibration, R5 ACTIVE+STATIC, Checkweigh OFF,
revision/saved `20/20`, dirty/fault/overrun `0/0/0`, and volatile offset
`+110949 ug`.

This run's complete SWD backups were:

```text
application_before.bin  2FFDE2F34986566D0923229EB99C7F50A2D4B1EAEBE9FF307C03A395A7EDB99B
config_before.bin       86A3CFC29E0567FD78A6597E5A221AC72D2CF274E8C8D9888EC77011D3D54785
```

Both V3 slots were valid/committed/CRC-clean. A was sequence 19; B was
active sequence 20. Both contained the legacy battery tuple `0/0/0/0,
alarm=false`.

## Application installation

The exact `0x0527 / 0x010F` binary was verified before programming:

```text
97,196 B
53C96D74C6830C266EEA4FCAC9A5AC60AC9F2345FF794ADC8E86CE99FEBC8F5B
```

Only application sectors 0..94 were erased/programmed; CubeProgrammer
reported successful Verify. The configuration region remained byte-identical
to the preflash SHA. Postflash readback was `0x0527 / 0x010F`, old
calibration/profile/configuration intact, revision/saved `20/20`, dirty/fault/
overrun zero, and offset zero after reset.

## RAM-only transaction

The raw transaction evidence is in `ram_transaction.json`.

- Invalid recovery threshold returned `INVALID_ARGUMENT`; active fields stayed
  legacy-zero.
- `CONFIG_CANCEL` returned OK and left revision/saved `20/20`, dirty zero.
- Valid five-field candidate returned `VALIDATE=OK`, `APPLY_RAM=OK`.
- Atomic active readback was exactly `+400 / 13600 / 13200 / 13900 / enabled`.
- Revision changed `20 -> 21`, dirty became 1, saved stayed 20, SAVE count
  stayed 0.

## Unsaved power-cycle

After the first physical power cycle, `after_unsaved_power_probe.json` showed
the candidate disappeared: revision/saved returned `20/20`, dirty zero,
legacy-zero battery fields and R5 offset zero after restart. This proves the
RAM candidate was not persistent.

## Explicit SAVE and persistence

The second candidate was rebuilt from a fresh session/generation. SAVE was
sent exactly once after successful validation, RAM apply and field readback.
The command returned `ACCEPTED`; the subsequent read showed revision/saved
`21/21`, dirty zero and SAVE count 1. The active V3 slot became A, sequence
21, with valid CRC/commit. `config_after_save.bin` SHA-256:

```text
7E2B19D04B2876280448B12D8C3D5B812C501C22C80F69B88D1F49DE44F1220B
```

Slot B sequence 20 remains valid and contains the old tuple. Slot A contains
the five new values; calibration, communication, profile, unit, brightness,
tare and R5/checkweigh request fields were preserved by the decoded payload.

After the second physical power cycle, `after_saved_power_probe.json` read
the five active words exactly, revision/saved `21/21`, dirty/fault/overrun
zero, calibration valid, 10 Hz/filt1/strength3 and R5 ACTIVE+STATIC with a
new volatile offset starting at zero. Final probe later showed offset `-2064
ug`, consistent with normal STATIC evolution.

## User-observed voltage UI

These are visual observations, not Modbus voltage measurements:

| Supply setting | Observation | Result |
| --- | --- | --- |
| 12.7 V | Battery page `0P`; battery LED flashing; NET/GROSS intermittently showed `Lo-bAt`, with long gaps and short display duration | USER-OBSERVED PASS for warning behavior |
| 14.45 V | LED stopped flashing; NET/GROSS no longer showed `Lo-bAt`; battery page showed `40P` | USER-OBSERVED PASS for recovery behavior |

The public map does not expose raw battery ADC or corrected millivolts, so the
actual `+400 mV` conversion error was not independently measured. The test
demonstrates the voltage-warning/display logic, not true 4S battery state of
charge accuracy. A separate normal-voltage visual baseline was not recorded;
the recovery setting above is the available high-voltage observation.

## Final state and scope

Final read-only probe at `2026-10-12T18:51:39.112582Z`:

```text
Firmware/Map: 0x0527 / 0x010F
Profile: 10 Hz / filt1 / strength3
Revision/saved: 21 / 21
Dirty/fault/overrun: 0 / 0 / 0
R5: ACTIVE + STATIC, offset -2064 ug
Checkweigh: OFF
Final config SHA: 7E2B19D04B2876280448B12D8C3D5B812C501C22C80F69B88D1F49DE44F1220B
```

The battery issue “0P but no low-voltage warning” is resolved on this sample:
the saved legacy fields were explicitly replaced, `0P` remains expected at
12.7 V, and the warning/recovery UI was observed. Formal metrology, true
battery SOC accuracy, 500 g load/unload, 40 Hz matrix, long drift and full
resource qualification were not rerun.
