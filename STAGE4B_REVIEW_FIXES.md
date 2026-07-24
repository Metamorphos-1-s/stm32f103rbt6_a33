# Stage 4B-R Review Fixes

## Baseline and scope

The actual baseline SHA is `40ebd3e8f69ac21deb349070563c574046826fbb`.
This delivery only repairs Stage 4B persistence. The A/B addresses, 2 KiB slot
size, 1 KiB page size, explicit little-endian V1 format, CRC-32/ISO-HDLC and
commit-marker-last protocol are unchanged. CubeMX `.ioc`, `Core`, startup, HAL
sources and the original CubeMX linker script are unchanged.

## Correct revision ownership

The defect was caused by reading the current revision after Flash completion.
That revision may describe edits made after the encoded snapshot was accepted.
PersistenceManager now retains the operation type and `s_requested_revision`.
Ordinary success and `NO_CHANGE` call
`SystemContext_MarkRevisionSaved(s_requested_revision)`. If revision 10 is
encoded and the user edits to revision 11 during the save, Flash contains the
revision-10 configuration, `saved_revision` is 10, `config_revision` is 11 and
dirty remains true. Storage failure never advances `saved_revision`.

`SystemContext_MarkRevisionSaved` explicitly accepts a historical revision,
recomputes dirty from current versus saved revision, and rejects reserved
`0xFFFFFFFF`. Revision increment still wraps from `0xFFFFFFFE` to zero.

## Factory-reset commit semantics

Factory defaults and zeroed default runtime are constructed before maintenance.
Device/calibration validation, ConfigApplication acceptance, runtime validation
and V1 codec validation all occur before the first erase. A preflight failure
does not enter maintenance or mutate Flash.

`FactoryResetResult` separates `COMPLETED`, `FAILED`, and
`COMMITTED_REBOOT_REQUIRED`. If the new default record is valid and committed
but RAM application or restart fails, the new record is not erased and the
result requests a reboot rather than reporting an ordinary pre-commit save
failure. A successful reset clears calibration, tare and volatile zero state by
applying defaults and reinitializing metrology/CS1237 settling.

## RAM and streaming verification

Stage 4B permanently allocated three maximum-format arrays and two snapshots.
Stage 4B-R stores only the encoded V1 snapshot and verifies Flash in 64-byte
chunks. Header CRC segments and each payload chunk are fed through
`Crc32_Update`; decode starts only after CRC success. Unknown schemas are not
decoded as V1.

| Static buffer | Stage 4B | Stage 4B-R |
|---|---:|---:|
| encoded body | 2016 B | 197 B (196 logical + 1 padding) |
| full verify buffer | 2016 B | 64 B chunk |
| active payload | 1984 B | 164 B |
| slot-load payload buffers | 0 B | 164 B + 164 B |
| DeviceConfig/Runtime snapshots | structure copies | removed |
| listed buffer total | at least 6016 B plus snapshots | 753 B |

The exact selected payload is copied during the first validated slot reads;
there is no unchecked second read. `s_active_payload_valid` is cleared by init
and load start, set only after validation, and gates `NO_CHANGE` comparison.

Logical body length is distinct from programmed body length. Programming rounds
up to an even byte count. For an odd logical length the final high byte is
`0xFF`; it is outside the payload and CRC. Tests cover 1-byte, 3-byte and maximum
odd payload-derived lengths without reading past the logical buffer.

## Flash errors and low voltage

`FlashBackendOperationInfo` retains the primary erase/program/verify result,
secondary lock result, HAL error and address. A lock failure never overwrites a
primary failure. Lock failure disables later writes until backend
reinitialization; reads remain available. If the final commit halfword was
programmed but locking failed, ConfigStore rereads and fully validates the
record, then reports `CONFIG_STORE_OPERATION_COMMITTED_LOCK_ERROR` rather than
calling it corrupt.

The PVD guard uses STM32F1 PVD level 6 as a conservative development default
(nominally about 2.8 V) and requires 500 ms continuously safe supply before a
new operation. This threshold is **DEVELOPMENT DEFAULT - VERIFY ON HARDWARE**.
Every write-state advance also reads the live PVD flag. Unknown, unstable or
unsafe supply returns `COMMAND_RESULT_POWER_UNSAFE` without a system fault.
Supply loss during an operation stops subsequent writes and records
`FAULT_CONFIG_SAVE_POWER_INTERRUPTED`; the old active slot remains valid.
Battery ADC thresholds are not used as a hard gate because final board battery
limits are not yet qualified.

## Cooperative timing limits

The save is a bounded cooperative state machine advanced by `App_Run`: one
`ConfigStore_Process` call erases at most one page, programs at most 16 body
halfwords, or performs one verification/commit step. However,
`HAL_FLASHEx_Erase` and each halfword program are synchronous blocking calls.
A page erase may block for tens of milliseconds. Storage maintenance protects
CS1237 timing, but Stage 5 UART reception will need a separate loss strategy.
No claim is made that the main loop runs in real time during a Flash HAL call.

## Verification results

- Host build: MSVC C11 `/W4 /WX`, three targets, zero warnings/errors.
- CTest: 3/3 passed (`stage4a`, storage, production PersistenceManager).
- Persistence integration: in-flight edit preserves dirty, `NO_CHANGE`, busy,
  failure, factory preflight, successful factory reset, committed/RAM-failed
  reboot recovery, and PVD start gating passed.
- Storage regression: CRC vector/streaming, V1 round trip, A/B recovery,
  sequence wrap, odd lengths, lock error preservation, low-voltage interruption
  and 104 cut-after cases passed. The save has 102 actual Flash mutations (2
  erases, 98 body halfwords, 2 commit halfwords); extra cases cover post-commit.
- Debug: FLASH 58664 B, RAM 5232 B.
- Release: FLASH 32904 B, RAM 5232 B.
- BoardDiagnostics: FLASH 58344 B, RAM 5240 B.
- Relative to Stage 4B: Debug RAM -5928 B; Release RAM -5920 B. Removing the
  reserved 512-byte heap contributes 512 B; buffer removal provides the rest.
- CONFIG_A and CONFIG_B linked usage remain 0 B; application FLASH remains
  limited to 124 KiB and ends before `0x0801F000`.
- Static application-source scan found no dynamic allocation, floating point,
  FreeRTOS, `osDelay` or `HAL_Delay` use.
- CS1237 still enables internal REFOUT and its default register is `0x0C`.

## Files and board validation

Core changes are in `App/persistence_manager.*`, `App/config_application.*`,
`App/system_context.h`, `Services/config_store/*`, `BSP/bsp_flash.*`, new
`BSP/bsp_power_monitor.*`, new `Services/storage_power_guard/*`, diagnostics,
project configuration/CMake, the custom linker script, and host tests/adapters.

Before Stage 5, measure the real PVD trip/recovery voltage, supply-fall save
rejection, erase/program latency, TM1628 and interrupt stalls, CS1237 `0x0C`
reconfigure/settling, FIFO overrun, physical A/B rotation/corruption and actual
power cuts. Stack watermark must be measured before stack reduction. Flash-lock
failure remains a fault-injection-only test.

**NOT TESTED ON HARDWARE.**
