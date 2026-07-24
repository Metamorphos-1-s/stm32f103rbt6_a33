# Stage 5B: Modbus RTU DMA Transport

## Baseline and scope

Baseline commit: `f6ca281d016997446bf5daf47f1250d70798ad78`.

This stage adds bounded USART2 DMA transport, hardware-timed RTU framing,
FC03/FC06/FC16, RS485 direction control, delayed communication apply,
diagnostics, and host tests. Stage 5A register addresses, token deduplication,
staging, Schema V2, A/B flash, CRC32, commit-last, PVD gating, CS1237 internal
REFOUT/channel A, and default `0x0C` are preserved.

Implementation references are ST RM0008, the vendored STM32F1 HAL, MODBUS
Application Protocol V1.1b3, and MODBUS Serial Line Guide V1.02.

## Directory and layering

```text
App/communication_manager.[ch]
Diagnostics/stage5b_modbus_diagnostics.[ch]
Protocol/modbus/
  modbus_crc16.[ch]
  modbus_rtu_timing.[ch]
  modbus_rtu_framer.[ch]
  modbus_rtu_server.[ch]
Drivers/serial/
  uart2_dma_transport.[ch]
  rs485_tx_controller.[ch]
BSP/
  bsp_uart_dma.[ch]
  bsp_rtu_timer.[ch]
Core/Inc/dma.h, Core/Inc/tim.h
Core/Src/dma.c, Core/Src/tim.c
Tests/host/test_stage5b.c
Tests/host/stage5b_transport_adapters.c
```

Protocol sources have no HAL dependency. IRQ handlers only forward to BSP;
copying, CRC, parsing, register access, commands, and flash run in the main loop.

## CubeMX configuration

STM32CubeMX 6.15.0 loaded, saved, and generated the project in quiet script
mode. USART2 RX uses DMA1 Channel 6, peripheral-to-memory, circular, byte
alignment, memory increment, high priority. TX uses DMA1 Channel 7,
memory-to-peripheral, normal, byte alignment, memory increment, medium priority.
TIM4 uses the 72 MHz APB1 timer clock with prescaler 71 for 1 MHz ticks. DMA1
Ch6/Ch7, TIM4, and USART2 IRQs use priority 5.

PA2/PA3 remain USART2, PA1 remains output-low `MCU_DE`, and HCLK remains
72 MHz. ADC1/PC0, USART1, PB10/PB11/PB12, PC7/PC8/PC9, SWD, linker regions,
and the no-RTOS configuration are unchanged.

## RX DMA and frame timing

RX uses a static 1024-byte power-of-two ring. Producer/consumer positions are
bounded to 128 consumed bytes per call. HT, TC, IDLE, DMA errors, and UART
PE/FE/NE/ORE are counted in ISR context. Main-loop recovery restarts RX DMA and
invalidates uncertain data. A visible producer overrun discards the frame.

IDLE is not frame completion. Since it represents approximately one character
of silence, TIM4 waits the remaining `t1.5 - character_time`, verifies the DMA
position, then waits to t3.5 and verifies again. Data between t1.5 and t3.5
invalidates the frame. Frames below 4 bytes are dropped; data beyond the static
256-byte ADU buffer is discarded until complete silence.

At 19200 baud and below, t1.5/t3.5 use integer ceiling calculations from start
+ 8 data + optional parity + 1/2 stop bits. Above 19200 they are 750/1750 us.
No floating point, SysTick framing, busy wait, or `HAL_Delay` is used.

A debugger halt can let DMA wrap while IRQ flags collapse. The firmware detects
visible HT/TC wrap counts and producer lag, but F1 hardware cannot reconstruct
an arbitrary hidden wrap count. Debug-halt and flash-stall behavior therefore
remain board-validation items.

## CRC and server

CRC-16/MODBUS uses initial `FFFF`, reflected polynomial `A001`, no final XOR,
and low-byte-first wire order. Host vectors verify `123456789 -> 4B37` and
`01 03 00 00 00 0A -> CDC5`.

Supported functions are FC03 (1..125), FC06, and FC16 (1..123). All use the
existing Stage 5A model. FC03 receives a consistency snapshot; FC16 retains
validate-before-mutate and EXECUTE-last behavior. Exceptions are 01 illegal
function, 02 illegal/read-only address, 03 illegal value/shape, 04 device
failure, and 06 busy. Bad CRC, short frames, other addresses, and broadcasts
are silent. Address 0 writes are counted but never executed.

## RS485 and apply

PA1 is low at idle and on all abort paths. TX asserts DE, waits the 10 us
development setup, starts DMA, then explicitly waits for USART `TC` before the
10 us hold and DE release. Setup/hold values require transceiver-datasheet and
oscilloscope verification. RS232 uses the identical response bytes.

Communication staging remains in `01A0-01BF`; mailbox command 24 requests
apply. The request is answered using old settings. After response TC and DE
release, RX DMA stops, UART/timing restart with the candidate, and RAM config is
committed dirty. Failure rolls back to the old settings. APPLY never saves.

## Flash strategy

Modbus SAVE is queued, its response completes through USART TC, then the
manager requests persistence. RX DMA remains configured while storage is busy,
but the RTU server is suspended. On completion, pending bytes are discarded and
framing restarts. This does not guarantee preservation of requests during a
synchronous page erase; masters must timeout and retry. BoardDiagnostics rejects
SAVE before queueing. Existing token caching prevents repeated actions.

## Verification and resources

MSVC C11 `/W4 /WX` host tests compile production communication manager, CRC,
timing, framer, RS485 TX controller, server, and register-model code. Stage 5B
runs 1,079 checks,
including 512 deterministic bad
frames. Stage 4A, 4B, 4B-R, 5A, and 5B targets pass.

Final clean ARM results:

| Build | FLASH | RAM | CONFIG_A/B |
|---|---:|---:|---:|
| Debug | 95,528 B | 10,592 B | 0 / 0 |
| Release | 51,736 B | 10,608 B | 0 / 0 |
| BoardDiagnostics | 93,584 B | 10,584 B | 0 / 0 |

Relative to Stage 5A, the deltas are Debug +23,544 FLASH/+2,720 RAM,
Release +12,124/+2,728, and BoardDiagnostics +23,480/+2,720. Highest flash
load ends are Debug `0x08017528`, Release `0x0800CA18`, and BoardDiagnostics
`0x08016D90`, all below `0x0801F000`. The largest new static object is the
1,024-byte RX DMA buffer; ADU request, response, and framer buffers are each
256 bytes and the FC register workspace is 250 bytes.

## Hardware status

**NOT TESTED ON HARDWARE.**

RS232, RS485 polarity/termination/bias, DMA continuity, IDLE/TIM4 timing, DE
edges, flash interaction, and one-hour 115200 polling need board and logic-
analyzer verification. Stage 5C should start only after those checks.
