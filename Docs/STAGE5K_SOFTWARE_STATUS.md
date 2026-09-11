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
| Debug | 94,140 B | 19,392 B | `36FC04DB647A38BD7728246D8509D0A0F5A11559D8FD3121A5F02007D7CDBD2C` |
| Release | 80,920 B | 19,352 B | `B213D4B7A8310AE751E4818EBD8EB1DBFA30C24514BA194DE857D990733CA957` |
| BoardDiagnostics | 117,144 B | 19,264 B | `46FB036329C0AE5C43128881CA213B5F5A076E929F8CBC1940923332EF2D774F` |
| USART3 Bringup | 93,840 B | 19,008 B | `C55DA68C3010A1158EE5421411090FFF49A982368DE332CD0BC7FB30C5204659` |

Compared with the Stage 5J reference, the active ConfigStore path remains
smaller despite the validator and diagnostic additions. Historical codec and
projection source have been removed; V1/V2 numeric schema constants remain
only for explicit unsupported-slot detection.

The product identity is Firmware `0x0510`; PC strict preflight requires a new
software baseline and hardware validation remains separate.

Current conclusion:

```text
STAGE 5K IMPLEMENTATION COMPLETE; PORTABLE HOST VERIFICATION PENDING
```
