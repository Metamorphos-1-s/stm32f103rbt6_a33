# Stage 5G CS1237 Open-Drain IO Alignment

## Status

Stage 5G is **COMPLETE WITH KNOWN LIMITATION** on branch
`stage5g-cs1237-open-drain` from the
Stage 5F baseline `0e10a53dd1b89eafc34b8dc3a95964394bc1c01b`. The GPIO change
is functional commit `c3d1f10`. Both PCB revisions pass the qualified 10/40 Hz
configuration matrix and independent 30-minute 10 Hz sampling runs. Release
containment commit `725ab8a` rejects unqualified 640/1280 Hz product profiles
while BoardDiagnostics retains those driver modes for engineering work. The
observed 640 Hz issue is
classified as Deferred P1 and is excluded from production and Stage 6.

Do not mark this stage complete, merge it to `main`, create the Stage 5G tag,
or begin formal Stage 6 measurements until pull-up measurements, production-
rate waveforms, functional regressions, and concurrency gates are closed.

## Scope and motivation

Both supported PCB revisions provide board-level pull-ups on CS1237 SCLK and
DOUT. The firmware baseline is therefore unified as follows:

| Signal/state | Mode | MCU pull | HIGH behavior |
| --- | --- | --- | --- |
| PB10 SCLK | Open-drain output | Disabled | Released to external pull-up |
| PB11 DOUT read | Input | Disabled | External pull-up/device establishes level |
| PB11 DOUT write | Open-drain output | Disabled | Released to external pull-up |

The MCU actively drives only LOW. The bit-bang protocol, 1 us half-period,
clock idle LOW, configuration state machine, sample processing, metrology,
calibration, UI, protocols, and storage layout are unchanged.

## Static review

1. PB10 was initialized by CubeMX in `Core/Src/gpio.c:MX_GPIO_Init`.
2. Its old mode was `GPIO_MODE_OUTPUT_PP`.
3. Its old pull setting was `GPIO_NOPULL`.
4. Its old speed was `GPIO_SPEED_FREQ_LOW`.
5. Its initial output latch and clock idle state were LOW.
6. PB11 starts as `GPIO_MODE_INPUT`.
7. PB11 starts with `GPIO_NOPULL`.
8. Runtime direction switching is owned by
   `BSP/bsp_gpio.c:BSP_CS1237_SetDataDirection`.
9. The old DOUT write mode was `GPIO_MODE_OUTPUT_PP`.
10. The old DOUT read mode was already input with `GPIO_NOPULL`.
11. SCLK runtime writes are confined to `BSP_CS1237_SetClock`; CubeMX only
    performs the initial pin setup.
12. DOUT runtime reads/writes and mode changes are confined to the BSP; no
    upper module bypasses it with direct HAL GPIO access.
13. `Drivers/cs1237/cs1237.c` uses only the BSP interface for SCLK/DOUT.
14. BoardDiagnostics uses the same driver and BSP; it has no alternate GPIO
    initialization path.
15. Speed is unchanged: LOW for SCLK and HIGH for DOUT while the MCU writes.
16. Driver-supported rates are 10, 40, 640, and 1280 Hz; gains are 1, 2, 64,
    and 128; channels are A, temperature, and internal short. Settling discard
    is four samples for every supported rate.
17. A configuration transaction first reads a 24-bit frame and two status
    bits with DOUT as input, emits the final clock, preloads the DOUT latch
    HIGH/released, changes DOUT to MCU output, emits two clocks and the 7-bit
    command, switches to input before a read value or remains output for a
    write value, transfers eight bits, releases and returns DOUT to input, and
    emits the final clock.

No other module was found operating PB10 or PB11.

## Production capability review

The static product-path review gives the following answers:

1. High Speed Profile uses 40 Hz / gain 128 by default, not 640 Hz.
2. The local `SPd` and `GAIn` advanced entries are read-only. The ordinary
   Profile selector can only activate the saved Profile 0 or Profile 1.
3. Modbus can stage a profile rate. Before containment its product validation
   accepted 640/1280 Hz; Release validation now rejects either value.
4. BLE profile-field writes explicitly reject sample-rate and gain changes.
5. The CS1237 driver enum continues to implement 10, 40, 640, and 1280 Hz.
6. BoardDiagnostics deliberately retains all driver rates for engineering.
7. Release users cannot validate, apply, or persist a profile above 40 Hz.
8. Production Profile 0 is 10 Hz / gain 128. Profile 1 is 40 Hz / gain 128.
9. Stage 6 uses Profile 0 at 10 Hz / gain 128. A segment collected after any
   unintended rate change to 640 Hz is invalid for formal Stage 6 conclusions.

Canonical configuration validation still understands every driver enum.
`MetrologyConfig_ValidateProductHardware` supplies the narrower Release
product boundary. A live Release test confirmed that a Modbus transaction
staging Profile 1 at 640 Hz returns `INVALID_ARGUMENT` without applying it;
40 Hz validates and applies normally. A physical power cycle then restored
saved Profile 0, `config_dirty=0`, and the unchanged configuration slot.

## Implementation and startup behavior

`stm32f103rbt6_a33.ioc` now defines PB10 as
`GPIO_MODE_OUTPUT_OD/GPIO_NOPULL/GPIO_SPEED_FREQ_LOW`, with initial level LOW.
STM32CubeMX generated `Core/Src/gpio.c` from the updated `.ioc`; the generated
diff only separates PB10 from the push-pull GPIOB group and initializes it as
open-drain. PB11 remains input/NOPULL at startup.

At reset, PB10/PB11 are not driven by application code. `MX_GPIO_Init` first
preloads the PB10 latch LOW, then enables open-drain output. PB11 remains input.
The BSP subsequently asserts the same SCLK idle LOW. There is no application
startup window that configures either line as push-pull HIGH.

For DOUT input-to-output switching, the BSP writes the output latch HIGH before
enabling open-drain output. For output-to-input switching, it again writes HIGH
before enabling input mode. A later output transition therefore starts from
the released state instead of producing a stale-latch LOW pulse.

## Datasheet electrical review

The STMicroelectronics STM32F103x8/xB datasheet, DS5319 Rev.20, marks PB10 and
PB11 as `FT` (5 V tolerant). Under normal operating conditions an FT input is
specified up to 5.5 V when VDD is above 2 V. If an input is held above
`VDD + 0.3 V`, the datasheet requires the internal pull-up/down resistors to be
disabled. Both pins use `GPIO_NOPULL`. The measured new-board pull-up voltage
is 4.997 V through 4.7 kOhm on each line, within the FT limit; dynamic timing
still depends on the measured RC rise time.

The official CS1237 product documentation identifies manual
`DS_CS1237_V1.1`. Its digital limits are `VIH >= 0.7 x DVDD` and
`VIL <= 0.3 x DVDD`; the SCLK maximum is 1.1 MHz and the documented minimum
HIGH or LOW pulse width is approximately 455 ns. Firmware retains a nominal
1 us HIGH and 1 us LOW period. Read sampling occurs after the 1 us HIGH delay.
Whether the external-pull-up RC rise reaches VIH with sufficient margin must
still be demonstrated by the required oscilloscope capture.

Primary references:

- STM32F103x8/xB datasheet DS5319 Rev.20:
  <https://www.st.com/resource/en/datasheet/stm32f103rb.pdf>
- Chipsea CS1237 product and V1.1 manual listing:
  <https://www.chipsea.com/product/details/?id=1155>

## Pull-up and waveform record

The repository contains no schematic, PCB source, or BOM from which the
pull-up values can be verified. The operator measured both old-board pull-ups
and their rail directly; the production-rate electrical measurements below
are accepted for this validation.

| Item | Old PCB | New PCB |
| --- | --- | --- |
| SCLK pull-up voltage | 3.33 V | 4.997 V |
| DOUT pull-up voltage | 3.33 V | 4.997 V |
| SCLK pull-up resistance | 4.7 kOhm | 4.7 kOhm |
| DOUT pull-up resistance | 4.7 kOhm | 4.7 kOhm |
| SCLK LOW/HIGH voltage | 66.66 mV / 3.26 V | 66.667 mV / 5.000 V |
| DOUT LOW/HIGH voltage | 66.66 mV / 3.26 V | 66.667 mV / 5.000 V |
| SCLK 10%-90% rise time | 250 ns | Approximately 315 ns |
| DOUT 10%-90% rise time | 15 ns | Approximately 15 ns |
| SCLK HIGH/LOW time | 2.5 us / 3.1 us | 2.515 us / 3.055 us |
| Input-to-OD glitch/contention | PASS; none observed | PASS; no unexpected LOW glitch during actual 10/40 Hz configuration transactions |
| OD-to-input glitch/contention | PASS; normal release, no contention or abnormal spike | PASS; normal release, no contention or abnormal spike |

The new-board production-rate waveform levels, rise times, and SCLK pulse
widths pass. SCLK reaches the CS1237 VIH threshold within its measured HIGH
window, and both measured pulse widths exceed the documented approximately
455 ns minimum by a wide margin. During actual Profile 0/10 Hz to Profile 1/
40 Hz and return transactions, DOUT input-to-OD produced no unexpected LOW
glitch, OD-to-input released normally, and neither transition showed
contention or an abnormal spike. No save was issued.

## Software verification

All pre-hardware gates pass:

- Host CTest: 15/15.
- Stage 5B Python: 28/28.
- Stage 5C Python: 12/12.
- Debug: 86,044 B Flash / 14,832 B RAM.
- Release: 73,896 B Flash / 14,816 B RAM.
- BoardDiagnostics: 122,516 B Flash / 14,720 B RAM.
- BoardDiagnostics image end: `0x0801DE94`.
- BoardDiagnostics margin to `0x0801F000`: 4,460 B.
- Release ELF SHA256:
  `3C90E13117EE673BEA04C1348840140106E24695255829F5F6F99BBB97277942`.

These results include the Release product-rate containment tests. The same
source builds cleanly as BoardDiagnostics, proving that the engineering-rate
driver path remains compiled and within the Flash boundary.

## Hardware record

The old-PCB pull-up, production-rate waveform, ZERO/RESET ZERO, TARE/CLEAR
TARE and calibration checks are now recorded below. The final Stage 5G
concurrent rerun is recorded below. Earlier failed attempts are retained as
retry history; only the clean third run is the final functional PASS.

The connected board is identified by the operator as the old hardware
revision. ST-LINK serial `E1007200D0D2139393740544`, firmware `V2J47S7`, read
the target at 3.29 V and identified an STM32F103 medium-density device. After
the operator reinserted the probe, the Release ELF was programmed with normal
SWD at 125 kHz. Sectors 0 through 72 were erased, download and verify passed,
and the configuration pages at sectors 124 through 127 were not touched.

The post-flash probe on COM7 at address 1, 115200 N1 returned register map
`0x0103`, firmware `0x050A`, Schema V2, slot B sequence 16, CS1237 RUNNING (4),
zero overrun, and `config_dirty=0`. A 300-second FC03 run completed 1368/1368
requests with zero timeout, CRC error, exception, or retry. Sample sequence
growth was 3054; mean response was 55.611 ms and P99 was 57.421 ms. The raw
report is `Tools/stage5b_hw/reports/20260816_223024_rs485` and the CSV is
`Results/stage5g_old_board_5min.csv`.

The first empty-scale 10-minute attempt stopped after 287.390 s when the CH340
USB-RS485 adapter temporarily disappeared from Windows. Its first 1309
requests succeeded, request 1310 timed out, and there were no CRC errors or
Modbus exceptions. A simultaneous SWD check showed that the MCU had not reset:
CS1237 remained RUNNING, its sample count continued to advance, and driver
read-error and overrun counters remained zero. After physically reinserting
only the USB-RS485 adapter, COM7 recovered. This attempt is retained as a host
USB-link interruption and is not counted as the ADC sanity PASS.

The empty-scale sanity test was then restarted from zero and completed
599.967 s. All 2733/2733 FC03 requests succeeded with zero timeout, CRC error,
exception, retry, or retry failure; sample sequence grew by 6114. Latency was
55.348 ms mean and 56.992 ms P99. Across 2733 observations, raw count was mean
`-43654.215`, standard deviation `18.140`, range `-43751..-43585`, and
peak-to-peak `166`. Filtered raw was mean `-43654.232`, standard deviation
`13.944`, range `-43704..-43583`, and peak-to-peak `121`. Every row reported
stable. The ending SWD snapshot independently showed driver read-error zero,
overrun zero, and an advancing sample count. The report is
`Tools/stage5b_hw/reports/20260816_231825_rs485`; CSV is
`Results/stage5g_old_board_empty_10min_rerun.csv`.

The independent 30-minute empty-scale continuous run completed 1799.979 s.
All 8187/8187 FC03 requests succeeded with zero timeout, CRC error, exception,
retry, or retry failure; sample sequence grew by 18342. Latency was 55.369 ms
mean and 56.996 ms P99. Raw count was mean `-43641.869`, standard deviation
`19.616`, range `-43715..-43570`, and peak-to-peak `145`. Filtered raw was mean
`-43641.929`, standard deviation `15.700`, range `-43704..-43587`, and
peak-to-peak `117`. The first and last five-minute filtered means differed by
`+22.134` counts. Every observation reported stable. The ending SWD snapshot
showed driver read-error zero, overrun zero, and sample count `36077`. This is
a Stage 5G sanity characterization, not a Stage 6 metrology baseline. Report:
`Tools/stage5b_hw/reports/20260816_233006_rs485`; CSV:
`Results/stage5g_old_board_empty_30min.csv`.

Configuration testing edited only inactive profile 1, applied it in RAM,
switched profiles, verified the active rate/gain and RUNNING state, observed
sample-sequence growth, then switched back. No save command was issued. All
four gains at 10 Hz and all four gains at 40 Hz passed, including config write,
readback, four-sample settling, sampling, restore, and zero overrun. The saved
default profile 1 (40 Hz / gain 128) was also restored and rechecked.

At 640 Hz, both gain 1 and gain 128 could be written, read back, settle, and
reach RUNNING with zero fault and zero FIFO overrun. Once high-rate continuous
sampling was active, however, RS485 multi-write command responses became
intermittent or incomplete and the switch-back command could not be completed
reliably within the retry window. A single FC03 succeeded after a 3-second idle
gap, showing that the MCU and ADC were still running; a software reset safely
restored the saved 10 Hz profile. One earlier rapid-switch attempt produced a
config readback of `0xFF` and `FAULT_CS1237_CONFIG_ERROR`; SWD confirmed the
expected config was `0x20`, the read error count was one, SCLK remained latched
LOW, and DOUT remained latched HIGH/released. The result is classified as a
Deferred P1 high-rate/configuration-concurrency issue. It is not yet attributed
to either GPIO rise time or application scheduling. The 1280 Hz matrix was not
continued after this reproducible 640 Hz issue.

After all temporary RAM tests, an SWD software reset restored profile 0
(10 Hz / gain 128), slot B sequence 16, `config_dirty=0`, CS1237 RUNNING, and
normal weighing. Flash calibration/configuration was not changed.

The operator then connected the new PCB revision. Its STM32 target was read at
3.29 V and identified as an STM32F103 medium-density device. Application Flash
at `0x08000000` was blank (`0xFFFFFFFF`), so the same Stage 5G Release ELF was
programmed and verified successfully at 125 kHz SWD; only sectors 0 through 72
were erased. After reset, SWD showed CS1237 RUNNING with an advancing sample
count (2206 at the recorded snapshot), driver read-error zero, and overrun
zero. Initial COM7 probes failed because the USB converter had not been placed
in RS485 mode. Once the operator selected RS485 mode, address 1 at 115200 N1
responded normally.

The operator completed one local 500 g two-point CAL but intentionally did not
save it. Readback showed `raw_zero=-43934`, loaded raw `-487700`, delta
`-443766` counts, span mass `500000000 ug`, calibration sequence 1, and a valid
calibration. Storage remained NONE/sequence 0 and `config_dirty=1`, proving the
calibration was RAM-only. With the 500 g load left in place, a 299.984-second
run completed 1364/1364 FC03 requests with zero timeout, CRC error, exception,
retry, CS1237 read error, or overrun; sample sequence grew by 2970. Raw count
standard deviation/peak-to-peak were 10.944/70 counts and filtered values were
6.676/37 counts. The locked display mass was `500083377 ug`. Report:
`Tools/stage5b_hw/reports/20260817_013142_rs485`; CSV:
`Results/stage5g_new_board_500g_5min.csv`.

The new board initially used the blank-Flash default Profile 0 filter strength
3 even though the visible menu value was `FILt=3` (MEDIAN3_IIR). Re-submitting
`FILt=3` through the local menu normalized the hidden strength to 1, as defined
by the menu commit mapping, and the operator confirmed that the response speed
returned to normal. Modbus readback then confirmed active Profile 0 mode 3 and
strength 1. The RAM calibration remained valid with sequence 1 and a 500 g
span, while storage remained NONE/sequence 0 and `config_dirty=1`; no save or
power cycle has occurred.

With the load removed, the post-normalization new-board empty-scale run
completed 599.781 seconds. All 2737/2737 FC03 requests succeeded with zero
timeout, CRC error, exception, retry, or retry failure; sample sequence grew by
5938. Latency was 55.329 ms mean and 56.990 ms P99. Raw count was mean
`-44214.479`, standard deviation `17.410`, and peak-to-peak `108`; filtered raw
was mean `-44214.559`, standard deviation `16.157`, and peak-to-peak `89`.
Every observation reported stable. The ending Modbus diagnostic read showed
CS1237 RUNNING (4), zero buffered samples, zero overrun, and zero reported
error count. Report: `Tools/stage5b_hw/reports/20260817_133507_rs485`; CSV:
`Results/stage5g_new_board_empty_10min_strength1.csv`.

The first post-load return-to-zero check exposed a stable approximately
`+0.27 g` offset from the original RAM calibration, rather than filter lag. A
120-second check completed 547/547 requests with zero communication error;
mass averaged `+0.269924 g` over `+0.244464..+0.298539 g`, while the first-to-
last quarter mean changed only `-0.019268 g`. The operator therefore repeated
the local two-point calibration after warm-up instead of saving the offset
calibration. The replacement calibration uses `raw_zero=-44148`, loaded raw
`-487980`, a 500 g span, and calibration sequence 2. It read `500.01 g` under
load and `0.05 g` after unload; the measured loaded-to-empty difference was
approximately `499.994 g`.

After explicit operator confirmation, REQUEST_SAVE completed and an identical
second request left the storage sequence unchanged. Post-save readback showed
slot A/sequence 1, `config_dirty=0`, active mode 3/strength 1, and calibration
sequence 2 with the same zero, loaded, and span values. A physical power-cycle
restore then passed: slot A/sequence 1 and dirty zero were retained, mode
3/strength 1 was restored, calibration sequence 2 and all calibration values
matched, CS1237 returned to RUNNING, and the stable empty display read
`-0.01 g`. The post-cycle 500 g check then read `500.03 g`
(`500.048442 g` by the 64-bit net-mass register), stable, with CS1237 RUNNING,
zero overrun, dirty zero, and slot A/sequence 1 unchanged. The new-board local
calibration, persistence, and power-cycle restore loop therefore passes.

The new-board normal-rate configuration matrix then passed all eight
combinations: gains 1, 2, 64, and 128 at both 10 Hz and 40 Hz. Each test edited
only inactive Profile 1 through CONFIG_BEGIN/VALIDATE/APPLY_RAM, switched to
Profile 1 separately, confirmed the selected profile and rate/gain readback,
observed at least four new samples in CS1237 RUNNING state, and reported zero
overrun before switching back to Profile 0. The complete active configuration
was restored word-for-word from its pre-test snapshot, Profile 0 was active,
and CS1237 remained RUNNING with zero overrun. The runtime dirty flag remained
set because RAM commits advance the configuration revision even when the final
values match Flash; no save was issued. A subsequent power cycle reloaded slot
A/sequence 1, restored Profile 0 mode 3/strength 1, cleared dirty to zero, and
returned to stable empty weighing at approximately `0.01 g` with CS1237
RUNNING and zero overrun.

The new-board 30-minute empty-scale run completed 1799.850 seconds. All
8213/8213 FC03 requests succeeded with zero timeout, CRC error, exception,
retry, or retry failure; sample sequence grew by 17819. Mean latency was
55.276 ms and P99 was 56.974 ms. Raw count was mean `-44087.742`, standard
deviation `14.702`, range `-44151..-44041`, and peak-to-peak `110`; filtered raw
was mean `-44087.803`, standard deviation `12.912`, range `-44140..-44053`, and
peak-to-peak `87`. The first-to-last five-minute filtered mean changed by
`+29.146` counts. Every observation reported stable. The ending probe showed
Profile 0, slot A sequence 1, dirty zero, CS1237 RUNNING, zero overrun, and a
display of `-0.08 g`. This remains a Stage 5G sanity characterization rather
than a formal metrology baseline. Report:
`Tools/stage5b_hw/reports/20260817_160119_rs485`; CSV:
`Results/stage5g_new_board_empty_30min_strength1.csv`.

At the saved production Profile 0 (10 Hz / gain 128), new-board ZERO and
RESET ZERO passed at empty scale: ZERO changed the display to `0.00 g`, and
RESET ZERO restored the underlying empty reading. With a stable 500 g load,
the pre-tare display was `500.02 g`; TARE changed it to `0.00 g` and asserted
the tare state, while CLEAR TARE restored `500.03 g` and cleared the tare
state. Sample sequence continued through every command and no Flash save was
issued. The previously recorded local two-point calibration, persistence, and
post-power-cycle known-load checks supply the calibration-path regression.

The new-board 60-second production-rate concurrency gate passed with the
actual W02 module at `C8:46:82:00:83:2C`. BLE remained connected for 69.625 s
and received FAST 312, SLOW 63, and CHECKWEIGH `0x03` 63. Device Info,
Get Config, and Calibration Status commands completed 3/3 without timeout,
retry, result error, or transaction mismatch. BLE CRC, sequence gap,
duplicate, timestamp anomaly, resynchronization, and disconnect counts were
all zero. Concurrent COM7 FC03 completed 164/164 requests with zero timeout,
CRC error, exception, or retry; sample sequence grew by 591. The test ran at
Profile 0 / 10 Hz / gain 128 with an indicated stable 500.00 g load.

The post-run SWD snapshot recorded CS1237 read errors 0, FIFO overruns 0,
event-queue drops 0, and Fault mask 0. UART/DMA error and overflow counters
were zero; Modbus recorded 1668 cumulative valid/addressed FC03 frames and
1668 responses with no CRC, length, protocol, or TX error. BLE recorded no
transport, UART, parse, CRC, queue, scheduler-drop, or command-parser error;
all 3 command requests had 3 responses. Evidence is in
`Results/stage5g_hw/20260817_1721_new_board_60s`.

The current old-board session measured 4.7 kOhm pull-ups to 3.33 V on both
SCLK and DOUT. At Profile 0/10 Hz, SCLK measured 66.66 mV LOW, 3.26 V HIGH,
250 ns rise time and 2.5 us/3.1 us HIGH/LOW; DOUT measured 66.66 mV/3.26 V
and 15 ns rise time. The same values were observed at Profile 1/40 Hz. No
glitch, spike or bus contention was observed at either rate or during the
profile direction transitions.

The same session completed 1108/1108 read-only FC03 requests in 300 s with
zero timeout, CRC error, exception or retry; sample sequence growth was 3055,
mean response 56.061 ms and P99 57.577 ms. The report is
`Tools/stage5b_hw/reports/20260817_200629_rs485` and the CSV is
`Results/stage5g_old_board_stage5h_session.csv`.

With the scale empty and stable, ZERO, RESET ZERO, TARE and CLEAR TARE each
returned `OK`. A stable 500 g load read 499.96 g before unload. RESET ZERO
returned the empty reading to its underlying approximately 0.97 g baseline;
no SAVE was issued during these actions. Reports are
`Tools/stage5b_hw/reports/20260817_201341_rs485`,
`Tools/stage5b_hw/reports/20260817_201724_rs485`,
`Tools/stage5b_hw/reports/20260817_201744_rs485`,
`Tools/stage5b_hw/reports/20260817_201824_rs485`, and
`Tools/stage5b_hw/reports/20260817_201535_rs485`.

The old-board calibration workflow then completed with a 500 g span. The
zero and span captures, commit, Flash SAVE and power-cycle recovery all
returned `OK`; after power-up the calibration-valid flag was `1`, the saved
slot sequence was `17`, the calibration sequence was `4`, and the stable
reading was 499.97 g. The calibration reports are
`Tools/stage5b_hw/reports/20260817_205112_rs485`,
`Tools/stage5b_hw/reports/20260817_205114_rs485`,
`Tools/stage5b_hw/reports/20260817_205313_rs485`,
`Tools/stage5b_hw/reports/20260817_205557_rs485`, and the post-cycle reads
under `Tools/stage5b_hw/reports/20260817_210207_rs485` through
`Tools/stage5b_hw/reports/20260817_210617_rs485`.

### Final old-board 120 s BLE/RS485/CS1237 concurrency (2026-08-17)

The final run used the old PCB, Stage 5G firmware commit `eca9921`, Release
ELF SHA256
`3C90E13117EE673BEA04C1348840140106E24695255829F5F6F99BBB97277942`, and
Profile 0 / 10 Hz / gain 128. W02 was `C8:46:82:00:83:24` on COM7 RS485.

BLE remained connected for the 120 s run (`disconnects=0`, connected at end)
and received 854 frames: FAST 610, SLOW 122, and CHECKWEIGH/`0x03` 122.
Six command requests/responses completed with zero timeout, retry, command
error, or transaction mismatch. BLE telemetry CRC, sequence-gap, duplicate,
timestamp, stream-CRC, unknown-frame, and partial-frame counters were all 0.

Concurrent RS485 completed 443/443 read-only FC03 requests with zero timeout,
CRC error, exception, or retry. Sample-sequence growth was 1221; mean response
latency was 55.917 ms and P99 was 57.545 ms. No CS1237 read error, overrun, or
other ADC error was externally observed during the run.

The first attempt had one RS485 timeout at request 250, and the second attempt
had a BLE GATT-services `Unreachable` startup failure while its independent
RS485 run passed 444/444. Both recovered without firmware changes; they remain
documented retry history, not hidden. The third run above is the final clean
functional result. The dedicated internal UART/DMA/RTU/BLE snapshot counters
were not verified for this run: they are not Modbus-mapped and the attempted
ST-LINK GDB capture could not bind its server port. No counter values are
invented.

| Gate | Old PCB | New PCB |
| --- | --- | --- |
| Stage 5G Release programmed/verified | PASS | PASS; blank MCU programmed and verified |
| 5 min sampling and zero errors | PASS at 10 Hz | PASS at 10 Hz with 500 g |
| Config write/readback and restore | PASS at 10/40 Hz; 640 Hz enters RUNNING | PASS at all gains, 10/40 Hz; power-cycle restore PASS |
| Production rate/gain/profile transactions | PASS at 10/40 Hz, all gains | PASS at 10/40 Hz, all gains |
| 640/1280 Hz | DEFERRED / NOT QUALIFIED | DEFERRED / NOT QUALIFIED |
| Empty/known-load/unload | PASS; 499.96 g at stable 500 g | PASS at empty and 500 g before/after save and power cycle |
| ZERO/RESET ZERO | PASS; command/readback and underlying baseline restored | PASS at 10 Hz; command/readback and return passed |
| TARE/CLEAR TARE | PASS; TARE_ACTIVE set and cleared | PASS at 10 Hz; 500.02 -> 0.00 -> 500.03 g |
| Calibration read/workflow | PASS; 500 g span, SAVE and power-cycle recovery, valid=1, sequence 4 | PASS; slot A sequence 1 restored after power cycle |
| 10 min ADC sanity | PASS; clean 599.967 s rerun after documented CH340 interruption | PASS after local `FILt=3` normalized strength to 1 |
| 30 min continuous sampling | PASS; 1799.979 s, 8187/8187, zero ADC/transport error | PASS; 1799.850 s, 8213/8213, zero ADC/transport error |
| 120 s BLE/RS485/CS1237 concurrency | PASS; BLE 854 frames/6 commands, RS485 443/443, external error counters zero; internal snapshot not captured | PASS; 60 s gate retained above |
| Open-drain waveform | PASS at 10/40 Hz; no glitch, spike or contention | PASS at production 10 Hz, including actual configuration direction switching |

## Known Limitation - CS1237 640 Hz Mode

The CS1237 640 Hz operating mode remains implemented in the driver but is not
qualified for production use after the push-pull to open-drain transition.

Observed issue: after a successful 640 Hz write, readback, settling, and entry
to RUNNING, continuous high-rate sampling made RS485 multi-write responses
intermittent or incomplete and prevented reliable profile switch-back. One
rapid-switch attempt also produced readback `0xFF` and
`FAULT_CS1237_CONFIG_ERROR`. The evidence does not yet distinguish electrical
rise-time margin from application scheduling.

Current impact:

- The qualified 10 Hz and 40 Hz production profiles are unaffected.
- Release product validation rejects 640 Hz and 1280 Hz profiles.
- BoardDiagnostics retains the modes for future engineering requalification.
- Stage 5H startup auto-zero development is unaffected.
- Stage 6 uses 10 Hz / gain 128 and explicitly excludes 640 Hz.
- 640 Hz must not be advertised as a qualified customer mode.

Decision: Deferred P1, excluded from the production baseline. Requalification
is required before any future customer enablement and must cover pull-up RC,
rise time, SCLK/DOUT timing, GPIO speed and direction switching, configuration
transactions, concurrency, and long-duration sampling.

## Stage 6 production baseline

- SCLK: PB10 open-drain/NOPULL, external pull-up, idle LOW.
- DOUT read: PB11 input/NOPULL, external pull-up.
- DOUT write: PB11 open-drain/NOPULL, external pull-up.
- Profile: Profile 0 / High Precision.
- Sample rate: 10 Hz.
- Gain: 128.
- 640 Hz: EXCLUDED, NOT QUALIFIED, NOT USED IN STAGE 6.
- Every Stage 6 run must record rate, gain, profile, filter, stability,
  Runtime Drift state, and Startup Auto Zero state before collecting data.

## Decision

The 640 Hz finding is no longer a blocking product issue because Release now
prevents entry and Stage 6 explicitly excludes it. It remains Deferred P1 and
must not be described as PASS. Qualified-rate sampling, configuration,
electrical waveforms, ZERO/TARE and calibration pass on both revisions. The
final BLE/RS485/CS1237 concurrency rerun passed on the old board. Stage 5G is
therefore **COMPLETE WITH KNOWN LIMITATION**. Blocking P0=0, blocking P1=0;
Deferred P1=1 is the excluded, unqualified CS1237 640 Hz path. Internal SWD
snapshot counters remain an evidence limitation for this run, not a product
failure, and are not represented as verified values.
