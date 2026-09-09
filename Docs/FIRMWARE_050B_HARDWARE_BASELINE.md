# Firmware 0x050B candidate hardware baseline

Date: 2026-09-09

UTC start: `2026-09-09T06:43:02.5460752Z`

## Identity and build

- Branch: `fix-usart3-command-source-validation`
- Production source commit: `a3cc744a6cae85d670008fe6a1bf96eae63bd2a7`
- Evidence parser commit: `ca7ad8bdd2257f1fb46c83432929ebd96559fa56`
- Release ELF SHA-256:
  `F6607DE318CE03925C27D8F4B8AA98020F3FC016EF8221229BBEEA8BE16C6FAC`
- Firmware: `0x050B`
- Register Map: `0x0104`
- Config Schema: `2`

This record is a **Firmware 0x050B Candidate Active Baseline** for a future PC
client rebaseline. It does not modify the PC client repository or declare a new
PC authoritative baseline.

## Offline gates

- CTest: 16/16 suites passed.
- Stage 5B C: 1213 checks passed.
- Stage 5B Python: 30/30 tests passed.
- Stage 5C Python: 12/12 tests passed.
- Register Map source validation passed.
- Debug: 95,168 B Flash / 17,768 B RAM, zero warnings/errors.
- Release: 81,916 B Flash / 17,736 B RAM, zero warnings/errors.
- Relative to `cb29eed523b1f6cd1fb4636e228454490399e0cd`:
  Debug Flash +200 B, Release Flash +168 B, RAM unchanged.
- `git diff --check` passed.

Async timeout tests cover APPLY/SAVE timeout message retention, return to the
owned transaction state, input blocking, delayed success/failure, candidate and
revision conflicts, exact one-APPLY/one-SAVE behavior, explicit SAVE-only retry,
FAULT display takeover without ownership loss, and firmware/Map/Schema display
identity. All tests use host fakes; they perform no hardware writes.

## Programming boundary

STM32CubeProgrammer 2.19.0 connected through ST-Link/SWD at 3.29 V. The Release
ELF was programmed once and verified once. Only application sectors 0..79 were
erased; configuration slots at `0x0801F000..0x0801FFFF` were preserved. The
programmer performed the single post-programming reset. There was no mass erase,
Option Byte operation, extra reset, or physical power cycle.

Configuration-region SHA-256 before programming, after programming, and after
manual STATUS verification was identical:

`8EF517795864C9113EFA7866C1876D35EDE09AF5EF63EEE7FBEDB7F9A96D09A8`

## Persistent configuration

The authoritative parser found both records valid, committed, Schema 2,
344-byte payload, and CRC-correct. Sequences are consecutive and select slot B.

| Slot | Sequence | Slot SHA-256 | Payload SHA-256 | Divider |
|---|---:|---|---|---|
| A | 15 | `8D2F406AFB9A1B5FB7BDC3D72D7EE6562FD47B308FCE9B921FFAD410B9BA23AA` | `09C817E62BE3A3519A910388E6FC51F8E51328F7B8BF8E05C9397AF695AB2D75` | 47000/10000 ohm |
| B | 16 | `88C16746BC30F30E5E41A92BDEBC40C3968B6E51F25F082DFDA1C6F648282C40` | `8ECBC5C77BFBC2800FE7707D1A82218B348DD2ECB3142AB06AC313A9C5BE0232` | 47000/10000 ohm |

Flash already persisted 47k/10k in both slots. The startup normalization's exact
30k/10k migration condition was therefore false. No migration SAVE or physical
power cycle was required or authorized.

## Runtime and Modbus evidence

Two independent COM5 sessions used 115200 8N1, Unit ID 1, and only FC03. Each
read two complete 64-register Active snapshots plus identity, diagnostics,
communication, storage, and mailbox state. Totals were 14/14 successful FC03,
zero failed FC03, zero FC06, zero FC16, and zero mailbox/configuration commands.

Both sessions produced identical 64/64 Active snapshots and canonical SHA-256:

`D52CD4E96F28E68BD9D4965CE27B0900CD3F7A59BA90864EB8524D190356287B`

Active registers (`0x0100..0x013F`):

```text
0000 0001 0007 0000 0000 0000 B2D0 5E00 0000 0000 000F 4240 0000 0000 0000 0001
0003 0001 0002 0001 0003 0001 0004 0000 0000 0000 0000 0000 0000 0000 0000 0000
0000 0003 0002 0001 0008 01F4 0000 0000 001E 8480 0000 0000 003D 0900 0001 0003
0000 0000 0008 01F4 0000 0000 001E 8480 0000 0000 003D 0900 0001 0000 0002 0000
```

Runtime state was ConfigStore IDLE, power-safe, dirty=0, current revision=16,
saved revision=16, fault mask=0, CS1237 RUNNING, and overrun=0. Storage reported
slot B, sequence 16. All 16 mailbox registers were zero.

Map `0x0104` does not expose battery divider or `migration_pending_save` through
FC03. A read-only ST-Link HotPlug read used the final ELF map to inspect
BatteryAdc's runtime `s_config`: top=47000 and bottom=10000 ohm. The same read
showed raw=3025, ADC=2438 mV, calculated VBAT=13897 mV, and valid=1. Because
Flash and RAM both contain 47k/10k, dirty is zero, and revision equals saved
revision, there is no pending battery migration. No claim is made that the
unmapped flag itself was directly observed.

## Manual confirmation and limits

The operator confirmed: `0x050B STATUS只读确认通过，无STATUS SAVE`. The STATUS
firmware value displayed 5.11, battery display was reasonable, nested TARE
navigation worked, and weighing returned normally. No STATUS SAVE was issued.

Not validated here: real communication APPLY, real configuration SAVE, timeout
behavior on physical Flash/UART faults, RS485/BLE/TCP cross-interface operation,
or PC Stage 2B persistence. The PC client must update its frozen firmware value,
Release ELF hash, candidate Active hash, and 47k/10k divider assumptions before
new qualification. Historical firmware `0x050A` evidence remains historical and
is not valid as the current firmware baseline.
