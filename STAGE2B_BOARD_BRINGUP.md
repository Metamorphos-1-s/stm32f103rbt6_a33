# Stage 2B: Board Bring-up and Raw Sampling

## 1. Scope and result

Stage 2B adds a bounded production consumer for the CS1237 FIFO, a HAL-free
raw-measurement domain model, and an optional non-blocking board diagnostic
firmware. It does not implement weight conversion, calibration, filtering,
zero/tare, menus, Modbus, Bluetooth protocol, or Flash writes.

The production default is `ENABLE_STAGE2B_BOARD_DIAGNOSTICS=0U`. It therefore
does not start display animation, buzzers, lamps, or W02 pulses automatically.

Hardware-owner-confirmed facts carried forward:

- PB12 high enables CS1237; PB12 low disables it.
- GRID1 through GRID6 map left-to-right to display positions 1 through 6.

All other physical behavior in this document is **NOT TESTED ON HARDWARE**.

## 2. Files

Added:

- `App/measurement_bridge.c`, `App/measurement_bridge.h`
- `Domain/measurement/raw_measurement.c`, `raw_measurement.h`
- `Diagnostics/stage2b_board_diagnostics.c`, `.h`
- `Diagnostics/stage2b_display_font.c`, `.h`
- `STAGE2B_BOARD_BRINGUP.md`

Modified:

- `App/app_main.c`, `app_state.h`, `device_manager.c`, `device_manager.h`
- `Config/project_config.h`
- `Domain/measurement/README.md`
- `Services/event_queue/event_queue.h`
- `Tests/host/CMakeLists.txt`, `README.md`, `mock_hal.h`, `mocks.c`,
  `test_stage2a.c`
- `CMakeLists.txt`, `.gitignore`

CubeMX `.ioc`, generated `Core`, startup, linker, and HAL files are unchanged.

## 3. Source tree

```text
stm32f103rbt6_a33/
|-- App/
|   |-- app_main.[ch]
|   |-- app_state.h
|   |-- device_manager.[ch]
|   |-- measurement_bridge.[ch]
|   `-- system_context.[ch]
|-- BSP/
|   |-- bsp_adc.[ch]
|   |-- bsp_board.[ch]
|   |-- bsp_gpio.[ch]
|   |-- bsp_time.[ch]
|   `-- bsp_uart.[ch]
|-- Config/
|   |-- default_config.[ch]
|   |-- device_config.h
|   `-- project_config.h
|-- Core/                         CubeMX generated
|-- Diagnostics/
|   |-- stage2a_driver_diagnostics.[ch]
|   |-- stage2b_board_diagnostics.[ch]
|   `-- stage2b_display_font.[ch]
|-- Domain/
|   |-- measurement/
|   |   |-- raw_measurement.[ch]
|   |   `-- README.md
|   |-- calibration/
|   `-- weight/
|-- Drivers/
|   |-- battery_adc/
|   |-- cs1237/
|   |-- output_gpio/
|   |-- tm1628/
|   `-- w02/
|-- Services/
|   |-- config_store/
|   |-- event_queue/
|   |-- fault_manager/
|   `-- scheduler/
|-- Tests/host/
|   |-- CMakeLists.txt
|   |-- README.md
|   |-- mock_hal.h
|   |-- mocks.c
|   `-- test_stage2a.c
|-- cmake/stm32cubemx/             CubeMX generated CMake
|-- Drivers/STM32F1xx_HAL_Driver/  vendor HAL
|-- CMakeLists.txt
|-- CMakePresets.json
|-- STM32F103XX_FLASH.ld
|-- startup_stm32f103xb.s
`-- stm32f103rbt6_a33.ioc
```

## 4. Raw measurement and bridge

`RawMeasurement` owns only hardware-independent raw state: latest valid
sample, signed minimum/maximum, accepted/invalid counts, and wrap-safe sample
interval statistics. The first valid sample does not update interval extrema.
It has no CS1237, HAL, GPIO, floating-point, allocation, filtering, or weight
conversion dependency.

`MeasurementBridge_Process(maximum_samples)` is the sole production bridge
from `CS1237_Sample` to `RawMeasurementSample`. `App_Run()` calls it on every
iteration with `MEASUREMENT_BRIDGE_MAX_SAMPLES_PER_RUN` (default 4), independent
of `EVENT_CS1237_SAMPLE_AVAILABLE`. A call consumes no more than that bound;
16 queued samples therefore require four calls. The FIFO remains reject-new.

The 20 ms task may publish at most one `EVENT_RAW_MEASUREMENT_UPDATED`. `arg0`
stores the latest signed raw value's bit pattern as `uint32_t`; consumers must
cast it back to `int32_t`. `arg1` is `accepted_sample_count`, and `source` is
`NULL`. The last-published count advances only after a successful queue push.

Overrun handling is edge-based: `DeviceManager` records backlog, total consumed
samples, and the new driver overrun count only when that count changes. With
`CS1237_OVERRUN_FATAL=1U` (default), it raises
`FAULT_CS1237_BUFFER_OVERRUN`; with 0 it records the driver error without
entering FAULT. It neither clears the FIFO nor overwrites old samples.

## 5. Application flow

Normal build:

```text
BOOT -> SELF_TEST -> LOAD_CONFIG -> DEVICE_INIT -> WARMUP -> RUN
```

Each `App_Run`: fast driver service, bounded measurement bridge, overrun
observation, scheduler, bounded event processing, diagnostics no-op, state
machine. The display stays blank, all 12 auxiliary LEDs and three lamps stay
off, both buzzers stay silent, W02 stays released, battery updates every 500 ms,
and UARTs do not transmit proactively.

Diagnostic build (`ENABLE_STAGE2B_BOARD_DIAGNOSTICS=1U`):

```text
BOOT -> SELF_TEST -> LOAD_CONFIG -> DEVICE_INIT -> WARMUP -> DIAGNOSTIC
blank 300 ms -> A-G+DP left-to-right -> UP left-to-right
-> DOWN left-to-right -> 123456 -> C-00xx -> live raw hex
```

All transitions use `BSP_TimeNowMs()` and unsigned wrap-safe elapsed checks.
The device manager, FIFO bridge, scheduler, TM1628 scan, battery sampling, and
event handling continue in DIAGNOSTIC. The battery page is selected explicitly
with `Stage2B_DiagnosticsSelectState(STAGE2B_DIAG_STATE_LIVE_BATTERY)`; live raw
can be selected the same way. A debugger call to `App_ExitDiagnostics()` stops
all diagnostic outputs and transitions to RUN. Entering FAULT stops diagnostic state, turns off
all outputs, releases W02 and TM1628, and puts CS1237 in its safe state.

## 6. Diagnostic display

The local seven-segment font supports `0-9`, `A-F`, `H`, `I`, `L`, `P`, `U`,
dash, and blank. It produces logical `BOARD_SEG_*` masks; only the board-map
layer translates them to TM1628 physical segments.

- Raw code: lower 24 bits displayed as six uppercase hexadecimal digits.
  Examples: `0x000000 -> 000000`, `0x7FFFFF -> 7FFFFF`,
  `0x800000 -> 800000`, `0xFFFFFF -> FFFFFF`. No kg label or conversion.
- No raw data or CS1237 not running: `------`.
- Battery: integer millivolts only; 12600 mV is rendered as `12.600` using the
  decimal point after `12`. Invalid data is `------`; overrange is `HI`.
- Raw key mask: `P-0000` through `P-001F`. A changed mask remains visible for
  1000 ms and then returns to live raw. It assigns no key function semantics.

## 7. Safe manual output tests

Debugger-callable interfaces:

- `Stage2B_DiagnosticsRequestOutputTest(output, duration_ms)`
- `Stage2B_DiagnosticsRequestLampTest(output, duration_ms)`
- `Stage2B_DiagnosticsRequestW02Pulse(duration_ms)`
- `Stage2B_DiagnosticsGetSnapshot()` for a read-only SWD Watch snapshot

Lamp requests allow 50-2000 ms. Internal and external buzzer requests allow
20-300 ms. Invalid durations are rejected, only one output test may be active,
and elapsed tests are switched off on the next main-loop pass even if that pass
is late. No external output is tested automatically.

W02 requests go through `W02PwrKey_RequestPulse`, allow only 50-200 ms, and
retain the driver's 250 ms hard release. Initialization and physical keys never
request a pulse. Recommended board-test request: 80 ms.

## 8. Automated verification and resources

Host tests compile production logic with HAL-free mocks under MSVC `/W4 /WX`.
They cover all requested RawMeasurement, bounded bridge/FIFO, event throttling,
hex/battery/key formatting, output limits/timeout, W02 limits, wrap-around state
timing, and FAULT shutdown cases.

| Build | Result | FLASH | RAM | Delta from Stage 2A |
|---|---|---:|---:|---:|
| Host tests | PASS, 1/1 CTest | n/a | n/a | n/a |
| Debug, diagnostics off | 0 errors, 0 warnings | 21392 B | 2960 B | +1252 B / +112 B |
| Release, diagnostics off | 0 errors, 0 warnings | 12912 B | 2960 B | +704 B / +120 B |
| Debug, diagnostics on | 0 errors, 0 warnings | 24772 B | 2992 B | informational |

Stage 2A baselines: Debug 20140 B/2848 B; Release 12208 B/2840 B.

Static review: no dynamic allocation, floating point, recursion, FreeRTOS, or
`HAL_Delay`; no physical GPIO/HAL references above BSP; no business code was
added to CubeMX `main.c`.

## 9. Board acceptance procedure

Before power-up: verify rails and SWD, limit the supply current, keep battery
input at or below 12.6 V, disconnect high-power loads where practical, and
prepare a meter plus oscilloscope or logic analyzer.

### Static and CS1237 checks

- Verify PA1 low (RS485 receive), PA8 and PC7-PC9 released high, loads off,
  PB12 initial/enable transition, PB10 idle low, and PB11 DRDY low.
- Measure SCLK high well below 100 us and configured microsecond high/low phases.
- Confirm internal-reference config readback `0x0C`, settling completion, increasing
  `raw_sample_count`, bounded `cs1237_backlog`, and zero overrun.
- Check unloaded variation, monotonic response under load, and return near the
  original code after unloading. Confirm RUN/DIAGNOSTIC does not enter FAULT.

Result: **NOT TESTED ON HARDWARE**.

### TM1628 and keys

- Confirm no random startup segments, left-to-right `123456`, every A-G/DP,
  every UP/DOWN LED, no cross-wiring, GRID7 clear, STB idle high, valid CLK/DIO
  waveform, and DIO release during key reads.
- Press each physical key alone, release it, test supported combinations and a
  hold, and verify scanning does not disturb the display.

| Physical key, left-to-right | Observed raw mask | Final bit mapping |
|---:|---:|---:|
| 1 | NOT TESTED ON HARDWARE | bit? |
| 2 | NOT TESTED ON HARDWARE | bit? |
| 3 | NOT TESTED ON HARDWARE | bit? |
| 4 | NOT TESTED ON HARDWARE | bit? |
| 5 | NOT TESTED ON HARDWARE | bit? |

No Function/Tare/Zero/*/# semantics may be assigned until this table is filled.

### Battery ADC

Use `error_mv = software_mv - meter_mv` and
`error_ppm = error_mv * 1000000 / meter_mv`. Record recommendations only; do
not write Flash in Stage 2B.

| Point | Meter battery mV | PC0 mV | ADC average | Software mV | Error mV | Error ppm |
|---|---:|---:|---:|---:|---:|---:|
| Low | NOT TESTED ON HARDWARE | | | | | |
| Mid | NOT TESTED ON HARDWARE | | | | | |
| Near 12600 mV | NOT TESTED ON HARDWARE | | | | | |

Recommended `calibration_gain_ppm`: **NOT TESTED ON HARDWARE**.
Recommended `calibration_offset_mv`: **NOT TESTED ON HARDWARE**.

### Outputs and W02

Request green/yellow/red for 500 ms and each buzzer for 100 ms. Confirm polarity,
exclusive activation, automatic shutoff, no sustained sound, and FAULT shutoff.
Request W02 for 80 ms and measure idle high, approximately 80 ms low, no normal
pulse above 200 ms, 250 ms hard release under delayed servicing, and no startup
pulse.

Result: **NOT TESTED ON HARDWARE**.

### Waveform record

| Signal/check | Instrument result |
|---|---|
| CS1237 SCLK high/low and maximum high | NOT TESTED ON HARDWARE |
| CS1237 DRDY/data timing | NOT TESTED ON HARDWARE |
| TM1628 STB/CLK/DIO timing and DIO release | NOT TESTED ON HARDWARE |
| W02 80 ms and 250 ms hard release | NOT TESTED ON HARDWARE |
| DWT delay accuracy | NOT TESTED ON HARDWARE |
| USART1/USART2 idle and waveform | NOT TESTED ON HARDWARE |

## 10. Remaining hardware uncertainty and Stage 3

Unconfirmed items are the CS1237 data/timing and load direction, all TM1628
physical segments, five physical-key bit positions, battery error/calibration,
lamp and buzzer active levels, W02 pulse timing, and DWT/serial waveforms. They
must remain marked **NOT TESTED ON HARDWARE** until measured.

After board acceptance, Stage 3 should freeze the measured mappings and
calibration recommendations, then add metrology conversion/filtering/stability
logic and formal key semantics behind the existing Domain/App boundaries. Do
not start menu, protocol, or persistent calibration work until the raw sampling
and board records above are accepted.
