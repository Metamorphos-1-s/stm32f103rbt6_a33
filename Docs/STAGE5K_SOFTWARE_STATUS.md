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

Stage 5K-C includes a shared pure `DeviceConfig` validator and semantic
configuration comparison based on V3 canonical bytes. Broad legacy projection
removal and larger register-model / CommandService splits remain follow-up
work; they are not claimed complete in this status document.

## Software gate measurements

| Image | Flash text+data | RAM data+bss | ELF SHA-256 |
| --- | ---: | ---: | --- |
| Debug | 95,452 B | 19,912 B | `3D3DF57F63F526101228B8196E6089DC5757E6512C9B9A6B7BDF6B707D853BF2` |
| Release | 82,184 B | 19,880 B | `1FD11414C0CAD4B8472BCD46C26B83A715D847D62192422302C6F1372545664D` |
| BoardDiagnostics | 119,736 B | 19,784 B | `CE2AB0BBB1CCBC37F745080AAA7588AC966328ADAA755AEFB8E169455E98065B` |
| USART3 Bringup | 95,152 B | 19,528 B | `942277A766CA7D2DD755DB3040FE1FEE6ECF417DA89D02A6F9DA8A29FFAA28FC` |

Compared with the Stage 5J reference, the active ConfigStore path remains
smaller despite the validator and diagnostic additions. Historical codec
source declarations remain to be removed in the follow-up cleanup commit
after dependent test files are fully retired.

The eventual product identity recommendation is Firmware `0x0510`; this branch
does not change the reported firmware value and does not claim client or
hardware 0x0510 compatibility.

Current conclusion:

```text
STAGE 5K IMPLEMENTATION COMPLETE; PORTABLE HOST VERIFICATION PENDING
```
