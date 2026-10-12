# Stage 5P Unified-R10C battery transaction candidate

Starting point: `2eddd0c08a76dc79aa357428d864221f5513ad0c`.
This is an unflashed software candidate. No device write, SAVE, burn or
power cycle was performed. Existing user `.pyc` changes remain unstaged.

The candidate has a new engineering identity `Firmware 0x0527 / Map 0x010F`.
The frozen `0x0524 / 0x010C` image and V3 format are unchanged.

## Design

The existing complete ConfigEdit staging path does not expose battery fields.
The candidate adds an isolated battery-only staging block:

| Block | Registers |
| --- | --- |
| active readback | `0x03A0..0x03A8` |
| candidate writes | `0x03A9..0x03B1` |
| validation / dirty | `0x03B2 / 0x03B3` |

`CONFIG_BEGIN` snapshots the current transaction and generation. Battery
writes require that session and reject the normal staging owner or a changed
revision. `CONFIG_VALIDATE` builds a candidate containing only the battery
changes and runs the existing full validator. `CONFIG_APPLY_RAM` must return
OK before any SAVE is allowed; the host reads all nine active registers and
compares the five requested values. `CONFIG_CANCEL` refreshes staging. No
automatic migration is performed.

The host helper `Tools/stage5_unified/battery_transaction.py` defaults to
`perform_save=False` and refuses SAVE after rejected validation, BUSY,
generation mismatch or readback mismatch. SAVE is explicit opt-in.

## Fixed target values

```text
calibration_offset_mv = +400
low_warning_mv = 13600
critical_low_mv = 13200
recovery_mv = 13900
low_voltage_alarm_enable = true
```

Only these members are changed. Calibration, communication, unit, filters,
R5/checkweigh requests, brightness, tare and V3 metadata remain copied from
the active RAM configuration.

## Software evidence

ARM Release was built with `A33_ENABLE_STAGE5P_BATTERY_TRANSACTION=ON` and the
existing Stage 5P dependencies. Build result: PASS. RAM is `19,416 B / 20,480
B`; application Flash is `97,196 B / 126,976 B`; identity is `0x0527 / 0x010F`.

```text
ELF SHA-256 52108E8A83E516C9BADA5B46D60FDACD1E525BEC3014C74E15B0509F146FA26D
BIN 97,196 B
BIN SHA-256 53C96D74C6830C266EEA4FCAC9A5AC60AC9F2345FF794ADC8E86CE99FEBC8F5B
```

The linker places `_ebss` at `0x200047D8`; the existing reserved heap/stack
block follows it. Conservative free RAM before the 512-byte indirect-call
allowance is 1,064 B; after that allowance it is 552 B, above the 512-B gate.
No DMA, Modbus, BLE or reserved stack buffer was reduced. Per-function `.su`
output was not enabled, so dynamic nested-IRQ high-water evidence is NOT RUN.

The Host CTest executable could not be linked because no native Windows C
compiler is installed; ARM GCC is not a host linker. The pure-Python battery
contract tests passed. Full host/strict-warning/stack-usage regression for
this new candidate is INCOMPLETE.

## Burn authorization stop

No write was authorized or attempted. Installing this candidate would reset
the MCU and may clear the current volatile R5 offset. Current backups remain:

```text
application: 2FFDE2F34986566D0923229EB99C7F50A2D4B1EAEBE9FF307C03A395A7EDB99B
config:      86A3CFC29E0567FD78A6597E5A221AC72D2CF274E8C8D9888EC77011D3D54785
```

Physical 12.7-V/normal/recovery checks, SAVE/power-cycle, post-migration
readback, visual `Lo-bAt`, indicator flash and battery-page `0P` behavior are
NOT RUN. They require explicit burn and configuration authorization after the
incomplete software gates are closed.
