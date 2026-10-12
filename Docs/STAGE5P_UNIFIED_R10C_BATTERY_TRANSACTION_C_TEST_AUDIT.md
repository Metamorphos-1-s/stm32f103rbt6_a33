# Stage 5P Unified-R10C battery transaction C-test audit

Baseline: `f1da7e45ac5eff325cab035d4138d088a946a8f2`.
No device access or firmware/configuration write was performed.

## Responsibility boundary

Firmware responsibilities:

- `ModbusCommandMailbox` validates command/token framing and dispatches
  `CONFIG_BEGIN`, `CONFIG_VALIDATE`, `CONFIG_APPLY_RAM`, `CONFIG_CANCEL` and
  `REQUEST_SAVE`.
- `ModbusRegisterModel` owns the battery active/staging registers, source
  owner lease, base revision check and candidate construction.
- `ConfigApplication_Validate/Apply` validates the complete candidate and
  commits RAM state; `ConfigStore`/`PersistenceManager` performs the actual
  V3 A/B Flash SAVE and recovery.

Host responsibilities:

- `battery_transaction.py` sequences the commands, refreshes readback and
  sends SAVE only after validation, RAM apply and exact active readback.
- It records a failed command and deliberately sends no SAVE in that failed
  transaction. This does not make a later independent SAVE impossible; the
  firmware only rejects SAVE when its own command/state/owner/power rules
  reject it.

## What was actually executed

With LLVM-MinGW Clang 23.1.3 and the unified macros
`A33_ENABLE_STAGE5P_UNIFIED=1` and
`A33_ENABLE_STAGE5P_BATTERY_TRANSACTION=1`, Debug and Release built and ran
`stage5p_unified_tests` (`1/1 PASS` each). The real C code executed includes
`DeviceConfig_Validate`, `BatteryAdc`, `ConfigEdit`, `CommandService` and
V3 codec sources. Assertions cover target five-field validity, invalid
threshold ordering, alarm-disabled legacy-zero values, percentage arithmetic
and the 60-sample warning confirmation.

## Missing integration coverage

The target does **not** link `Protocol/modbus/modbus_register_model.c` or
`modbus_command_mailbox.c`, and therefore does not execute the new battery
register transaction. It does not prove:

- same-transaction five-register update and active readback;
- `CONFIG_BEGIN → VALIDATE → APPLY_RAM` mailbox result flow;
- dirty/revision versus saved revision at APPLY_RAM;
- explicit SAVE and ConfigStore reload through this transaction;
- generation mismatch, non-owner BUSY, cancellation or APPLY failure;
- host no-SAVE-on-failure trace for the actual C mailbox path.

Existing fake Flash tests prove generic ConfigStore save/recovery behavior,
but do not connect the new Modbus battery staging path to that store. They
cannot be relabelled as this transaction's end-to-end C evidence.

## Conclusion

```text
TRANSACTION C TEST: INCOMPLETE
HOST FOCUSED CONFIG TEST: PASS
HOST DEBUG/RELEASE BUILD: PASS
FULL 47-TEST SUITE: INCOMPLETE (unbuilt targets retain the known Clang menu warning)
```

Before any 0x0527 hardware use, add a dedicated Host executable that links
the Modbus register model/mailbox plus the existing fake Flash and storage
adapters, or provide an equivalent product-level test seam. It must assert
the missing paths above and preserve field-by-field equality for calibration,
communication, unit, filter, R5, checkweigh, brightness and tare. Until then,
the candidate remains software-incomplete for the requested write-transaction
gate; no burn authorization is implied.
