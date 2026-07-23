# Stage 1 Architecture And Handoff

## Added And Modified Files

- Modified: `stm32f103rbt6_a33.ioc`, `CMakeLists.txt`, `Core/Src/main.c`,
  `Core/Src/gpio.c`, `Core/Src/usart.c`, and `Core/Inc/main.h`.
- Added App: `app_main.c/.h`, `app_state.h`, and `system_context.c/.h`.
- Added BSP: `bsp_board`, `bsp_gpio`, `bsp_time`, `bsp_uart`, and `bsp_adc`
  header/source pairs.
- Added Config: `project_config.h`, `device_config.h`, `default_config.c/.h`,
  and `runtime_state.h`.
- Added Services: scheduler, event_queue, config_store, and fault_manager
  header/source pairs; watchdog placeholder README.
- Added placeholders: README files under the reserved Drivers, Domain, Protocol,
  and UI directories listed in the tree below.
- Added this handoff document. Build outputs under `build/` are generated artifacts.

CubeMX 6.15 loaded and generated the project after the hardware edits. A final
load verified PC0/ADC1_IN10, HCLK 72 MHz, USART1 9600 8N1, and released open-drain
W02 PWRKEY while preserving the application USER CODE blocks.

## Project Tree

```text
stm32f103rbt6_a33/
|-- App/
|   |-- app_main.c/.h
|   |-- app_state.h
|   `-- system_context.c/.h
|-- BSP/
|   |-- bsp_adc.c/.h
|   |-- bsp_board.c/.h
|   |-- bsp_gpio.c/.h
|   |-- bsp_time.c/.h
|   `-- bsp_uart.c/.h
|-- Config/
|   |-- default_config.c/.h
|   |-- device_config.h
|   |-- project_config.h
|   `-- runtime_state.h
|-- Core/                       CubeMX generated startup and HAL glue
|   |-- Inc/
|   `-- Src/
|-- Drivers/
|   |-- CMSIS/                  ST supplied
|   |-- STM32F1xx_HAL_Driver/   ST supplied
|   |-- battery_adc/README.md
|   |-- cs1237/README.md
|   |-- output_gpio/README.md
|   |-- tm1628/README.md
|   |-- uart_port/README.md
|   `-- w02/README.md
|-- Services/
|   |-- config_store/config_store.c/.h
|   |-- event_queue/event_queue.c/.h
|   |-- fault_manager/fault_manager.c/.h
|   |-- scheduler/scheduler.c/.h
|   `-- watchdog/README.md
|-- Domain/
|   |-- calibration/README.md
|   |-- filter/README.md
|   |-- limit_checker/README.md
|   |-- measurement/README.md
|   |-- stability/README.md
|   `-- zero_tare/README.md
|-- Protocol/
|   |-- ble_protocol/README.md
|   |-- command_service/README.md
|   |-- custom_protocol/README.md
|   `-- modbus_rtu/README.md
|-- UI/
|   |-- display_model/README.md
|   |-- key_service/README.md
|   `-- menu_controller/README.md
|-- cmake/                       CubeMX CMake support and toolchain
|-- build/                       Generated build output
|-- CMakeLists.txt
|-- CMakePresets.json
|-- CONFIGURATION_SUMMARY.md
|-- STM32F103XX_FLASH.ld
|-- startup_stm32f103xb.s
`-- stm32f103rbt6_a33.ioc
```

## Layer Responsibilities

- L0 Core/HAL: CubeMX-owned clocks, pins, peripheral handles, startup, and HAL.
- L1 BSP: semantic board signals, time, UART port mapping, and one-shot ADC access.
- L2 Drivers: reserved device-specific transports; no driver behavior in Stage 1.
- L3 Services: cooperative scheduling, FIFO events, storage contract, and fault bits.
- L4 Domain: reserved HAL-independent weighing rules and algorithms.
- L5 Protocol/UI: reserved wire formats and human interaction.
- L6 App: initialization order, bounded event dispatch, and state orchestration.
- Config: persistent parameter model, development defaults, and separate runtime state.

Dependencies point downward. App, Config, and Services do not include HAL headers or
use physical GPIO names. SystemContext storage is private and exposed read-only.

## Scheduler

The scheduler owns a fixed array of `SCHEDULER_MAX_TASKS`. Due checks use signed
interpretation of unsigned tick subtraction, so deadlines work through uint32 wrap
for intervals below 2^31 ms. Each task runs at most once per scheduler pass. When a
loop is late, its deadline advances by all elapsed periods: missed cycles are
skipped, not replayed, and the original phase does not drift. Stage 1 registers
empty 1, 10, 20, 100, and 500 ms tasks; the 1 ms task only services BSP safety.

## Event Queue

The queue is a fixed `EVENT_QUEUE_CAPACITY` FIFO with head, tail, and count. A full
queue rejects the new event, preserves pending events, and increments a 32-bit drop
counter. App processes at most `APP_MAX_EVENTS_PER_RUN` events per pass. It is main
context only; a future ISR producer requires a critical section or separate API.

## Configuration Model

`DeviceConfig` groups metrology, calibration, stability, communication, Bluetooth,
alarm, display, battery, and system settings. `RuntimeState` separately contains
the view, tare state, dirty flag, and boot count. Defaults are development values,
calibration is invalid, battery low alarm is disabled, and the divider is 30k/10k.
ConfigStore reports NOT_FOUND on load and IO_ERROR on save. Stage 6 is reserved for
Flash A/B slots, sequence numbers, and CRC32.

## BSP Interfaces

- `bsp_board`: software-only board wrapper; it never repeats HAL/MX initialization.
- `bsp_gpio`: RS485 direction, buzzers, limit lamps, CS1237 pins, TM1628 pins, and
  guarded W02 PWRKEY. Initial state is receive/off/low/released as applicable.
- `bsp_time`: HAL tick access and wrap-safe elapsed-time checks.
- `bsp_uart`: W02->USART1 and COM1->USART2 blocking transmit abstraction only.
- `bsp_adc`: ADC calibration, one-shot raw read, and integer millivolt conversion
  with uint64 intermediates. A 3.15 V ADC node converts to 12.6 V battery voltage.

## Main Initialization

`HAL_Init` -> clock -> GPIO -> USART1 -> USART2 -> ADC1 -> ADC calibration ->
`App_Init`. The main loop calls only `App_Run`. `App_Init` initializes BSP and
services, loads defaults after NOT_FOUND, creates the private context, registers
periodic tasks, pushes startup events, and starts in BOOT.

## Build

```powershell
cmake --preset Debug
cmake --build --preset Debug --clean-first
cmake --preset Release
cmake --build --preset Release --clean-first
```

The VS Code CMake Tools extension can select the Debug or Release preset and build
the `stm32f103rbt6_a33` target with the bundled GNU Arm toolchain configuration.

## Deliberately Not Implemented

CS1237 timing/configuration, TM1628 display/key access, weighing algorithms,
calibration, stability, zero/tare, limits and buzzer sequencing, display self-test,
Modbus/custom/BLE protocols, W02 AT flow, UART ring buffers, Flash erase/write,
IWDG enablement, USB, DMA, and FreeRTOS are not implemented.

## Stage 2 Starting Point

Start with independently testable Drivers: CS1237 non-blocking transport, TM1628
transport, UART interrupt receive/queued transmit, battery sampling policy, and
W02 PWRKEY state control. Keep measurement and UI logic out of these drivers, then
connect their completion signals to App through the event queue.
