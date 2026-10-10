# Stage 5P unified focused hardware validation

Date: 2026-10-11 (Asia/Shanghai)

## Stop point

This validation stopped before COM5/SWD access. The required target-resource
gate could not be produced for the 0x0524 candidate: the only available
cumulative resource instrumentation (A13D-R) was built as an isolated
diagnostic attempt and overflowed the 124 KiB application FLASH region at
link time. No diagnostic image, timing evidence, runtime stack watermark, or
40 Hz target FIFO evidence therefore exists for this candidate. The old
0x0521/A13D-R results are not substituted for this gate.

Because the resource gate is a hard prerequisite, no device preflight,
application/configuration backup, erase, flash, Verify, SAVE, migration,
power-cycle, load/unload, profile or battery test was performed in this
stage. The device's current identity and final state are consequently
**NOT READ / UNKNOWN IN THIS WORKTREE**.

## Software baseline and candidate

- Worktree baseline and starting HEAD: `7a88c55d2d17de488a1931841bf692afbb850d8e`.
- Branch: `stage5p-unified-delivery-candidate` (already synchronized with its
  remote at the start of this stage).
- Candidate: Firmware `0x0524`, Map `0x010C`.
- Candidate BIN: 96,620 bytes,
  SHA-256 `CBAB6D5E294D36F62231F9A233DF9E915E9F599D4B4826E6DC993A95035FFBF1`.
- Ordinary Release remains byte-identical,
  SHA-256 `C181AE5B916A69C6B120E712D52B575AA62E46B9F9F0C12786FFD3C8C2100CD9`.

The existing software gate is retained in
`Docs/STAGE5P_UNIFIED_SOFTWARE_GATE.md`. Its Host, frozen replay, ARM and
candidate-image results remain unchanged:

- Host Debug/Release CTest: 47/47 each.
- A13B 88,765, A9 107,491 and A13C-R 10,774 historical replay samples:
  zero C/Python mismatches.
- ARM Debug: 19,440 B RAM, 1,552 B conservative stack, 512 B collision
  margin. ARM Release: 19,384 B RAM, 1,448 B stack, 672 B margin.
- These are software and historical regression gates, not new hardware
  qualification or drift efficacy evidence.

## Target-resource attempt

The attempt used the repository's A13D-R cumulative diagnostic sources with
the unified candidate feature set retained (`A33_ENABLE_STAGE5P_UNIFIED=ON`,
`A33_ENABLE_STAGE5PA13E_ACTIVE=ON`, `A33_ENABLE_STAGE5PA13C_SHADOW=ON`,
`A33_ENABLE_STAGE5PA13DR_RESOURCES=ON`). This was a build-only check; the
resulting diagnostic was never installed.

Result: **FAIL / NOT FLASHABLE**. The final link reported:

```text
RAM used:   19,416 B (94.80% of 20 KiB)
FLASH used: 180,856 B (142.43% of 124 KiB)
FLASH overflow: 53,880 B
```

The unlinked diagnostic has no valid BIN, no valid map-based stack collision
margin, and no target execution-time or runtime-watermark evidence. The
complete build log is retained at
`Results/stage5p_unified/software/target_resource_diag_build.log`.

This failure is not a failure of the uninstrumented 0x0524 BIN's measured
software gate; it means the required target-side resource evidence was not
available without a new, bounded diagnostic design. The diagnostic cannot be
used as a candidate image and was not burned.

## Hardware gates

| Gate | Result | Evidence |
| --- | --- | --- |
| Current-device readback and full application/V3 backup | NOT RUN | Hard-stopped before device access |
| Candidate 0x0524 flash/Verify and config preservation | NOT RUN | No image-qualified resource gate |
| 40 Hz normal Modbus target timing/FIFO/sequence | NOT RUN | No valid target diagnostic |
| 10/40 Hz and filter representative load/unload | NOT RUN | No flash performed |
| Non-zero offset STATIC/DOSING and re-建参 | NOT RUN | No flash performed |
| One-time V3 migration/SAVE and real power cycle | NOT RUN | No device state changed |
| Menu/PLC SAVE and cancellation behavior | NOT RUN | No device state changed |
| Battery percentage, low-voltage and idle-dimming bench checks | NOT RUN | Hardware prerequisites not reached |
| A13B long-duration efficacy | INCONCLUSIVE | Unchanged historical conclusion |

## Required next step

Design a bounded target diagnostic that fits the 124 KiB application image
and preserves at least 512 B static/runtime stack margin, then rerun the
target-resource gate from a new, uniquely identified diagnostic build. Only
after that gate passes should a fresh COM5 preflight, complete SWD backup and
the 0x0524 focused hardware sweep be considered. No configuration migration
or persistent default change is authorized by this stopped run.

## Conclusion

`SOFTWARE CANDIDATE PASS; TARGET RESOURCE GATE FAIL/INCOMPLETE; HARDWARE VALIDATION NOT RUN; UNFLASHED`.

The candidate must not be called a qualified Release or metrology-certified
firmware. A13B long-duration drift efficacy remains `INCONCLUSIVE`; 40 Hz,
other filter combinations, cross-sensor qualification and formal metrology
remain deferred.
