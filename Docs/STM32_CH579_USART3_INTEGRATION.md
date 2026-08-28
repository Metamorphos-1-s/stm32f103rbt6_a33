# STM32 and CH579 USART3 integration

## Status and baseline

Stage 5I-A is code complete on branch `stage5i-ch579-usart3`, based on
`8c44caf93180005207ef53a9de6969433f21fc37` and tag
`stage5h-zero-drift-control-tested`. Hardware validation is pending.

The STM32F103RBT6 does not provide a fourth asynchronous serial peripheral.
The CH579 interface therefore uses USART3 with its partial remap. All source
symbols and files use `USART3` or `Uart3` naming.

This stage adds only a raw, nonblocking serial transport. It does not connect
USART3 to CommunicationManager, ModbusRtuServer, the register model, command
transactions, or Flash persistence.

## Hardware configuration

USART3 is fixed at 115200 baud, 8 data bits, no parity, one stop bit, no flow
control, and oversampling by 16. Partial remap assigns USART3 TX to PC10 and RX
to PC11. PC10 is high-speed alternate-function push-pull; PC11 is a floating
input with no internal pull. Full remap is not used.

| Function | Resource | Mode | Priority |
|---|---|---|---|
| USART3 RX | DMA1 Channel 3 | circular, byte, memory increment | high |
| USART3 TX | DMA1 Channel 2 | normal, byte, memory increment | medium |
| DMA1 Channel 2 IRQ | NVIC | enabled | 5,0 |
| DMA1 Channel 3 IRQ | NVIC | enabled | 5,0 |
| USART3 IRQ | NVIC | enabled | 5,0 |

DMA1 Channel 6/7, USART2, TIM4, USART1, and W02 configuration are unchanged.

## Wiring

| CH579M | STM32F103RBT6 | Direction |
|---|---|---|
| PB7 / UART0_TXD | PC11 / USART3_RX | CH579 to STM32 |
| PB4 / UART0_RXD | PC10 / USART3_TX | STM32 to CH579 |
| GND | GND | common reference |

The connection is 3.3 V TTL only. It must not pass through an RS232 level
converter or an RS485 transceiver.

## BSP interface

`BSP/bsp_uart3_dma.h` exposes initialization, circular RX DMA, normal TX DMA,
RX position, RX stop, TX abort, physical TX-complete state, IDLE control and
event retrieval. TX accepts `const uint8_t *`, supports up to 256 bytes, and
returns BUSY without waiting when another transfer owns the HAL TX state. The
caller must keep its data valid until completion; the diagnostic layer meets
this rule by copying requests into its static TX buffer.

DMA Channel 2, DMA Channel 3, and USART3 vector handlers delegate directly to
the BSP. Interrupt handlers only capture counters, RX positions, timestamps,
and bounded IDLE events. The existing single set of HAL UART callbacks remains
in `BSP/bsp_uart_dma.c`: its USART1 and USART2 paths retain their prior order
and behavior, with a new USART3 branch forwarding to the independent BSP.

The BSP records IDLE, RX half/full completion, RX/TX DMA errors, TX completion,
parity/frame/noise/overrun errors, physical TC, and IDLE queue overflow. IDLE
interrupts stay disabled until an RX DMA buffer is successfully started.

## Bring-up diagnostics

The CMake option `A33_ENABLE_STAGE5I_USART3_BRINGUP` defaults to OFF. The
`Usart3Bringup` preset enables it:

```powershell
cmake --preset Usart3Bringup
cmake --build --preset Usart3Bringup
```

When enabled, `Stage5iUsart3Diagnostics` starts one 512-byte static circular RX
buffer and owns one 256-byte static TX buffer. It records frame/byte/error
counters, the most recent length, the first 64 retained bytes of the last IDLE
delimited receive span, and the first 64 bytes of the last TX request. No
dynamic allocation, formatted logging, blocking wait, or automatic echo is
used.

The following symbols are callable or inspectable through SWD:

- `Stage5iUsart3Diagnostics_Get()` returns the bounded snapshot.
- `Stage5iUsart3Diagnostics_SendTestFrame()` requests
  `A5 5A 00 FF 81 03 56 78` through TX DMA.
- `Stage5iUsart3Diagnostics_RequestTx()` accepts caller data up to 256 bytes,
  copies it into the static diagnostic buffer, and starts DMA if TX is idle.

The CH579-to-STM32 receive vector for hardware validation is
`55 AA 00 FF 01 03 12 34`.

## Modified files

- CubeMX and generated glue: `stm32f103rbt6_a33.ioc`, `Core/Inc/main.h`,
  `Core/Inc/usart.h`, `Core/Inc/stm32f1xx_it.h`, `Core/Src/main.c`,
  `Core/Src/usart.c`, `Core/Src/dma.c`, `Core/Src/stm32f1xx_it.c`.
- USART3 BSP: `BSP/bsp_uart3_dma.c`, `BSP/bsp_uart3_dma.h`.
- Shared callback routing: `BSP/bsp_uart_dma.c`.
- Diagnostics and app hook: `Diagnostics/stage5i_usart3_diagnostics.c`,
  `Diagnostics/stage5i_usart3_diagnostics.h`, `App/app_main.c`.
- Build/configuration: `CMakeLists.txt`, `CMakePresets.json`,
  `Config/project_config.h`.
- Evidence: `Docs/STM32_CH579_USART3_INTEGRATION.md`.

## Build and software regression

GNU Arm 13.3.1 builds completed with no compiler warnings. Stage 5H sizes are
from the frozen Stage 5H closeout and current sizes are linker-reported values
using the same 124 KiB application region.

| Image | Stage 5H Flash/RAM | Stage 5I-A Flash/RAM | Delta |
|---|---:|---:|---:|
| Debug, diagnostics OFF | 87,212 / 14,864 B | 88,260 / 15,224 B | +1,048 / +360 B |
| Release, diagnostics OFF | 74,840 / 14,848 B | 75,824 / 15,208 B | +984 / +360 B |
| BoardDiagnostics, diagnostics OFF | 124,324 / 14,760 B | 125,836 / 15,120 B | +1,512 / +360 B |
| Usart3Bringup, diagnostics ON | n/a | 89,468 / 16,240 B | n/a |

BoardDiagnostics uses 99.10% of its 124 KiB application region and leaves
1,140 bytes. It links successfully, but this margin is a Stage 5I-B constraint.

Host CTest was configured with MSVC 19.43.34810 and its existing `/W4 /WX`
policy. All 16 of 16 tests passed. The Stage 5B Python unittest suite passed
29 of 29 and the Stage 5C Python unittest suite passed 12 of 12. These are
software regressions only and do not replace physical serial tests.

## Hardware validation

No STM32/CH579 wired test or physical USART1/USART2 regression was performed
during this implementation. Compilation, source inspection, and host tests are
not reported as hardware PASS.

| Item | Result | Evidence / note |
|---|---|---|
| USART3 initialization | NOT RUN | requires STM32/CH579 hardware |
| PC10/PC11 configuration | NOT RUN | requires pin waveform and level check |
| CH579 TX | NOT RUN | requires STM32/CH579 hardware |
| STM32 RX | NOT RUN | requires byte comparison on hardware |
| STM32 RX DMA | NOT RUN | requires wrap and position observations |
| STM32 TX DMA | NOT RUN | requires waveform and byte comparison |
| CH579 RX | NOT RUN | requires CH579 capture/parser evidence |
| IDLE detection | NOT RUN | requires one-event-per-gap observation |
| Continuous communication | NOT RUN | requires soak evidence |
| FC03 | NOT RUN | outside Stage 5I-A scope; Stage 5I-B |
| FC06 | NOT RUN | outside Stage 5I-A scope; later stage |
| FC16 | NOT RUN | outside Stage 5I-A scope; later stage |
| USART2 regression | NOT RUN | host tests pass; physical RS232/RS485 required |
| W02 BLE regression | NOT RUN | host/Python tests pass; physical BLE required |

Hardware validation must include ordinary bytes, `00`, `FF`, continuous data,
and a transfer close to 256 bytes. Evidence must include the diagnostic
snapshot and the CH579-side byte capture, plus USART2 and W02 regression logs.

## Stage 5I-B entry conditions

Stage 5I-B must not begin its dual-port Modbus integration until:

1. PC10/PC11 levels, partial-remap routing, 115200 8N1, RX circular DMA, TX
   normal DMA, IDLE behavior, wrap handling, error counters, and a near-maximum
   transfer pass on physical hardware.
2. USART2 RS232/RS485 and USART1 W02 BLE hardware regressions pass while
   USART3 traffic is active.
3. The second Modbus instance receives explicit ownership, timing, buffering,
   and concurrency designs rather than copying the USART2 singleton stack.
4. BoardDiagnostics Flash pressure is addressed or a deliberate image-content
   decision is recorded before adding protocol code.

Current conclusion: **Stage 5I-A CODE COMPLETE; hardware validation pending**.

## Stage 5I-A-HW validation attempt - 2026-08-26

### Environment and blocker

The validation attempt started on branch `stage5i-ch579-usart3` at commit
`458cb1db545d56f9f1dc282b29f64b5247dd23eb` with a clean worktree. Windows
enumerated `COM20` as WCH CH343 and `COM3` as WCH CH340. Their physical wiring
to PC10/PC11 or CH579 UART1 was not identifiable from USB metadata and was not
assumed. STM32CubeProgrammer 2.19.0 returned `No ST-Link detected!` for
`-l stlink`. Consequently the diagnostic image could not be programmed and no
SWD snapshot, reset evidence, pin measurement, or physical serial result was
available. No USB-TTL or CH579 TX line was driven without confirmed wiring.

### CubeMX regeneration

STM32CubeMX 6.15.0/DB.6.0.150 loaded, saved, and generated the project twice
in quiet script mode. The first generation normalized only generated ordering:
PC10/PC11 pin records, CH579 pin defines, and USART2/USART3 DMA handle
declarations. The second generation produced byte-identical key files; SHA256
values before and after the second generation matched:

| Key file | SHA256 after both generations |
|---|---|
| `stm32f103rbt6_a33.ioc` | `4BC1C13599D562FB98004E6A810F3A834D41794BE07BE21BD73A9B63BE0CD7EB` |
| `Core/Inc/main.h` | `234DC8DEBDC0375688DC96BF5E2E7DC56D5E185A543D399A242C01F665782F5F` |
| `Core/Inc/stm32f1xx_it.h` | `905E53400896A745AF6F67AEDCE841247D2E353341E7EA7F47BAFF763E269A28` |
| `Core/Inc/usart.h` | `6295C53F36A77D18300BEEF8324E37FD3CD769D7F0B37488E6F7BCDE75A0CEC8` |
| `Core/Src/dma.c` | `535D134CA0C7AAA8E29B4D79D22CFCE0C298A86C4828AE4A55D60D8CD64BFAD2` |
| `Core/Src/main.c` | `EC059267306193DABEFEDED529A4EE0B3942F8C63F0F14B10692A82B33A389C6` |
| `Core/Src/stm32f1xx_it.c` | `F2E9AD681CD957CDCB4B3766DC5A897BE7810FF40DCB6ED5AD34A31AB648DD6E` |
| `Core/Src/usart.c` | `353E91D50CB827CAA050ED6EB7C6A002998422692ADF2582360069F8ECA53BF5` |

Both generated results retained PC10 TX, PC11 RX, USART3 partial remap,
DMA1 Channel 3 circular/high RX, DMA1 Channel 2 normal/medium TX, all three
new IRQs, and the pre-existing USART1, USART2, DMA1 Channel 6/7, TIM4 and USER
CODE paths. CubeMX logged optional import diagnostics saying DMA request
parameters were "currently not set" for both pre-existing USART2 and new
USART3 requests, plus unrelated installed third-party pack warnings. The saved
IOC and generated sources nevertheless retained the complete DMA parameters,
the second generation was stable, and all four firmware builds passed.

### Rebuild evidence

`A33_ENABLE_STAGE5I_USART3_BRINGUP` was OFF in Debug, Release, and
BoardDiagnostics CMake caches and ON only in Usart3Bringup. GNU Arm 13.3.1
reported no compiler warnings.

| Image | Flash / RAM | ELF timestamp (Asia/Shanghai) | ELF SHA256 |
|---|---:|---|---|
| Debug | 88,260 / 15,224 B | 2026-08-26 16:29:45 | `169CD56954DADF250879B7D2C64BB2170EEF5FE68651D2ABAA6FCC2D7A4A238D` |
| Release | 75,824 / 15,208 B | 2026-08-26 16:29:44 | `BCA8310A5D39996F4DFC2D47F06D32D3321D8A1E0D54CB5627E2C1A4AB107925` |
| BoardDiagnostics | 125,836 / 15,120 B | 2026-08-26 16:29:45 | `AD8605FAB90EB8269F23D7581DC9082BA754F6B8D5B81BDFE503F5BB3B02D558` |
| Usart3Bringup | 89,468 / 16,240 B | 2026-08-26 16:27:09 | `5582C57208DC85B23C1142CBDA2BA91CF5E7CFD5F853B965DA142759371313E9` |

The programming candidate is
`build/Usart3Bringup/stm32f103rbt6_a33.elf`. It was not programmed because no
ST-Link was detected. Host CTest passed 16/16 with the existing `/W4 /WX`
policy, Stage 5B Python passed 29/29, and Stage 5C Python passed 12/12.

### Diagnostic contract confirmed from source

- RX buffer: 512-byte static circular DMA buffer.
- TX buffer: 256-byte static buffer; requests are copied before DMA starts.
- Test-frame trigger: explicit SWD call to
  `Stage5iUsart3Diagnostics_SendTestFrame()`; no automatic transmit.
- Echo: disabled and not implemented.
- Snapshot symbol/API: `Stage5iUsart3Diagnostics_Get()` returning
  `Stage5iUsart3Diagnostics`; `last_rx[64]`, `last_tx[64]`, lengths and totals
  are bounded fields in that structure.
- DMA position: `BSP_Uart3DmaGetRxPosition(512)`.
- IDLE/UART/DMA counters: `uart_events` inside the diagnostic snapshot, sourced
  from `BSP_Uart3DmaGetEvents()`.

### Validation matrix

| Item | Result | Evidence / reason |
|---|---|---|
| CubeMX two regenerations | PASS | second-generation key-file hashes identical; build passed |
| USART3 115200 8N1 | NOT RUN | no ST-Link/programmed target |
| PC10 TX | NOT RUN | no waveform or USB-TTL capture |
| PC11 RX | NOT RUN | no SWD snapshot |
| Partial Remap | NOT RUN | generated configuration retained; physical route unverified |
| USB-TTL to STM32 | NOT RUN | wiring not confirmed and target not programmed |
| STM32 to USB-TTL | NOT RUN | target not programmed |
| RX Circular DMA | NOT RUN | requires SWD counters/data |
| DMA position wrap | NOT RUN | requires at least 1,553 received bytes and SWD evidence |
| IDLE single event | NOT RUN | requires programmed target |
| IDLE multiple frames | NOT RUN | requires programmed target |
| 256-byte transfer | NOT RUN | requires physical byte comparison |
| 1,000-frame stress | NOT RUN | requires physical link and diagnostic capture |
| CH579 to STM32 | NOT RUN | CH579 raw UART0 mode/wiring not confirmed |
| STM32 to CH579 | NOT RUN | CH579 raw UART0 mode/wiring not confirmed |
| CH579 continuous communication | NOT RUN | physical prerequisite unavailable |
| USART2 RS232 regression | NOT RUN | requires USART3 traffic and physical RS232 link |
| USART2 RS485 regression | NOT RUN | requires USART3 traffic and physical RS485 link |
| USART1 W02 regression | NOT RUN | requires three-port physical concurrency |
| USART3 FC03 | NOT RUN | outside Stage 5I-A-HW scope; Stage 5I-B |
| USART3 FC06 | NOT RUN | outside Stage 5I-A-HW scope |
| USART3 FC16 | NOT RUN | outside Stage 5I-A-HW scope |

UART/DMA runtime error counters are **NOT READ**, not zero: the target was not
programmed or attached through SWD. Actual wiring is also **NOT CONFIRMED**.
Stage 5I-B entry conditions are not satisfied. The conclusion remains:
**Stage 5I-A CODE COMPLETE; hardware validation pending**.

## Stage 5I-A-HW continuation - 2026-08-28

### Programming and diagnostic corrections

ST-Link `E1007200D0D2139393740544` connected to the STM32F103RBT6 target at
3.29 V. STM32CubeProgrammer identified device ID `0x410`, programmed the
Usart3Bringup ELF, verified it, and reset the MCU without a mass erase. The
final diagnostic ELF SHA256 is
`35BC6F553660E95152DB6DABD2FA3A7600CAE180B32274DAB5F4E989AEB08E11`.

Hardware testing exposed two diagnostic-only limitations. Long continuous RX
spans were originally counted as capture overwrites because the bounded
snapshot consumed data only at IDLE. Commit `fe449ee` separated continuous DMA
observation from the IDLE snapshot and added a full-stream checksum. Commit
`df165f6` added a bounded, explicitly started, automatically terminating soak
sender so sustained tests do not repeatedly attach a debugger. Neither change
is present in normal Debug, Release, or BoardDiagnostics behavior because the
Stage 5I diagnostic option remains OFF in those images.

Final build results after both fixes:

| Image | Flash / RAM | Result |
|---|---:|---|
| Debug | 88,260 / 15,224 B | PASS, no warnings |
| Release | 75,824 / 15,208 B | PASS, no warnings |
| BoardDiagnostics | 125,836 / 15,120 B | PASS, 99.10% Flash |
| Usart3Bringup | 89,876 / 16,288 B | PASS, diagnostics ON |

Host CTest passed 16/16, Stage 5B Python passed 29/29, and Stage 5C Python
passed 12/12 after each diagnostic correction.

### USB-TTL validation

COM20 was connected as 3.3 V USB-TTL with TX to PC11, RX to PC10, and common
ground. The STM32 USART3 register divisor was `0x0138` at a 36 MHz APB1 clock,
corresponding to 115200 baud. AFIO MAPR was `0x02000010`, confirming USART3
partial remap while retaining SWD. PC10 was alternate-function push-pull and
PC11 was input with no internal pull.

| Test | Result | Evidence |
|---|---|---|
| PC to STM32 fixed frame | PASS | exact `55 AA 00 FF 01 03 12 34`, one IDLE, errors 0 |
| STM32 to PC fixed frame | PASS | exact `A5 5A 00 FF 81 03 56 78`, DMA complete and TC |
| RX 1/8/64/256 bytes | PASS | exact lengths, content snapshot and checksum |
| TX 1/8/64/256 bytes | PASS | complete byte comparison, mismatch index -1 |
| TX BUSY semantics | PASS | first 256-byte request OK, overlapping request BUSY |
| Circular wrap | PASS | 1,553 bytes, checksum `0x0002FD88`, half/full 3/3, position 17 |
| 100 IDLE frames | PASS | 100 frames, 800 bytes, 100 IDLE events |
| RX 1000 x 8 | PASS | 8,000 bytes, checksum `0x00006D60`, all errors 0 |
| RX 500 x 64 | PASS | 32,000 bytes, checksum `0x000F6180`, all errors 0 |
| RX 100 x 256 | PASS | 25,600 bytes, checksum `0x0031CE00`, all errors 0 |
| TX 1000 x 8 | PASS | 8,000/8,000 bytes, no mismatch |
| TX 500 x 64 | PASS | 32,000/32,000 bytes, no mismatch |
| TX 100 x 256 | PASS | 25,600/25,600 bytes, no mismatch |
| 60 s continuous RX | PASS | 691,968 bytes and checksum `0x05423880` match |

The continuous stream produced 1,352 RX half and 1,351 RX full callbacks,
ended at DMA position 256, and had zero stream overwrite, UART error, DMA
error, or IDLE queue overflow. Long IDLE-delimited snapshots are explicitly
marked capture-truncated while their complete byte count and checksum remain
verified.

### CH579 direct validation

CH579 PB7/TXD0 was connected to PC11, PB4/RXD0 to PC10, with common ground.
The connected CH579 ran its previously validated Stage 3B raw TCP-to-UART0
bridge at `192.168.1.100:5000`; COM3/UART1 at 115200 8N1 supplied internal
statistics. This mode forwards raw bytes and does not run the Stage 4A Modbus
master during the test.

| Direction/test | Result | Evidence |
|---|---|---|
| CH579 to STM32 fixed frame | PASS | exact 8-byte RX snapshot and checksum |
| STM32 to CH579 fixed frame | PASS | exact 8-byte TCP return |
| CH579 to STM32 1000 x 8 | PASS | 8,000 bytes/checksum match |
| CH579 to STM32 500 x 64 | PASS | 32,000 bytes/checksum match |
| CH579 to STM32 100 x 256 | PASS | 25,600 bytes/checksum match |
| STM32 to CH579 1000 x 8 | PASS | 8,000/8,000 bytes, no mismatch |
| STM32 to CH579 500 x 64 | PASS | 32,000/32,000 bytes, no mismatch |
| STM32 to CH579 100 x 256 | PASS | 25,600/25,600 bytes, no mismatch |
| 600 s bidirectional soak | PASS | 600/600 cycles, 19,200/4,800 bytes, errors 0 |

The final 600-second soak checksum was `0x00217D50`. STM32 soak
request/accepted counts were 600/600, busy/error counts were zero, and TX DMA
complete/TC counts were 600/600. CH579 reported zero TCP/UART buffer overflow,
TCP send error, and UART line error.

### USART2 concurrent regression

USART3/CH579 raw traffic continued for 630 seconds around each 600-second
USART2 window.

| Interface | FC03 result | Latency | Concurrent USART3 |
|---|---|---|---|
| RS232 COM9 | 2,150/2,150 PASS | mean 59.851 ms, P99 61.657 ms | 630 cycles, errors 0 |
| RS485 COM10 | 2,137/2,137 PASS | mean 60.079 ms, P99 61.973 ms | 630 cycles, errors 0 |

Both runs had zero timeout, CRC error, Modbus exception, retry recovery, and
retry failure. USART2 SWD statistics after the runs showed zero DMA error,
DMA overrun, IDLE queue overflow, UART parity/frame/noise/overrun error, TX DMA
error, and TX timeout. One supported DMA wrap-race recovery occurred without
transport or protocol loss. After RS485, the TX controller was IDLE, its last
error was NONE, and PA1 DE was released low.

Evidence summaries are under:

- `Results/stage5i_hw/20260828_rs232_concurrent.json` and `.csv`
- `Results/stage5i_hw/20260828_rs485_concurrent.json` and `.csv`
- `Results/stage5i_hw/20260828_ch579_final_10min_uart1.log`
- `Results/stage5i_hw/20260828_rs232_concurrent_uart1.log`
- `Results/stage5i_hw/20260828_rs485_concurrent_uart1.log`
- `Results/stage5i_hw/20260828_usart3_soak_flash.json` and `.log`

### Remaining gate

W02 was subsequently returned to advertising state as `W02_00DB8C`, address
`C8:46:82:00:DB:8C`. Read-only DEVICE_INFO and GET_CONFIG passed and reported
firmware `0x050A`, Schema V2, and Modbus map `0x0104`.

Four complete three-interface attempts were made with USART3/CH579 raw traffic,
COM5 RS485 FC03 traffic, and BLE telemetry/read-only commands. None of them
met the strict zero-error gate:

| Attempt | USART3 | RS485 | BLE |
|---|---|---|---|
| 1 | 630 cycles, errors 0 | 442/443 success; one storage-state timeout | 4,211 frames; 3 sequence gaps; 24/24 commands |
| 2 | 630 cycles, errors 0 | 482/483 success; one realtime timeout | 4,210 frames; 5 sequence gaps; 24/24 commands |
| 3, COM3 closed | 630 cycles, errors 0 | 108/109 success; one storage-state timeout | 4,212 frames; 2 sequence gaps; 24/24 commands |
| 4, replacement adapter | 630 cycles, errors 0 | 261/262 success; one sample-sequence timeout | 4,210 frames; 4 sequence gaps; 24/24 commands |

All BLE attempts had zero disconnect, command timeout/retry/result error,
transaction mismatch, CRC error, duplicate, and partial byte. Attempt 1 had
165 stream-resync bytes for three missing FAST sequences; attempt 2 had 275
resync bytes for five missing FAST sequences; attempt 3 had 110 resync bytes
for two missing FAST sequences; attempt 4 had 220 resync bytes for four missing
FAST sequences. The ratio of 55 discarded bytes per missing
56-byte FAST frame is consistent with one missing byte followed by
resynchronization across the remainder of that frame.

MCU evidence does not show a firmware queue or UART fault. After attempt 2 the
BLE scheduler reported 5,646 generated and 5,646 sent frames, with every queue,
transport-not-ready, encode, FAST, SLOW, and CHECKWEIGH drop counter at zero.
BLE transport reported zero RX overflow, UART error, TX error, priority queue
full, and transport reset. The missing bytes therefore occur after the STM32
USART1 transport boundary, in the W02/radio/Windows BLE path.

The RS485 timeouts occurred after clean request sequences. MCU
statistics showed every request reached USART2 as a complete 8-byte frame, but
one fewer response completed in each failed run. USART2 DMA error, overrun,
IDLE queue overflow, UART parity/frame/noise/overrun error, TX DMA error, and TX
timeout were all zero; the RS485 controller ended IDLE with last error NONE.
Standalone 600-second RS485 concurrency previously passed 2,137/2,137, so the
COM5 USB-RS485/host path remains intermittent under the final combined setup.
With the replacement adapter in attempt 4, MCU counters showed 1,321 valid and
addressed FC03 frames and 1,321 completed responses, with zero server CRC,
Framer, DMA, UART, TX, and DE-controller error. The PC still missed one
response, locating that timeout after the STM32 response boundary in the
adapter/USB/Windows receive path.

Failed-run evidence is under:

- `Results/stage5i_hw/20260828_final_threeway_rs485.json`
- `Results/stage5i_hw/20260828_final_threeway_ble/summary.json`
- `Results/stage5i_hw/20260828_final_threeway_rerun_rs485.json`
- `Results/stage5i_hw/20260828_final_threeway_rerun_ble/summary.json`
- `Results/stage5i_hw/20260828_final_clean_rerun_rs485.json`
- `Results/stage5i_hw/20260828_final_clean_rerun_ble/summary.json`
- `Results/stage5i_hw/20260828_final_newadapter_rs485.json`
- `Results/stage5i_hw/20260828_final_newadapter_ble/summary.json`

The final gate requires a clean 600-second rerun after the external BLE and
USB-RS485 links are corrected or replaced. Repeating unchanged hardware while
allowing retries would hide the observed failures and is not accepted.

USART3 FC03, FC06, and FC16 remain **NOT RUN - outside Stage 5I-A-HW scope**.
Until the strict three-interface concurrency gate passes, the conclusion remains:
**Stage 5I-A CODE COMPLETE; hardware validation pending**.

## Stage 5I-B architecture review - 2026-08-28

Stage 5I-B starts from commit `95ed17b`. The Stage 5I-A USART3/CH579 raw
transport result is accepted as an input. The intermittent external RS485 PC
adapter timeout and W02/Windows BLE sequence gaps remain open release gates;
they are not attributed solely to either adapter and are not reported as
zero-error passes.

### Existing singleton to instance mapping

| Existing mutable singleton | Stage 5I-B owner |
|---|---|
| Framer state, 256-byte RX frame and length | One `ModbusRtuFramer` per port |
| RTU timing and TIM4 pending event | Per-framer timing and software deadline from the common cycle clock |
| Framer counters and last DMA position | One framer statistics block per port |
| Server state and configuration snapshot | One `ModbusRtuServer` per port |
| 256-byte request and response buffers | Private buffers in each server instance |
| Register scratch buffer | Private server scratch buffer; register values still come from the shared model |
| Server counters and delayed-response state | One statistics/state block per port |
| USART2 DMA receive bookkeeping | USART2 transport adapter only |
| RS485 DE, TX DMA and USART TC state | USART2 transport adapter only |
| USART3 DMA receive/TX bookkeeping | USART3 transport adapter only |
| Register map, weighing state and configuration edit | Shared `ModbusRegisterModel` and `CommandService` |
| SAVE/Flash operation | Shared `PersistenceManager`, one operation at a time |
| Modbus mailbox request fields | Shared mailbox with explicit requesting `CommandSource` ownership |

The current CRC/address/FC03/FC06/FC16/exception implementation remains one
protocol implementation. A second protocol copy or USART3-specific register
map is not introduced.

### Reviewed behavior and ownership rules

The existing USART2 path consumes circular DMA bytes through
`Uart2DmaTransport`, treats IDLE as an observation rather than a frame end,
then advances through t1.5 and t3.5 before submitting a frame. The server owns
address and CRC checks, FC03/FC06/FC16 dispatch, exception construction and
the response delay. `Rs485TxController` owns PA1 DE setup, DMA completion,
physical USART TC and the final 10 us DE hold. This adapter remains USART2
private.

The new protocol core receives an explicit instance and transport binding on
every stateful entry. Each port owns RX assembly, request/response buffers,
silence deadline, TX state and diagnostics. The common monotonic cycle clock
is only a time base: no shared timer owner or global selected-port variable is
used. Consequently, an IDLE, overflow, CRC error, DMA wrap or TX busy event on
one port cannot reset or overwrite the other port.

USART2 remains configured from `CommunicationConfig`. USART3 binds to the
Stage 5I-A circular RX DMA and normal TX DMA at fixed 115200 8N1 without DE.
The `Usart3Bringup` image retains sole ownership of USART3 for raw diagnostics;
normal dual-Modbus startup is excluded when that compile-time diagnostic is
enabled.

Both servers use the same active Slave ID and the same register model.
Business writes carry an explicit source (`MODBUS` for USART2 and a distinct
USART3 Modbus source) into the shared mailbox and `CommandService`. The
existing edit owner and BUSY semantics serialize concurrent writes and SAVE;
Flash remains a single commit-last operation. During Flash, both servers are
suspended and both receive backlogs are discarded before resume.

Communication changes keep the existing old-settings response guarantee. The
manager waits for both port responses to finish, but only stops and
reconfigures USART2 for baud/parity/stop-bit changes. A request received on
USART3 therefore completes on USART3 before USART2 changes. A Slave ID change
is committed once and then copied into both server snapshots. Failed apply
restores the one shared configuration and both snapshots.

`CommunicationManager_Process()` gives each port a bounded step and alternates
which port is serviced first on successive calls. It never drains an
unbounded backlog in one `App_Run()` iteration. Ordinary FC03 requests remain
independent and may have concurrent DMA responses; shared writes are submitted
in the deterministic rotating service order and preserve the existing BUSY
semantics.

Implementation, host/build measurements and hardware result tables will be
added below after their corresponding evidence is produced. No Stage 5I-B
hardware result is claimed by this architecture review.

### Stage 5I-B implementation and evidence

The implementation uses one protocol core with two static instances. USART2
is bound through `modbus_uart2_transport.c` to the existing circular DMA and
`Rs485TxController` (including PA1 DE setup, physical TC and the 10 us hold).
USART3 is bound through `uart3_modbus_transport.c` to the Stage 5I-A
`BSP_Uart3Dma*` circular RX/IDLE and normal TX APIs at fixed 115200 8N1. Its
producer and consumer positions, error latches and TX state are private to
that adapter. Raw `Usart3Bringup` keeps exclusive ownership of the same BSP
when enabled.

The Framer stores one silence position and one software deadline per instance,
using the common monotonic cycle counter only as a time base. IDLE therefore
starts or refreshes the t1.5/t3.5 sequence and never submits a frame by itself.
The manager alternates the first service port on each bounded process call;
each port consumes at most 128 bytes and at most one queued IDLE event per
call. Response buffers and delayed-response state remain in their originating
server, so a response cannot cross ports.

Writes and mailbox commands carry `CommandSource`. USART2 uses
`COMMAND_SOURCE_MODBUS`; USART3 uses `COMMAND_SOURCE_MODBUS_USART3`. The
shared mailbox rejects a different owner with `MODBUS_REGISTER_BUSY` until the
current owner executes or completes its transaction. SAVE remains delegated to
the existing shared persistence manager. Configuration application waits for
both server instances to become idle, then reconfigures USART2 only; USART3
stays fixed. A committed Slave ID is copied to both snapshots and a failed
apply restores the previous shared configuration.

Host evidence (MSVC developer environment):

| Test | Result | Evidence |
|---|---|---|
| Existing Host CTest | PASS | 16/16 tests |
| Dual instance FC03 | PASS | complete response bytes and CRC checked on both instances |
| Interleaved framing | PASS | independent Framer buffers/timers and source-port responses |
| CRC/address isolation | PASS | USART2 error/miss counters changed; USART3 stayed unchanged |
| Shared mailbox ownership | PASS | USART3 write rejected BUSY while USART2 transaction owned mailbox, then released |
| FC06 / FC16 semantics | PASS | existing Stage 5A/5B assertions, both server paths call the same model |
| DMA position regression | PASS | existing UART2 recovery test and bounded USART3 adapter |
| Stage 5B Python suite | NOT RUN | `pytest` is unavailable in the current Python environment |
| Stage 5C Python suite | NOT RUN | `pytest` is unavailable in the current Python environment |

Build evidence after the dual-port change:

| Image | Flash / RAM | Result | Change vs Stage 5I-A |
|---|---:|---|---:|
| Debug | 90,804 / 17,608 B | PASS | +2,544 / +2,384 B |
| Release | 78,072 / 17,576 B | PASS | +2,248 / +2,368 B |
| BoardDiagnostics | 126,172 / 17,504 B | PASS, 99.37% Flash | +336 / +2,384 B |
| Usart3Bringup | 90,520 / 17,216 B | PASS, diagnostics ON | +644 / +928 B |

The BoardDiagnostics image initially exceeded Flash by 3,092 B at `-O0`.
The existing project size policy was applied to the new protocol and adapter
translation units with `-Os`; no diagnostics or product behavior was removed.
The final image has 804 B Flash headroom and no compiler warnings.

Static checks: no dynamic allocation, `HAL_Delay`, blocking TX, duplicated
CRC/function-code implementation, global selected-port pointer, cross-port
mutable frame buffer or protocol logging were introduced. `git diff --check`
is clean.

Hardware status remains conditional. USART3/CH579 raw transport retains the
Stage 5I-A evidence (600 s bidirectional soak, zero transport errors), but
Stage 5I-B USART3 FC03/FC06/FC16 and dual-port target-side tests were not run
on the programmed target in this change. The previously recorded intermittent
COM5 RS485 timeout and W02 BLE sequence gaps remain open external-link
waivers. No Stage 5I-B final hardware PASS or tested tag is claimed.

Files changed for this stage:

* `Protocol/modbus/modbus_rtu_framer.[ch]` - explicit Framer instances and independent software timing.
* `Protocol/modbus/modbus_rtu_server.[ch]` - explicit server instances and transport-bound TX.
* `Protocol/modbus/modbus_rtu_transport.[ch]` - minimal protocol transport contract.
* `Drivers/serial/modbus_uart2_transport.[ch]` - USART2/RS485 binding.
* `Drivers/serial/uart3_modbus_transport.[ch]` - USART3/CH579 binding.
* `App/communication_manager.[ch]` - two-port lifecycle, fairness and shared apply rules.
* `Protocol/modbus/modbus_register_model.[ch]`, `modbus_command_mailbox.[ch]`, and
  `Protocol/command_service/*` - explicit source and mailbox ownership.
* `Tests/host/*`, `CMakeLists.txt` - regression coverage and image integration.

The next-stage input is a programmed board with the CH579 Modbus gateway
enabled and a reliable RS232/RS485 adapter. Product release still requires
the strict external three-interface 600 s gate with zero RS485 timeouts and
zero BLE sequence gaps.
