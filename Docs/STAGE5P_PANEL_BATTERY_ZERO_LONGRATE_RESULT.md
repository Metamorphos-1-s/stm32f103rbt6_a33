# Stage 5P panel, battery, zero and long-rate candidate result

Date: 2026-10-13 Asia/Shanghai
Patch SHA-256: `6839558DE81DD39D4910154541AF1F30D49938C17CA7402AA2BBDB4FCCE45945`

## Patch migration

The patch was based on `054bfab3`, while this worktree started from the
0x0527 battery-transaction final commit `55f6653ddf7b269f5d042e10610d72291df2bf66`.
SHA and `git apply --check` passed; every hunk applied cleanly. No forced or
fuzzy application was used.

| Patch area | Result |
| --- | --- |
| NET -> GROSS -> BATTERY -> NET page cycle | Migrated |
| NET/GROSS stable display-anchor transfer | Migrated; focused C test added |
| 5 s battery confirmation | Migrated; existing battery C test updated |
| 20 s / 4 s `Lo-bAt` window | Migrated |
| Default zero range ±2 g | Migrated as new-default behavior only |
| Stable approximately-one-division zero indicator | Migrated; additional 1 s display hysteresis added after hardware diagnosis |
| 10 ug long-rate | Migrated |
| Existing 0x0527 battery transaction | Preserved; no V3 layout change |
| Formal checkweigh LED/alarm paths | Preserved; no removal of LOW/OK/HIGH output logic |
| Current sample zero-range migration | Not migrated: sequence 21 remains ±60 g; no reliable public field-write path and no automatic overwrite |

## Software gates

The final candidate is a new engineering identity `0x0529 / Map 0x010F`.
Public Modbus addresses were unchanged, so the map remains 0x010F.

Host LLVM-MinGW Clang 23.1.3 Debug and Release focused tests passed:

```text
stage5p_unified_defaults_and_startup: 1/1 PASS
display_conditioner_logic: 1/1 PASS
```

The tests cover NET/GROSS anchor transfer, actual stable/unstable input
behavior, 5-second battery confirmation and zero-indicator hysteresis. The
full historical 47-test suite remains INCOMPLETE due to unrelated existing
menu `-Werror` issues in unbuilt targets.

ARM strict Release build passed with `-fstack-usage`:

```text
RAM: 19,424 B / 20,480 B
Flash: 97,876 B / 126,976 B
_ebss: 0x200047E0
conservative collision margin: 544 B (after the established 512 B allowance)
BIN: 97,876 B
BIN SHA-256: 85216447B5CB1701AFCADD947891111298FAA52E4F05DF89B97552A46FBD5786
ELF SHA-256: 3283F48A36820DB49EF00E5F03440E4182F980BD0C8026965F10FC4CE17117D1
```

134 `.su` files were generated. They are compile-time frame evidence, not a
runtime stack high-water measurement. No dynamic allocation or V3 payload
layout change was introduced.

The opened A13 development/replay data were run through the 10 ug candidate
using the real C replay runner:

| Source | Samples | C/Python mismatches | Max/sample | Max 10 s | DOSING changes | Step loss |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| A13B | 88,765 | 0 | 35 ug | 3,465 ug | 0 | 0 ug |
| A9 | 107,491 | 0 | 35 ug | 3,465 ug | 0 | 0 ug |
| A13C-R | 10,774 | 0 | 35 ug | 3,465 ug | 0 | 0 ug |

These are development/regression replays, not independent long-duration
efficacy qualification. A13B remains `INCONCLUSIVE`.

## Current sample and hardware

Before the new image, 0x0528 was read as valid with sequence 21, revision/saved
21/21, battery configuration preserved and a small nonzero volatile offset.
Fresh backups were taken before flashing:

```text
application_before SHA-256: B3C755D46C941F819869AD06343BE97C7112C379913963F704CCD2254E4ACA9C
config_before SHA-256:      7E2B19D04B2876280448B12D8C3D5B812C501C22C80F69B88D1F49DE44F1220B
```

Only application sectors 0..123 were erased/programmed. Verify passed. After
0x0529 boot:

```text
Firmware/Map: 0x0529 / 0x010F
revision/saved: 21/21
dirty/fault/overrun: 0/0/0
profile: 10 Hz / filt1 / strength3
calibration: valid
R5: ACTIVE+STATIC, offset 0 after reset
Checkweigh: OFF
config SHA: 7E2B19D04B2876280448B12D8C3D5B812C501C22C80F69B88D1F49DE44F1220B
```

## Hardware observation status

The user's earlier observations were made on 0x0528, not this 0x0529 image:
FUNCTION page cycle normal, 12.7 V `0P` with flashing battery LED and
intermittent `Lo-bAt`, and 14.45 V recovery to `40P` without warning. They are
retained as historical engineering evidence and are not relabelled as 0x0529
PASS. A fresh 0x0529 visual pass is **NOT RUN** in this handoff.

The current sample's saved zero range decodes as `60,000,000 ug` (±60 g),
while the new-device default is `2,000,000 ug` (±2 g). No automatic migration
was performed. ZERO key/PLC semantics and startup auto-zero code were not
changed by this patch, but current-sample persistence remains old.

## Final status

The device currently retains the 0x0529 application and sequence 21 battery
configuration; no SAVE or configuration write was performed in this stage.
The current volatile R5 offset was cleared by flashing and is rebuilding from
zero. Full long-rate efficacy, 40 Hz matrix, 500 g qualification, formal
metrology and fresh 0x0529 visual observations remain incomplete.
