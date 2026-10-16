# Stage 5P 0x0530 software review closure

**SOFTWARE READY FOR FOCUSED HARDWARE QUALIFICATION; HARDWARE NOT RUN.**

This revision supersedes the remaining software limitations in
STAGE5P_052F_DISPLAY_ANCHOR_SOFTWARE.md. It does not supersede any physical
panel or metrology result. Starting point: 65ad337a81b39c50eea6cda5a4614eed11a179e1,
PR #1, branch stage5p-display-anchor-follow-v3.

## Review findings and changes

1. **A stale median could move the anchor away from current weight.**
   With anchor 500.60 g, sixteen 40 Hz samples at 500.68 g leave the fast
   timer just short of expiry. A current sample at 500.52 g is only 8e
   from the old anchor, but the old median can still be 500.68 g. Following
   it on that sample gives 16e error. The new pre-commit check rejects a
   median more than 8e from current desired weight and resets its evidence.
   The added 69-scenario suite reproduced one failure before this fix.

2. **Candidate qualification could lock onto an outdated ramp window.**
   Quiet adjacent steps do not establish that the whole window is quiet.
   Steps of 8e per sample can leave the nine-point median 32e behind at
   candidate-to-LOCKED transition. The new transition check follows the
   current value in TRACKING when the proposed median is more than 8e old.
   Positive/negative ramp and deterministic walk cases exercise divisions
   1/2/5. Before this check, the extended suite had 387 failing bound
   assertions; with both fixes, all 78 scenarios pass.

3. **Complete strict Host build was blocked by conditional/dead code.**
   Join32 is now compiled only with its battery-transaction callers.
   The never-read menu variable s_original_unit and its assignment were
   removed. The inherited -Wno-unused-but-set-variable exceptions were
   removed from unified/SAVE test targets. Strict warnings remain enabled.

4. **The build comparison used two different GCC distributions.**
   The historical ELF identifies GNU Tools for STM32
   13.3.rel1.20240926-1715. The earlier reconstruction used Arm GNU Toolchain
   13.3.Rel1 (Build arm-13.24). Both report GCC 13.3.1, but have different
   compiler/runtime builds. The exact ST distribution has now reproduced
   the historical binaries below, closing this discrepancy.

5. **The recorder needed an explicit current port and byte interpretation.**
   The existing display recorder now accepts --port and --word-order,
   preserves unsigned sequence/tick values, removes an unused FC03 block,
   identifies its snapshots as non-atomic, and stops on an identity change.
   Contract tests use a fake transport and verify high/low word order,
   signed values, unsigned counters, selected port and read-only requests.

Runtime changes are confined to display anchor checks and the firmware
identity (0x0530, Map 0x010F). No sampling/filter/R5/checkweigh/alarm logic,
configuration schema, calibration model or Flash layout is changed.
The state structure has not grown. No device was accessed.

## Final software gates

| Gate | Result |
| --- | --- |
| Complete strict native Host Debug build + CTest | 51/51 PASS |
| Complete strict native Host Release build + CTest | 51/51 PASS |
| Display behavioral suite | 78 scenarios, 0 failures |
| Unified Python contract suite | 17/17 PASS; fake transport only |
| ASan + UBSan display suite | PASS; leak inspection disabled due to executor limits |
| Clean ST ARM Release builds in two directories | Byte-identical |
| Static RAM/stack gate, including audited IRQ nesting | PASS |
| Hardware COM5/SWD/panel/droplet tests | NOT RUN |

The CTest matrix includes historical feature-gated targets and the two
candidate-enabled startup/SAVE integration targets. It is not a claim that
every historical target is compiled with the new display flag.

Canonical compiler:
arm-none-eabi-gcc (GNU Tools for STM32 13.3.rel1.20240926-1715) 13.3.1 20240614.

The ST official bundle is gnu-tools-for-stm32 13.3.1+st.8, x86_64-linux,
SHA-256 7D6424FE4BD8F092898FEB24A3DD06191846A8BC19D61C245DCB0172DBBDAB7F.
The original package metadata and runtime library hashes are archived.
Historical ELF .comment and linker map paths identify the same ST build.

| Image | Bytes | SHA-256 |
| --- | ---: | --- |
| Historical 0x052E, exactly reconstructed with ST | 98,980 | AC13730228B15E5C9EA031D64795E28CF589986EEB18F63B6F056396346ED7D9 |
| Display flag OFF, historical 0x052C exactly reconstructed | 98,644 | E87C5FCFDCC4FE8800725741AFE39BA8378FDE917A7C45A4453BA4CBE8826DD6 |
| 0x0530, two independent clean canonical builds | 98,796 | 8ADB103085629F9A0F645CA25F324C6B53DDB395C714E62D1C0306ED7CBCBA34 |

Canonical 0x0530 RAM is 19,432 / 20,480 B including the linker-reserved
1024 B stack. The conservative callgraph bound including IRQ nesting is
1464 B; the physical collision margin is 608 B, above the required 512 B.
CONFIG_A/CONFIG_B linker use remains zero. This is static analysis, not a
runtime stack watermark. An additional Arm-distribution check also built
successfully, but its different BIN is not the canonical hardware image.

## Replay boundary

Same retained, sparse, non-atomic snapshots; no interpolation:

| Input record | Rows | Frozen 0x052E replay changes | 0x0530 replay changes |
| --- | ---: | ---: | ---: |
| 0x052C mixed transitions | 1517 | 51 | 67 |
| 0x052D loaded record | 965 | 53 | 37 |
| 0x052E loaded record | 979 | 31 | 26 |

These are descriptive software replays of about 8 Hz Host records, not
complete 40 Hz streams, not actual LED readings, and not new hardware
qualification. The transition-rich record legitimately has more following
changes; no blanket reduction gate is claimed.

## Reproduction and handoff

The added Stage5PDisplayReview preset freezes the complete candidate flags.
The software-only driver refuses the wrong GCC distribution:

    python Tools/stage5_unified/qualify_display_0530.py --arm-bin <ST-bin> --host-cc <native-compiler> --cmake <cmake> --ninja <ninja> --output <new-directory>

It builds the whole strict Host matrix in Debug/Release, runs Python
contracts, builds ARM twice from clean directories, compares BINs, checks
RAM/stack, and records compiler/runtime identities and commands.
ASan/UBSan, historical-image reconstruction, disabled-feature reconstruction
and retained-record replays are separately archived checks.

Evidence: Results/stage5p_0530_software_review/software/.
The manifest hashes stored evidence bytes and the canonical Git blobs of
changed source files, avoiding Windows CRLF checkout differences.
ELF/map are losslessly gzipped; the manifest includes decompressed hashes.

Executable local instructions:
Docs/STAGE5P_0530_LOCAL_CODEX_PROMPT.md.

The next dependency is actual equipment: fresh link/target identification,
protected application-only programming, full readback, then observed static,
load/unload and one-drop-per-second tests with 2–10 s pauses. No product
merge or physical acceptance is claimed here.
