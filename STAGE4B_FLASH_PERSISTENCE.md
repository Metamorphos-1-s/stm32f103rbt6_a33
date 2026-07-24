# Stage 4B Flash Persistence Handover

## 1. Baseline and scope

The actual Stage 4A baseline is
`fdac2a215a90c5012173c5db4840a3e0e2b1c6b2`. Its verified resource baseline is
46368/4384 bytes (Debug) and 26156/4392 bytes (Release). Stage 4B was
implemented incrementally on that exact delivery.

Stage 4B adds linker-protected Flash A/B storage, explicit V1 serialization,
CRC-32, power-loss-safe asynchronous commits, startup recovery, save/factory
reset commands, revision tracking, storage maintenance, diagnostics, and host
fault injection. It does not add Modbus, BLE application framing, W02 AT
initialization, USB, IWDG, online update, logs, or automatic periodic saving.

## 2. Relevant tree

```text
stm32f103rbt6_a33/
|-- App/
|   |-- app_main.c                         [startup/scheduling integration]
|   |-- device_manager.c/.h                [storage maintenance]
|   |-- metrology_manager.c/.h             [clean sampling restart]
|   |-- persistence_manager.c/.h           [new]
|   `-- system_context.c/.h                 [revision tracking]
|-- BSP/
|   `-- bsp_flash.c/.h                      [new, HAL boundary]
|-- Diagnostics/
|   `-- stage4b_storage_diagnostics.c/.h    [new]
|-- Protocol/command_service/               [save/reset commands]
|-- Services/
|   |-- config_store/
|   |   |-- config_store.c/.h               [A/B state machine]
|   |   |-- flash_backend.h                 [backend abstraction]
|   |   |-- persistent_codec.c/.h           [V1 codec]
|   |   `-- persistent_schema.h             [layout/schema constants]
|   `-- crc32/
|       `-- crc32.c/.h                       [new]
|-- Tests/host/
|   |-- fake_flash_backend.c/.h              [new]
|   `-- test_stage4b.c                       [new]
|-- UI/                                      [SAVE/reset menu and messages]
|-- cmake/STM32F103RB_FLASH_CONFIG.ld        [new]
|-- CMakeLists.txt
|-- CMakePresets.json
`-- STM32F103XX_FLASH.ld                     [original, unchanged]
```

CubeMX `.ioc`, `Core`, startup, HAL sources, and the original CubeMX linker
script were not modified.

## 3. Physical Flash layout

| Region | Address range | Size | Purpose |
|---|---:|---:|---|
| Application | 0x08000000-0x0801EFFF | 124 KiB | firmware and constants |
| Slot A | 0x0801F000-0x0801F7FF | 2 KiB | pages 124-125 |
| Slot B | 0x0801F800-0x0801FFFF | 2 KiB | pages 126-127 |

The custom linker script declares `FLASH`, `CONFIG_A`, and `CONFIG_B`, exports
all requested boundary symbols, and asserts the 124/2/2 KiB layout. Runtime
validation compares those linker symbols with the fixed device addresses before
exposing the production backend. All erase/program APIs reject addresses outside
the final 4 KiB. Mass erase is not used.

Formal `Debug` and `Release` use the custom script and define
`ENABLE_STAGE2B_BOARD_DIAGNOSTICS=0`. `BoardDiagnostics` defines it as 1 and
rejects save/factory-reset requests. Use:

```powershell
cmake --preset Debug
cmake --build --preset Debug
cmake --preset Release
cmake --build --preset Release
cmake --preset BoardDiagnostics
cmake --build --preset BoardDiagnostics
```

## 4. Record format

Each slot contains one record:

```text
offset 0       32-byte explicit little-endian header
offset 32      actual V1 payload (164 bytes)
unused         erased 0xFF
offset 2044    32-bit commit marker, written last
```

Header fields are encoded individually, never by casting a Flash address to a
structure:

| Field | Width |
|---|---:|
| magic (`0x41333343`) | 4 |
| record format version (`1`) | 2 |
| payload schema version (`1`) | 2 |
| header size (`32`) | 2 |
| payload length | 2 |
| sequence | 4 |
| record flags | 4 |
| CRC32 | 4 |
| reserved0/reserved1 (zero) | 8 |

The slot-tail marker is `0x434F4D54`. The factory-default flag is bit 0 of
`record_flags`.

## 5. Payload V1

V1 serializes every field of MetrologyConfig, CalibrationConfig,
StabilityConfig, CommunicationConfig, BluetoothConfig, AlarmConfig,
DisplayConfig, BatteryConfig, and SystemConfig. It also stores `weight_view`,
`current_tare`, and `tare_active`.

Encoding uses fixed-width integers, little-endian order, one-byte enums and
one-byte booleans. Booleans must decode as 0 or 1. Reserved bytes encode as zero
and must decode as zero. The codec does not depend on structure packing, enum
size, alignment, or compiler padding.

Transient weight, filtered raw value, daily zero offset, stability, alarms,
menu/key state, connection buffers, boot self-test, dirty/revision state, and
Flash operation state are not stored.

Only schema V1 exists. `PersistentCodec_Migrate` is the migration entry point;
unknown older or future schemas return `UNSUPPORTED_SCHEMA` and are not erased
or interpreted as V1. A future V2 should add an explicit V1-to-V2 migration.

## 6. Validation and tare restoration

Decoded records pass metrology/stability validation, calibration consistency,
all enum ranges, communication constraints, Bluetooth version/baud checks,
alarm ordering/hysteresis, display brightness/view, battery divider and enabled
threshold ordering, boolean/reserved encoding, and runtime view validation.

An uncalibrated record is valid. A record claiming valid calibration must pass
`CalibrationModel_Validate`; otherwise the entire record is rejected.

Tare is restored only when retention is enabled, calibration is valid,
`tare_active` is true, and tare is positive and no greater than the configured
overload/capacity limit. Invalid tare is cleared while the remaining valid
configuration is retained. `zero_offset_raw` is never restored.

## 7. CRC and commit

CRC-32/ISO-HDLC uses reflected polynomial `0xEDB88320`, initial state
`0xFFFFFFFF`, and final XOR `0xFFFFFFFF`. The `123456789` vector produces
`0xCBF43926`.

CRC covers header bytes before and after the CRC field plus the actual payload.
It excludes the CRC field itself and the commit marker. Reserved header bytes
are included and fixed to zero. The marker is programmed as two halfwords only
after body readback and CRC verification.

## 8. Asynchronous save state machine

```text
IDLE -> PREPARE -> ERASE_PAGE_0 -> ERASE_PAGE_1
     -> PROGRAM_BODY -> VERIFY_BODY -> PROGRAM_COMMIT
     -> VERIFY_FINAL -> COMPLETE
                         `-----------> ERROR
```

A request immediately copies DeviceConfig/RuntimeState and encodes a normalized
payload into internal fixed storage. External edits cannot alter the in-flight
record. A second request while active is rejected.

Each `Process` call performs no more than one page erase, 16 halfword body
programs, or one verification step. The commit step writes its two halfwords.
There are no unbounded loops, interrupt Flash operations, dynamic allocation,
or `HAL_Delay` calls.

The target is always the non-active slot. The active slot is never erased during
the save. The target becomes active only after commit readback and complete
record revalidation. Identical normalized payloads return `NO_CHANGE` without
erase/program.

## 9. Power-loss recovery

Power loss during either target-page erase, header/payload programming, body
verification, or the first commit halfword leaves the old record valid and the
target uncommitted. Power loss after the full marker leaves a record that is
selected only if its CRC, payload, and configuration all validate.

At startup:

- no committed slots: defaults, `NOT_FOUND`;
- one valid slot: use it;
- valid plus corrupt slot: recover the valid slot and count recovery;
- two valid slots: use wrap-safe newer sequence;
- equal sequences: deterministically choose A and report conflict;
- both unusable: load defaults with corrupt/validation/unsupported status;
- unsupported schema: preserve both slots and do not auto-erase.

Sequence `0xFFFFFFFF` is reserved for erased media. Increment skips it, and
comparison supports wrap (`0` is newer than `0xFFFFFFFE`).

## 10. Revision and dirty semantics

SystemContext tracks `config_revision`, `saved_revision`, and
`storage_has_record`. Persistent configuration/view/tare changes advance the
revision while skipping `0xFFFFFFFF`. Dirty means the current revision differs
from the saved revision.

A save records the requested revision. Successful completion clears dirty only
when the current semantic state is marked saved; edits made while a snapshot is
in flight remain dirty. Failure retains dirty. Loading a valid record initializes
both revisions to its sequence and clears dirty. Defaults use revision zero,
dirty false, and `storage_has_record=false`, so the first explicit SAVE still
creates slot A.

## 11. Sampling maintenance

PersistenceManager enters DeviceManager storage maintenance before erase/write:
RS485 remains receive, W02 is idle/released, CS1237 enters safe state, and the
measurement bridge stops publishing old samples. TM1628 remains available.

Every success and error path reinitializes CS1237 using the current config,
therefore internal REFOUT remains enabled and the expected register remains
`0x0C`. Metrology is restarted without re-publishing the pre-save raw sample;
filter/stability wait for new post-settling samples. Ordinary saves preserve
volatile zero/tare, while changed factory calibration clears them.

## 12. Commands, menu, and factory reset

`COMMAND_REQUEST_CONFIG_SAVE` is asynchronous: accepted, no-change, busy,
invalid-state, or internal-error are distinct. `ACCEPTED` never means the write
has completed. Completion remains observable through events and Persistence
status.

Factory reset uses REQUEST, CONFIRM, and CANCEL commands. The menu displays a
reset confirmation; TARE cancels and FUNCTION-long confirms. Confirm writes a
newer default record to the non-active slot before applying defaults to RAM.
It never erases both slots first. Successful reset clears calibration, tare, and
the old RAM configuration. SAVE displays SAVE/donE/noCHG/ErrSAv according to the
asynchronous result.

No automatic save occurs for samples, stability, page changes, key presses,
alarms, connections, tare, or intermediate calibration steps.

## 13. Diagnostics and events

The read-only Stage4B snapshot exposes slot validity/sequences, active slot,
store state/operation, current/saved revisions, dirty state, request/success/
no-change/failure counts, page erases, halfword programs, recoveries, CRC errors,
and last error for SWD Watch.

Save/load/factory-reset start, complete, no-change, recovery, defaults, and
failure events carry integer metadata only. ConfigStore has no EventQueue
dependency; event queue overflow does not erase PersistenceManager status.

## 14. Host verification

MSVC C11 `/W4 /WX` builds two targets and CTest reports 2/2 passed. Stage 4B
tests compile the production CRC, codec, ConfigStore, validators, and revision
logic directly. Coverage includes CRC vectors/segmentation, positive and reverse
calibration round trips, retained/cleared tare, malformed enum/bool/truncation,
A/B selection, no-change wear suppression, sequence wrap, corrupt active-slot
recovery, per-Process erase/program bounds, busy rejection, and 104 individual
power-cut points across both erases, every body halfword, and both commit
halfwords. Every reboot selected a complete old or complete new record.

These are simulated power cuts, not physical brownout tests.

## 15. ARM build and map verification

| Build | Stage 4B FLASH | Stage 4B RAM | Stage 4A baseline | Delta |
|---|---:|---:|---:|---:|
| Debug | 56036 B | 11160 B | 46368 B / 4384 B | +9668 B / +6776 B |
| Release | 31424 B | 11152 B | 26156 B / 4392 B | +5268 B / +6760 B |

Debug uses 44.13% of the 124 KiB application region and 54.49% RAM. Release
uses 24.75% application Flash and 54.45% RAM. Both retain adequate
STM32F103RBT6 capacity.

Debug and Release complete 76 compile/link steps with zero errors and zero
warnings. The map files export:

```text
__application_flash_end__ = 0x0801F000
__config_slot_a_start__    = 0x0801F000
__config_slot_a_end__      = 0x0801F800
__config_slot_b_start__    = 0x0801F800
__config_slot_b_end__      = 0x08020000
```

The final Debug loadable image ends at `0x0800DAE4`; Release ends at
`0x08007AC0`. Both are below `0x0801F000`, and CONFIG_A/CONFIG_B report zero
linked bytes.

## 16. Static checks and compatibility

PersistentCodec, ConfigStore, and CRC are HAL-free. HAL Flash appears only in
BSP. No dynamic memory, floating point, recursion, FreeRTOS, `HAL_Delay`, or raw
DeviceConfig-to-Flash memcpy is used. UI and Domain boundaries remain intact.
The original Stage 2B diagnostics and internal REFOUT `0x0C` regression remain.

## 17. Hardware status and remaining risks

**NOT TESTED ON HARDWARE.** Real Flash save, reset, reboot recovery, erase time,
program time, voltage sensitivity, endurance, SWD slot corruption, CS1237 FIFO
behavior during erase stalls, settling, and physical brownout timing have not
been validated. The software has no supply-voltage write inhibit, IWDG strategy,
ECC, authenticated records, or external EEPROM fallback. V1 is the only real
schema and has no field-level salvage policy.

Board validation must exercise A/B/A rotation, retained and non-retained tare,
calibration persistence, unsaved RAM rollback, active-slot CRC corruption,
missing commit markers, power removal during erase/body/commit, firmware-region
protection, and CS1237 return to internal REFOUT `0x0C`.

## 18. Stage 5 recommendation

Stage 5 should add protocol transports that reuse CommandService, a controlled
low-voltage Flash-write guard, real brownout/endurance qualification, and an
explicit V1 migration policy before any V2 schema is released. It should not
weaken the fixed linker boundary or A/B commit contract.
