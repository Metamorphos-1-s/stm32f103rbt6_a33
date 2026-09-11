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
- Deferred SAVE has observable internal terminal outcomes instead of dropping
  `PersistenceManager_RequestSave()` failures.

Stage 5K-B canonical storage is implemented:

- Internal format/schema V3, canonical payload 281 bytes.
- Public configuration schema remains independent and unchanged.
- V1/V2 records are unsupported and never migrated or decoded as V3.
- A/B slots, CRC32, sequence, commit-last and 2 KiB slot layout remain.

Stage 5K-C has only begun with semantic configuration comparison based on V3
canonical bytes. Broad legacy projection removal and larger register-model /
CommandService splits remain follow-up work; they are not claimed complete in
this status document.

## Software gate measurements

| Image | Flash text+data | RAM data+bss | ELF SHA-256 |
| --- | ---: | ---: | --- |
| Debug | 95,320 B | 19,900 B | `79C34006...44C839` |
| Release | 82,012 B | 19,868 B | `906ECBB3...5A7586` |
| BoardDiagnostics | 119,396 B | 19,772 B | `82E158A2...FA5784` |
| USART3 Bringup | 95,020 B | 19,516 B | `8054E753...47E90A` |

Compared with the Stage 5J reference, Release Flash decreased by 1,028 bytes
and BoardDiagnostics Flash decreased by 800 bytes. The decrease is primarily
the canonical codec and removal of the V1/V2 codec from the active ConfigStore
path; the legacy source declarations remain to be removed in the follow-up
cleanup commit after dependent test files are fully retired.

The eventual product identity recommendation is Firmware `0x0510`; this branch
does not change the reported firmware value and does not claim client or
hardware 0x0510 compatibility.

Current conclusion:

```text
STAGE 5K SOFTWARE READY; HARDWARE VALIDATION PENDING
```
