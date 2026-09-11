# Stage 5K software status

Branch: `stage5k-fw0510-correctness-refactor`.

Stage 5K-A is implemented and tested:

- Mailbox signed 64-bit assembly uses unsigned bit composition and
  bit-preserving `memcpy`; boundary tests cover `INT64_MIN` through `INT64_MAX`.
- ConfigStore sequence comparison is explicit modulo arithmetic with reserved
  `0xFFFFFFFF` handling.
- The Modbus register-model always-true GCC condition is removed.
- Half-auto TARE rejects gross mass `<= 0` before changing runtime tare.
- CONFIG staging ownership is source isolated; failed cross-source writes have
  no staging effect, CANCEL is owner-only and atomically restores Active,
  validation and dirty state, and owner leases are wrap-safe.
- Deferred SAVE has terminal outcomes, source/token/revision binding, and
  read-only Modbus diagnostics instead of dropping
  `PersistenceManager_RequestSave()` failures.

Stage 5K-B canonical storage is implemented:

- Internal format/schema V3, canonical payload 281 bytes.
- Public configuration schema remains independent and unchanged.
- V1/V2 records are unsupported and never migrated or decoded as V3.
- A/B slots, CRC32, sequence, commit-last and 2 KiB slot layout remain.

Stage 5K-C includes a shared pure `DeviceConfig` validator, semantic
configuration comparison based on V3 canonical bytes, revision and rebuild
helpers, and removal of the legacy configuration model. The register model
and command service remain conservative single-entry dispatchers because a
behavior-changing split was not justified by the current size and dependency
constraints.

## Software gate measurements

| Image | Flash text+data | RAM data+bss | ELF SHA-256 |
| --- | ---: | ---: | --- |
| Debug | 94,084 B | 19,392 B | `1FDE18BCD0E15CC5CAC87132DBB17A8E4310A18333E109DE9D398B581D00FCAD` |
| Release | 80,880 B | 19,352 B | `A82F89F0FE12480078D125BB8BEE581439A1CEFE1A77319EACFB7E4FEA396E2D` |
| BoardDiagnostics | 117,072 B | 19,264 B | `F4C58E9F792CA8E6455E5F54598534A633A6CF20AF7C20510A44D6CC921102D7` |
| USART3 Bringup | 93,784 B | 19,008 B | `087D2522C0C7FDC4FB0DE52D34C8EDB7B073CD7DD2ADF97399645CDB620AED24` |

Compared with the Stage 5J reference, the active ConfigStore path remains
smaller despite the validator and diagnostic additions. Historical codec and
projection source have been removed; V1/V2 numeric schema constants remain
only for explicit unsupported-slot detection.

The final alarm validation audit removed a duplicate signed subtraction that
overflowed for `INT64_MIN..INT64_MAX`. Extreme enabled/disabled alarm,
hysteresis, Modbus staging/APPLY, V3 encode, and LimitChecker arithmetic now
run in the host suite. Portable run `34610805291` passed GCC, Clang, and UBSan
for implementation commit `6d92b1efa39b9c9b2d6fdf950d0f3fbb6cfbb4fe`.

The product identity is Firmware `0x0510`; PC strict preflight requires a new
software baseline and hardware validation remains separate.

Current conclusion:

```text
STAGE 5K SOFTWARE READY; HARDWARE VALIDATION PENDING
```
