# Stage 5M-R5E Local Runtime Control

## Scope and status

R5E starts from R5D head
`06adb202b06f7fa64b40594019f0a32c87456796`. It adds only a local advanced-menu
runtime control entry and a read-only STATUS entry. It does not modify the R5
algorithm, parameters, input path, filtering, DeviceConfig, Persistent Format
V3, or Stage 5M-F.

Current status: **STAGE 5M-R5E LOCAL RUNTIME CONTROL READY; R5D LONG-DURATION
SAFETY EVIDENCE PRESERVED; R5 REMAINS DEFAULT-OFF ENGINEERING BETA; STAGE 5M-F
ENTRY REMAINS APPROVED**.

## Product contract

R5E Beta identifies as Firmware `0x0512`, Map `0x0104`, Public Schema 2 and
Persistent Format 3. Standard Release remains Firmware `0x0510` and its ELF is
byte-identical at SHA-256
`82E726F5B32A0DE36A5E686F62A937EC4FD9CBB488DB9733E83D2062673EF486`.

The Beta-only advanced item `drIFt` has four choices:

| Display | Safe command order | Result |
|---|---|---|
| `OFF` | application SHADOW, then mode OFF | clear volatile offset/reference/windows |
| `SHAdO` | application SHADOW, then mode STATIC | learn without changing official weight |
| `StAtIC` | mode STATIC, then application ACTIVE | learn and apply the current offset |
| `doSInG` | mode DOSING, then application ACTIVE | freeze and apply the existing offset |

FUNCTION short confirms only a volatile candidate. STAR/HASH only browse.
FUNCTION long performs the transition through CommandService. TARE and the
30-second menu timeout cancel without a runtime or configuration write. A menu
session records the public R5 pair at entry; a PLC/PC change before apply yields
`bUSY` and is not overwritten. A DeviceConfig candidate blocks entry to the R5
editor, while an R5 candidate blocks ordinary edits. No PersistenceManager API
is linked into the coordinator.

Two-step failure handling attempts to restore the entry pair. If restoration
fails it attempts OFF+SHADOW. Every failed transition increments a volatile
diagnostic counter and displays an error; success alone displays `donE`.

The Beta-only STATUS item `drSt` reads `COMMAND_R5_GET_STATUS` every time its
value is rendered. It displays `OFF`, `doSInG`, `HOLd`, `rEF`, `ObS`, `trAC`,
or `LInIt`. `LInIt` is the seven-segment-safe spelling for LIMITED because the
font has no M glyph. STATUS never executes a mode command or persistence call.

## RAM and stack

R5D used 12 bytes `.data`, 18,376 bytes `.bss`, and 19,416 linker RAM bytes.
R5E uses 12 bytes `.data`, 18,392 bytes `.bss`, and 19,432 linker RAM bytes.
The local coordinator accounts for 12 bytes; alignment makes the total delta
16 bytes. Unallocated RAM after the unchanged 1,024-byte minimum stack is
1,048 bytes. Static end to `_estack` is 2,072 bytes.

The maximum resolved main chain remains 1,128 bytes and the maximum IRQ
software chain remains 72 bytes. The existing conservative stack bound is
1,488 bytes, leaving a 584-byte collision margin. This exceeds the mandatory
512-byte gate. DMA, Modbus, BLE and queue capacities are unchanged, heap remains
zero, and no dynamic allocation exists.

## Freeze and software gates

The C implementation, header and Python model retain exactly the same Git blobs
as R5D. Python/C parity remains 25,057 samples, including 22,557 real samples,
with zero mismatch. Host CTest is 28/28. R2/R3/R4/R5/R5D, Stage 5B/5C/5L and
Manifest tests pass. Debug, Release, R5E Beta and R5E Beta `-Wextra -Werror`
builds pass. ASan and UBSan are **NOT RUN** because the portable MinGW runtimes
are unavailable.

The R5E Beta BIN is 100,792 bytes with SHA-256
`BA8F02B2024042D601FD7F2D75BEF9E1004AACAE16852DD97CD2B28777BAF6B9`.
The ELF is 1,954,668 bytes with SHA-256
`FE5444CD2DA50E1F58BF8E6BF95F8BC53255628805B816A4A271493E5500485F`.
Hardware reflash and supervised local-key closure are mandatory because the
Beta binary changed. The final application-only reflash and Verify passed at
3.29 V without changing the configuration SHA.

## Hardware closure

The 4,438.785-second supervised workflow passed the advanced entry, `drIFt`,
all four public selections, TARE and timeout cancellation, short-confirm versus
long-apply separation, external-control refresh, and an intentionally induced
external conflict that displayed `bUSY` without overwriting the PLC state.
`drSt` directly displayed `rEF`, `ObS`, and `trAC`. HOLDOFF was captured by PC
diagnostics immediately after transition, but its 15-second panel value was not
directly observed: both operator view attempts reached `rEF` after the state
advanced. This limitation is explicit and is not reported as a direct panel
PASS; the production state-to-text mapping and glyphs are Host-tested.

A 960-second SHADOW run reached TRACKING with 300/600 fill and zero errors. A
360-second ACTIVE+DOSING load/unload run held offset exactly at `18,619 ug`.
Both uncompensated and corrected steps were `500.096877 g`, giving zero step
loss. Loaded TARE and CLEAR TARE preserved the offset and revision. Empty ZERO
cleared offset/reference/windows. Fault, overrun, dirty and SAVE remained zero,
revision/saved revision remained `7/7`, and the configuration SHA remained
`D74C98D8D4221437773155E8D1ED75BC59D71AE2D285D5C2F18F6B494AA5DC86`.
Final reset state is OFF+SHADOW with offset/reference/evaluation zero.

R5D long-duration safety evidence remains applicable because algorithm,
parameters and input path are frozen. Its efficacy result remains
**INCONCLUSIVE DUE TO LOW NATURAL DRIFT** and its display-level correction is
only partially effective. Cross-sensor validation and metrology certification
remain deferred.
