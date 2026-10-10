# Stage 5P unified software candidate gate

Date: 2026-10-10 Asia/Shanghai. This branch is a **software candidate in
progress**, not a qualified or official Release. No device was accessed or
flashed; no V3 configuration was migrated or saved.

## Baseline and patch

Independent worktree started at exact `578f7ce9d7923a98b477001645f7f75f6e504126`.
Patch SHA-256 matched `D119354ABA57B9BBBB4759DA352E10EF264E1DB0B33EC9835E2AC335A1ACFD3B`.
`git am` produced `ebac584af1a52160cb2bb4de2f63d495decfbacd`.
Subsequent audited fixes are source-only on this branch. The candidate identity
is **0x0524 / Map0x010C**, explicitly separated from the patch's unflashed
0x0522 and an intermediate 0x0523. The working contract remains
`Docs/STAGE5P_UNIFIED_DELIVERY_WORKING_CONTRACT.md`.

## Final software results

**PASS:** Host Debug/Release CTest **47/47 each**. The initially failing MSVC
`/WX` constant-mask warning was preserved in `software/host_debug_initial_warning_failure.log`
and fixed without changing the intended behavior. Strict ARM Debug/Release
GCC builds pass `-Wall -Wextra -Werror -fstack-usage -fcallgraph-info=su`.

**PASS:** frozen C/Python sample replay: A13B 88,765, A9 107,491, A13C-R
10,774, all zero mismatches. These are historical implementation regressions,
not a new 40 Hz efficacy holdout. Six synthetic A13C cases and the new 40 Hz
Host controller tests pass.

**PASS:** 40 Hz software adapter contract. The controller admits one sample
per four physical samples and keeps physical sequence/timestamp validation on
all samples. The 10 Hz A13C windows remain wall-clock equivalent: 15 s holdoff,
30 s reference, 20 s observation, 300 s boost. Physical invalid/fault/overload/
near-rail/FIFO error exits ACTIVE safely. Host tests cover obvious steps on a
slow IIR filter, 35 ug admitted-sample bound, 3500 ug/10 s bound, nonzero
DOSING freeze, missing period exit, all legal filter modes and strengths at
10/40 Hz. No 10 Hz data were upsampled into a qualification claim.

**PASS:** PLC ConfigEdit metrology commit now re-arms the saved R5 request after
a rebuild rather than leaving a half-rebuilt state. RAM commit increments
revision/dirty; no SAVE is issued automatically. The explicit battery offset
field supports old V3 retention and an owner-controlled RAM edit; the Host
test confirms calibration, address and units are retained and no SAVE occurs.
The migration is still **NOT RUN on a real device**.

**PASS:** battery UI logic and arithmetic tests. 4S correction is nominal
`+400 mV`; 12.8–16.8 V maps linearly to 0–100%. This is an approximate terminal
voltage indicator, not battery SOC or measurement precision. Low voltage uses
13.6 V/30 s warning and 13.9 V/10 s recovery; charging can make the estimate
optimistic. Idle dimming changes brightness only.

## ARM resource and image identity

| Build | RAM | Complete conservative stack | Collision margin |
|---|---:|---:|---:|
| 0x0524 Debug | 19,440 B | 1,552 B | **512 B** |
| 0x0524 Release | 19,384 B | 1,448 B | **672 B** |

The Debug margin is exactly the required minimum. No DMA, Modbus, BLE or stack
reserve was reduced. Source A13C math object SHA-256 is
`EF86E64F47B87F9900BD36108C1132447C1FB74CF2F23B30D83F073A21784F5F`.

Candidate Release BIN: **96,620 B**, SHA-256
`CBAB6D5E294D36F62231F9A233DF9E915E9F599D4B4826E6DC993A95035FFBF1`.
It is explicitly **UNFLASHED**. Ordinary Release remains byte-identical with
SHA-256 `C181AE5B916A69C6B120E712D52B575AA62E46B9F9F0C12786FFD3C8C2100CD9`.
Target MCU execution time, runtime high-water and 0x0524 40 Hz FIFO/sequence
conservation are **NOT RUN**.

## Historical 40 Hz evidence

`Results/stage5p_unified/software/historical_rate_review.json` audits eight
real 0x0519 / Map0x0104 profile captures. They predate the corrected S+/S−
wiring and are sparse host polls. Each 40 Hz 30-second file contains only 300
host rows covering about 25.1% of physical sequences, with 895–896 missing
sequences. Filter strength is not synchronized in these CSV fields. They are
historical context only, not current 0x0524 qualification.

## Hardware gate status

| Gate | Result |
|---|---|
| Current-device readback/config backup | **NOT RUN** |
| 0x0524 accurate flash/Verify | **NOT RUN** |
| New 10/40 Hz target resource timing | **NOT RUN** |
| Corrected-wiring 10/40 Hz physical records | **NOT RUN** |
| Multi-filter real-device records | **NOT RUN** |
| Menu/PLC SAVE and real power-cycle migration | **NOT RUN** |
| Battery low-voltage/charger/idle bench check | **NOT RUN** |
| PA14 long-rate comparison | **NOT RUN** |
| A13B long-duration efficacy | **INCONCLUSIVE**, unchanged |

No current device identity, mode, configuration SHA or final device mode can be
reported from this worktree. Do not infer them from R4/R5 history. Before any
flash or migration, run a fresh COM5 preflight, full application/V3 backup and
candidate-specific target diagnostic. Preserve all failures and stop on any
resource, sequence, profile, SAVE, or safety violation. Existing V3 data remain
the authority until an explicit, separately verified migration and SAVE.

**Overall: SOFTWARE CANDIDATE PASS; STAGE 5P UNIFIED DELIVERY INCOMPLETE;
UNFLASHED.**
