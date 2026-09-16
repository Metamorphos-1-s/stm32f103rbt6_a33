# Stage 5M-R5C Engineering Use and RAM Safety

## Baseline and scope

R5C starts from the pushed R5B freeze
`16e5b051a26dd104475c4de2e4987347032da04d` on the independent branch
`stage5mr5c-engineering-use-ram-safety`. R5 parameters and the 10-second
block-median reference-lock algorithm are unchanged. Stage 5M-F is not
implemented here.

The standard Release, R5 Beta and Host builds remain separate. The standard
Release includes the historical `RuntimeDriftCompensator`, defaults it off and
remains byte-identical. The Beta build includes `R5DriftCompensator`, removes
the unused old runtime-drift instance, and makes the old enable API reject the
request. The historical `StaticDriftCompensator` remains Host-only. At most one
drift offset can affect weight.

## Engineering operation contract

| Public control | Internal state | Calculates | Updates offset | Applies offset | Main display/PLC weight | Reference and offset behavior |
|---|---|---:|---:|---:|---|---|
| OFF | OFF | No | No | No | Uncompensated product path | Clears volatile offset and learning windows |
| SHADOW + STATIC | HOLDOFF | State only | No | No | Uncompensated product path | Keeps offset; 15 s holdoff then rebuilds reference |
| SHADOW + STATIC | REFERENCE_FILL | Yes | No | No | Uncompensated product path | Builds 300 s reference from 30 ten-second medians |
| SHADOW + STATIC | OBSERVATION_FILL | Yes | No | No | Uncompensated product path | Builds 600 s rolling observation from 60 blocks |
| SHADOW + STATIC | TRACKING | Yes | Yes | No | Uncompensated product path | Evaluates every 60 s; reference locked |
| ACTIVE + STATIC | TRACKING | Yes | Yes | Yes | Corrected gross enters ZERO/TARE/NET/display/PLC | Keeps locked reference and bounded volatile offset |
| SHADOW/ACTIVE + DOSING | DOSING | Diagnostic only | Strictly frozen | ACTIVE only | SHADOW uncompensated; ACTIVE frozen correction | Clears learning windows, preserves offset |
| STATIC load step | HOLDOFF | Step guard | Frozen | Per SHADOW/ACTIVE | No algorithm step added | Preserves offset; rebuilds reference |
| Invalid calibration/fault/overload/near rail | LIMITED | No | Frozen | Per SHADOW/ACTIVE | Immediately readable diagnostics | Preserves offset and reports reason |

ZERO clears the R5 offset and learning state. TARE, clear-tare and unit changes
preserve the common drift offset. Calibration begin selects OFF; calibration
commit clears offset and reference. Profile changes enter LIMITED. Power-on
starts OFF + SHADOW with offset zero. R5 state is not encoded into Persistent
Format V3 and cannot trigger SAVE or a configuration revision by itself.

The R5 input is calibrated, unquantized, uncompensated gross in micrograms.
Display-divided mass is never fed back. Raw ADC remains available for near-rail,
jump, rate and hardware diagnostics. Uncompensated gross remains readable in
every mode.

## Engineering recorder

`Tools/stage5mr5b_beta/r5_beta_hw.py record` extends the existing R5 hardware
tool rather than creating a second protocol client. It streams one row at a
time to CSV, raw frames and state events to JSON Lines, and atomically replaces
`summary.json`. Ctrl+C retains all completed rows. Read errors, reconnects,
host polling gaps and intentionally unobserved 10 Hz device sequences are
reported separately. The recorder is read-only and has no SAVE or Flash API.

The stream contains UTC, uptime, firmware/map/format, display and uncompensated
and corrected mass, mode/application/state, offset/reference/error/rate,
holdoff/fill/rebase/reason, fault/overrun/dirty/revisions, storage sequence and
the low 16 bits of ConfigStore SAVE request count. Mode, state and application
transitions are emitted as events. ZERO/TARE actions are visible through the
weight/status fields and can be paired with the raw Mailbox frame log.

## RAM closure

STM32F103RB RAM is `0x20000000-0x20005000` (20,480 bytes). The linker script
sets `_Min_Heap_Size=0`, `_Min_Stack_Size=0x400` and `_estack=0x20005000`.
R5B ended static allocation at `0x20004B20`; its reported 20,256 bytes therefore
already included the 1,024-byte minimum stack. The remaining 224 bytes were in
addition to that stack, not the entire stack.

R5C removes the unused old drift instance from Beta, shares the mutually
exclusive Menu/Status configuration workspace, and converts large nested
configuration/rebuild temporaries to bounded validated workspaces or in-place
commit paths. DMA, Modbus, BLE, queue and persistence buffer capacities are not
reduced. Standard Release preprocessing remains unchanged.

R5C Beta has 12 bytes `.data`, 18,376 bytes `.bss`, no `.noinit`, and static end
`0x200047D8`. Adding the unchanged 1,024-byte minimum stack consumes 19,416
bytes, leaving 1,064 bytes unallocated. Static end to `_estack` is 2,088 bytes.
This meets the 1 KiB unallocated-RAM objective without reducing the declared
stack.

Largest module RAM users include communication manager 2,613 bytes, metrology
manager 1,844, BLE transport 1,445, UART2 DMA 1,303, ConfigStore 1,303, BLE
command service 1,189, persistence manager 1,126 and UART3 Modbus 1,081. The R5
state is 840 bytes. The shared Menu/Status workspace is 689 bytes.

## Stack evidence

Runtime fill-pattern watermarking was **NOT RUN** because safely painting a live
bare-metal stack without an early-startup reserved region would overwrite the
active stack. Instead, a dedicated non-flashed build used GCC `-fstack-usage`
and `-fcallgraph-info=su` over all 128 translation units.

The largest resolved main chain is 1,128 bytes. The largest IRQ software chain
is 72 bytes. Adding one Cortex-M3 exception frame (32 bytes) gives 1,232 bytes.
An additional explicit 256-byte allowance covers unresolved function-pointer
targets, yielding a 1,488-byte conservative bound. Against 2,088 bytes from
static end to `_estack`, the collision margin is 600 bytes. All enabled UART,
DMA and TIM4 peripheral IRQs use preemption priority 5 and cannot preempt one
another.

A read-only SWD halt/register/run sample on the previous R5B image observed
MSP `0x20004E58`, 424 bytes below `_estack`, with IPSR zero. This only confirms
the stack address range; it is not reported as an observed maximum watermark.

## Qualification status

R5 Python and C parity remains 25,057 samples with zero mismatches, including
22,557 real R4 samples. Host CTest is 25/25, including the Beta-only guarantee
that volatile TARE cannot increment configuration revision or dirty state.
R2/R3/R4/R5, Stage 5B/5C/5L,
Manifest and register-map tests pass. Debug, Release, BoardDiagnostics, Beta and
Beta `-Wextra -Werror` builds pass. No dynamic allocation call exists in product
sources. ASan and UBSan remain **NOT RUN** because the available portable MinGW
distribution does not contain their runtimes.

The new Beta BIN changes because the RAM/stack fixes enter target firmware.
Hardware reflash and supervised smoke are therefore mandatory before Stage
5M-F entry can be approved.

An intermediate 99,424-byte R5C image was application-only flashed and verified,
then superseded before formal smoke when the pre-smoke audit found the volatile
TARE revision defect. The final 99,440-byte image is SHA-256
`2494CB70923C67E38F1642285CF3CEE717C1A207C42BF97B96FCC2A3BCFBDEFC`.

Current state: **STAGE 5M-R5C SOFTWARE READY; HARDWARE CLOSURE PENDING; STAGE
5M-F NOT YET APPROVED**.

The 2-4 hour static qualification, real 12-hour qualification, cross-sensor
validation and metrology certification remain **DEFERRED**.
