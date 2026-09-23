# Stage 5P-A1 40 Hz Target-Rate Root-Cause Closure

## Final Decision

**STAGE 5P-A1 ROOT CAUSE CONFIRMED; MODBUS THROUGHPUT OPTIMIZATION CLOSED;
R5 PROFILE-SWITCH RATE COMPATIBILITY INCOMPLETE; 40 HZ REMAINS BLOCKED;
STAGE 5P-A2 NOT APPROVED.**

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

## Hardware Evidence

The configuration region was backed up before flashing. Both slots were valid
V3 records (A sequence 7, B sequence 8 active). Its SHA-256 was
`A615A475C2D7B5D64126EAEB3AAE9E3D094348FA4C4AEC7BDB8EF0010D4A3BB4`.
Both diagnostics were programmed application-only and verified.

| Baseline load | Rate | App_Run >25 ms | >50 ms |
|---|---:|---:|---:|
| none | 39.933 Hz | 0 | 0 |
| q1 | 39.967 Hz | 0 | 0 |
| q10 | 39.919 Hz | 0 | 0 |
| q27 | 28.017 Hz | 520 | 0 |
| q32 | 24.681 Hz | 457 | 6 |
| q40 | 22.740 Hz | 375 | 375 |
| normal five blocks | 27.600 Hz | 372 | 124 |

The q27/q32/q40 register-model averages were approximately 2.807/3.325/4.213
million cycles. In every run, ready/read/push/pop/bridge/engine counts matched;
there was no FIFO loss. This confirms that synchronous repeated conversion
prevented timely DRDY observation and reproduces the original 26.7-29.2 Hz
range under the normal five-block workload.

With the request-level read view, the same normal workload produced 39.949 Hz
at 40 Hz/filt0. q27/q32/q40 fell to about 0.006/0.110/0.048 million cycles and
there were no App_Run executions above 25 ms. The complete matrix passed:

| Rate | filt0 | filt1 | filt2 | filt3 |
|---|---:|---:|---:|---:|
| 10 Hz | 10.000 | 9.983 | 9.983 | 9.983 |
| 40 Hz | 39.949 | 39.933 | 39.933 | 39.933 |

All eight runs had matching acquisition/processing counts and zero Modbus
errors, read errors, overrun, or fault. CS1237 readback was `0x0C` at 10 Hz and
`0x1C` at 40 Hz.

R5 SHADOW+STATIC at 40 Hz spent 15 real seconds in holdoff and reached
reference fill 20 after a 35-second run. It never became LIMITED. A 30-second
DOSING run held offset exactly at zero. However, both 40-to-10 and 10-to-40
profile transitions entered TIMESTAMP LIMITED. Investigation found that
`MetrologyManager_Reconfigure()` rebuilt WeightEngine and reset its sequence
without issuing the R5 profile-change event. The hard-stop rule was applied.

A minimal post-failure software correction now issues that event after a
successful rebuild. It passes Host 36/36, strict ARM build and the unchanged
RAM/stack gates, but was not reflashed or reused to claim hardware success.
The corrected, software-only BIN is 107,532 bytes with SHA-256
`2101D1F344B63AA9A6983B56A90EBFF3AF8167B838B506611776D71C37D3C6AA`.

The device was application-only rolled back and verified. Final state is
0x0517, Map 0x0104, OFF+SHADOW, Checkweigh OFF, offset/reference/evaluation
zero, fault/overrun/dirty zero, revision/saved 8/8. The configuration SHA before
testing, at hard stop, and after rollback is identical. No SAVE was executed.
