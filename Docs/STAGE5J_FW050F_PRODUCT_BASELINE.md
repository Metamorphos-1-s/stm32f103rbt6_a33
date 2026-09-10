# Stage 5J Firmware 0x050F product baseline

## Baseline

- Product code: `b119703cee70b228aa240e7f3477c7dca9946841`
- Starting evidence: `5236da68341e8feed0c6f6aedbc5ee52cea91f3b`
- Firmware / Register Map / Schema / BLE Protocol: `0x050F` / `0x0104` / `2` / `1`
- Release ELF SHA-256: `15C8269A80962E2CA7329A2623AB69F2286C2E3336373D0B658E2755E2B8DE8D`

Stage 5J changes evidence verification and diagnostic build settings only. It
does not change the 0x050F product executable or its protocol behavior.

## Reproducible evidence

`Tools/evidence/verify_firmware_manifest.py` validates the 0x050F evidence
Manifest, referenced file existence, UTF-8 JSON syntax, byte lengths, SHA-256,
identity, ConfigStore state, active slot, sequence and brightness. Evidence
JSON under `Docs/evidence` and `Results` is fixed to LF by `.gitattributes`;
binary dumps are never treated as text. Manifest hashes now describe Git
repository bytes rather than a Windows CRLF working tree.

The hardware result data was not changed. The corrected repository-byte hashes
are:

- final probe: `B3BD6269FE620F8FF3DCC7EFB3F554CB77DD3B321DF800F742E51FCC5418F08F`
- final parsed slots: `8E09AC69B510831500F7D19161F60E35383384D8A4B7BF94B63834784BC1FA43`
- final 4096-byte config region: `4BCDE2EB7482D29D2AD5153FC1166BBFA1DC651B95F62314F1B50019587F1867`

## Software regression

| Gate | Result |
| --- | --- |
| Host CTest | PASS, 16/16 |
| Stage 5B Python | PASS, 30/30 |
| Stage 5C Python | PASS, 12/12 |
| Evidence verifier tests | PASS, 4/4 |
| Debug | PASS, 96,732 B Flash / 20,144 B RAM |
| Release | PASS, 83,040 B Flash / 20,104 B RAM |
| BoardDiagnostics | PASS, 118,904 B Flash / 20,024 B RAM |
| USART3 Bringup | PASS, 96,432 B Flash / 19,760 B RAM |

BoardDiagnostics initially overflowed the 124 KiB application region by 6,716
bytes because new 0x050F menu/status transaction files were still compiled at
global `-O0`. Those non-diagnostic paths now use the existing diagnostic-image
`-Os` policy while diagnostic modules remain `-O0`. The image leaves 8,072
bytes before configuration Slot A.

Host coverage includes FUNCTION/STATUS session candidates, short-confirm
non-commit, single long-confirm SAVE, TARE/timeout/validation/save rollback,
brightness preview isolation, source ownership, dual Modbus mailbox isolation,
Flash pause/resume, apply rollback and reboot-required behavior.

## Hardware status

The preserved 0x050F unified-menu evidence confirms FUNCTION cancel, timeout,
long-save, STATUS long-STAR isolation, brightness `3 -> 4 -> 3`, one physical
power cycle, valid A/B slots, and final A/25, revision 25/25, dirty 0.

Stage 5J supplemental results:

- RS232: PASS, 100 FC03 plus FC06/FC16 Staging write/readback/restore,
  exceptions 01/02/03, bad-CRC silence and post-error recovery.
- CH579 TCP to USART3: PASS, identity, 4x16 Active, FC03/06/16, exceptions
  01/02/03, Staging restore and source-owner cleanup.
- RS485: NOT RUN; the connected COM5 path is verified as RS232 and no physical
  switch operation was available to this run.
- BLE phone capture: NOT RUN; no mobile BLE session was attached to this run.
- PC 0x050F two-SAVE/two-physical-power-cycle qualification: NOT RUN; two
  operator-controlled physical power cycles are still required.

The first RS232 FC06 attempt is preserved as FAIL/exception 06. Its cause was
the preceding gateway validation tool restoring Staging values without ending
the USART3-owned candidate session. The firmware correctly isolated sources.
Both Stage 5J tools now send a same-source CONFIG_CANCEL in `finally`; a
TCP-then-RS232 sequential rerun passes and leaves both owners released.

Current conclusion:

```text
STAGE 5J SOFTWARE READY; HARDWARE CLOSURE PENDING
```
