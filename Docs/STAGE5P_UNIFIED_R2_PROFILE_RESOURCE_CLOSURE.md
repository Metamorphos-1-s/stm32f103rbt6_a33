# Stage 5P Unified-R2 profile-switch and resource closure

Date: 2026-10-11 (UTC evidence timestamps)
Baseline: `a97526dd6a46e64d09d8c9544a7b345338c0df2f`

## Command-contract correction

The Modbus mailbox uses an external command table. Profile switching is
mailbox command **7**, not the C `CommandId` enum value 23. The new host
contract tests pass and require:

- profile 0 -> 1: result `ACCEPTED=1`, then asynchronous completion;
- profile 1 -> 0: result `ACCEPTED=1`, then asynchronous completion;
- requesting the current profile: `OK=0`.

The previous R report is retained unchanged; its wording is corrected by
`Docs/STAGE5P_UNIFIED_R_DIAGNOSTIC_HARDWARE_CLOSURE_ERRATA.md`.

## Corrected device evidence

Fresh preflight at `2026-10-10T17:46:43Z` found `0x0520/0x0109`, OFF+SHADOW,
10 Hz/filt1/strength3, calibration valid, revision/saved 19/19, dirty/fault/
overrun 0. A fresh 126,976-byte application and 4,096-byte V3 configuration
backup was made before installation. The backup hashes were:

- application: `F85404B5D551EB44CB9B585793CA1935D6D06D7DC4608B9F986956B9FAA84A6B`
- configuration: `856BD8F5C14760561FC4BFEC4274FE0C5480C19BC4422046617C2450F8439733`

The exact 0x0525 resource image was installed app-only and Verify succeeded;
configuration SHA remained unchanged. The corrected raw RTU record shows:

- FC16 command `7`, `arg0=1`, unique token `17590`;
- FC06 execute echo valid;
- FC03 response token `17590`, raw result `1`, last command `7`;
- asynchronous readback reached active profile `1`, sample rate `1` (40 Hz),
  CS1237 running state, with no fault or overrun.

This proves **command accepted** and **profile completed switching**. It does
not by itself prove target resource qualification.

## Resource result

The 40 Hz cumulative recorder stopped on its first validation snapshot and
automatically restored the backup. The snapshot was retained unchanged:

- diagnostics flags `3`, violating the frozen `flags=1` DWT/stack contract;
- `produced=5487`, `consumed=5487`, but `engine=4848`, so target conservation
  was not established;
- path coverage was only OFF at the stopping point;
- no 40 Hz Feed/App_Run/stack qualification was accepted.

Recorder result: **FAIL / INCOMPLETE**. Raw snapshot SHA-256:
`FB60FBF6CCA3BC01F10989C48249B141884001251B147C0BE3B8816116C017E7`.
The recorder's error was `invalid target diagnostics/lease/MSP`; no data was
interpolated or promoted to PASS.

The exact pre-test image and configuration were restored byte-for-byte. Final
readback is `0x0520/0x0109`, OFF+SHADOW, offset 0, revision/saved 19/19,
dirty/fault/overrun 0, Checkweigh OFF. Restored application SHA equals the
backup SHA above.

## Stage results

| Item | Result |
| --- | --- |
| Offline mailbox contract tests | PASS |
| Correct profile=1 command acceptance | PASS |
| Asynchronous completion to 40 Hz | PASS |
| 40 Hz target resource contract | FAIL/INCOMPLETE |
| Exact diagnostic rollback | PASS |
| 0x0524 flash and focused function tests | NOT RUN |
| SAVE/migration/physical power cycle | NOT RUN |
| Menu/battery UI | NOT RUN |
| A13B long-duration efficacy | INCONCLUSIVE, unchanged |

The remaining blocker is the diagnostic build/runtime contract (DWT/stack
flags and target engine counter conservation), not the profile mailbox
command. Do not install 0x0524 until a diagnostic identity with those gates
closed is available.
