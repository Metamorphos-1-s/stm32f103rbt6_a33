# Stage 5P-A1 40 Hz Target-Rate Root-Cause Closure

## Current Decision

**SOFTWARE AND DIAGNOSTIC CANDIDATES READY; TARGET ROOT-CAUSE CLOSURE
PENDING AUTHORIZED HARDWARE EXECUTION.**

This stage is not a release and does not claim metrology, cross-sensor,
external DRDY timing, or new 12-hour qualification.

## Baseline

The branch starts from `d91c79003baed98b07c1388e30dcf01f65afc05f`.
The product-path comparison against the Stage 5P-A capture commit
`74218f89f5f79c2b0395375403cd6493d86776a8` found no relevant data-path
change. Only a later removal of a redundant `app_main.c` startup guard differs;
the Modbus, communication, CS1237, measurement, conversion, and R5 sources used
during capture are otherwise identical.

The frozen 0x0517 rebuild remains 107,084 bytes with SHA-256
`9D8C5881CD880D2C5D6A7D0C499A4EACF9495A550B08B237A81D4BB2F71641C5`.
The historical formal Release SHA remains
`82E726F5B32A0DE36A5E686F62A937EC4FD9CBB488DB9733E83D2062673EF486`.

## Independent Code Findings

The original `ModbusRegisterModel_ReadHolding()` calls `ReadOne()` once per
register. Before dispatching the address, `ReadOne()` converted panel, NET,
GROSS, and TARE through `UnitConverter_MassToDisplay()`. The normal five-block
monitor reads 110 registers, so it executed 440 display conversions per cycle.
Historical frames show approximately 62-63 ms for quantity 32 and 78 ms for
quantity 40, while a 40 Hz converter period is 25 ms. `CS1237_Process()` runs
once at the start of `App_Run()`, so synchronous register processing can defer
the next ready observation. This is a supported hypothesis, but the required
autonomous target matrix must still establish causality.

R5's independent defect is confirmed. The one-second array accepts at most 16
samples and the 17th calls `EnterLimited(...TIMESTAMP)`. The Stage 5P-A 40 Hz
captures reported state LIMITED and reason TIMESTAMP. This does not explain the
26.7-29.2 Hz acquisition rate, but it prevents R5 from operating at a genuine
40 Hz rate.

## Code Changes

The optimized Modbus build creates one request-local read view. A basic 32-word
request performs four display conversions total; 0x0020/27, 0x01C0/10 and
0x0103/1 perform none. The D1-C diagnostic block converts only its selected
NET or GROSS value. No address, response layout, word order, exception, or
cross-request cache changed. The original implementation remains selectable in
the baseline diagnostic build and remains compiled for frozen 0x0517.

The product R5 path admits the last sample in each 100 ms slot relative to the
current one-second bucket. Exact 10 Hz input therefore preserves its ten
samples, while 40 Hz is bounded to ten representatives without enlarging the
16-element array. Timestamp wrap, jitter, sequence gaps, DOSING freeze and
10/40 transitions are covered. Product profile/filter changes preserve offset,
clear learning and admission windows, and restart holdoff; frozen 0x0517 keeps
its prior behavior.

## Software Gates

- Debug and Release ARM builds: PASS.
- 0x0519 candidate and both full-feature diagnostics builds: PASS.
- ARM `-Wall -Wextra -Werror`: PASS.
- MSVC `/W4 /WX` Host CTest: 36/36 PASS.
- Legacy R5: 25,057 samples, 22,557 real, zero mismatch.
- R5 10/40 admission: 95,200 synthetic samples, zero mismatch.
- D1-D: 49,533 samples, zero mismatch.
- Stage 5N-A: 45,711 samples, zero mismatch.
- Stage 5B/5C/5L Python: 3/3, 12/12, 8/8 PASS.
- Dynamic allocation in product source: none.
- Persistent Format 3, Map 0x0104, Schema 2: unchanged.

Product RAM is 19,464 bytes. Static-to-estack space is 2,040 bytes,
conservative stack is 1,504 bytes, and collision margin is 536 bytes. The
diagnostic build uses 19,744 bytes and is not used as the product RAM result.

## Artifacts

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| 0x0519 candidate BIN | 107,516 | `74992D2F8C0537DFAD06C12C878C6791FD041C3F1798B4CC0981EDA2E5A0BB82` |
| 0x0519 candidate ELF | 2,024,140 | `07026CC12DE030445F8D72EB2980C2D8FD8CAB8539DEC81EC76888C3295A0E17` |
| 0x0519 candidate MAP | 1,444,919 | `D0854C49A9598A725A7FBAF3E37FFB725B48CAAFDAE72D8446E136AB6529086C` |
| baseline diagnostic BIN | 109,008 | `81A601EE1618F5EE41E3C40F3AD54A57289B9F2E0CDB29E7EAC2FAC2F4F5823E` |
| optimized diagnostic BIN | 108,808 | `01B39FC5CCF7DEA7CF0EDEDA146EC510A512740A08D99AF6BE54EF86EB58D057` |

Both diagnostic builds retain the complete product feature set. They differ
only in the request-view optimization switch and include SWD-only DWT counters
outside the product RAM result.

## Hardware Plan And Stop Point

The current Modbus-only preflight confirms 0x0517, Map 0x0104, 10 Hz, R5
OFF+SHADOW, offset/reference/evaluation zero, fault/overrun/dirty zero,
revision/saved 8/8, and approximately 500 g still loaded.

No SWD configuration read or flash was performed. After explicit authorization,
the configuration region will be backed up first. The baseline diagnostic then
runs the no-load, q1, q10, q27, q32, q40, and normal five-block matrix before
the optimized diagnostic is tested. The diagnostic image occupies application
pages 0-106 (`0x08000000-0x0801ABFF`); configuration starts at `0x0801F000`
and is excluded. Every flash uses Verify. The final default action is a verified
application-only rollback to frozen 0x0517 and an unchanged configuration SHA.

Hardware execution is waiting for the exact authorization:

`授权读取配置并烧录5P-A1诊断固件`
