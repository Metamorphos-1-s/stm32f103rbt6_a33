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
