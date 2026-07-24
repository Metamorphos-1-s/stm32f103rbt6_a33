# Stage 4A Local UI and Calibration Handover

## 1. Baseline and scope

Stage 4A was implemented on main commit
`6e1be20817dbe06e81846d4752067135e47be865` (Stage 3). It adds local key
semantics, six-digit weight presentation, annunciators, a transport-neutral
command layer, RAM configuration transactions, a non-blocking startup self
test, a local menu, and first-time two-point calibration.

It does not implement Modbus/BLE framing, UART transport, Flash erase/write,
configuration A/B slots, factory reset, external alarm behavior, automatic
zero tracking, multi-point calibration, USB, IWDG, or legal-metrology sealing.

## 2. Relevant tree and files

Vendor, CubeMX, and generated build contents are omitted. CubeMX-owned files
were not modified.

```text
stm32f103rbt6_a33/
|-- App/
|   |-- app_main.c
|   |-- calibration_controller.c/.h       [new]
|   |-- config_application.c/.h           [new]
|   |-- device_manager.c/.h
|   |-- measurement_bridge.c/.h
|   |-- metrology_manager.c/.h            [extended]
|   |-- self_test_controller.c/.h         [new]
|   `-- system_context.c/.h               [extended]
|-- Config/
|   |-- default_config.c/.h
|   |-- device_config.h
|   |-- project_config.h                  [Stage 4A timing limits]
|   `-- runtime_state.h
|-- Diagnostics/                          [Stage 2A/2B/3 retained]
|-- Domain/
|   |-- calibration/
|   |   |-- calibration_model.c/.h
|   |   `-- raw_calibration_stability.c/.h [new]
|   |-- filter/
|   |-- measurement/
|   |-- stability/
|   `-- zero_tare/
|-- Drivers/
|   |-- battery_adc/
|   |-- cs1237/
|   |-- output_gpio/
|   |-- tm1628/
|   `-- w02/
|-- Protocol/
|   `-- command_service/
|       |-- command_types.h               [new]
|       |-- command_service.c/.h          [new]
|       `-- README.md
|-- Services/
|   |-- config_edit/config_edit.c/.h      [new]
|   |-- config_store/
|   |-- event_queue/
|   |-- fault_manager/
|   `-- scheduler/
|-- UI/
|   |-- display_controller.c/.h           [new]
|   |-- display_model/
|   |   |-- display_types.h               [new]
|   |   |-- display_codes.c/.h            [new]
|   |   |-- display_formatter.c/.h        [new]
|   |   |-- display_model.c/.h            [new]
|   |   `-- README.md
|   |-- key_service/
|   |   |-- key_types.h                   [new]
|   |   |-- key_map.c/.h                  [new]
|   |   |-- key_service.c/.h              [new]
|   |   `-- README.md
|   `-- menu_controller/
|       |-- menu_types.h                  [new]
|       |-- menu_controller.c/.h          [new]
|       `-- README.md
|-- Tests/host/
|   |-- CMakeLists.txt                    [extended]
|   |-- test_stage2a.c                    [runner extended]
|   `-- test_stage4a.c                    [new]
|-- CMakeLists.txt                        [extended]
`-- STAGE4A_LOCAL_UI_CALIBRATION.md       [new]
```

Other modified files are `Services/fault_manager/fault_manager.h` and the
three UI/Command READMEs. `DeviceConfig` layout was not changed, so schema
version remains 2.

## 3. Key mapping and events

The development mapping is:

| Logical key | TM1628 board mask bit |
|---|---:|
| FUNCTION | bit 0 |
| TARE | bit 1 |
| ZERO | bit 2 |
| STAR | bit 3 |
| HASH | bit 4 |

This is a **DEVELOPMENT DEFAULT - VERIFY ON HARDWARE**. `KeyMap_Validate`
rejects duplicate and out-of-range bits, so the physical panel can later be
corrected only in the map.

Each key has independent 30 ms debounce. A stable press produces PRESSED; a
stable release produces RELEASED and SHORT unless LONG was already emitted.
LONG is emitted once at 1000 ms. STAR/HASH alone repeat from 600 ms at 150 ms
intervals. The fixed 16-entry queue rejects new events when full, preserves old
events, counts drops, supports multiple keys, and uses wrap-safe timestamps.

## 4. Normal-page semantics

| Key | Short | Long |
|---|---|---|
| FUNCTION | NET -> GROSS -> TARE -> BATTERY -> NET | enter menu |
| TARE | tare through CommandService | clear tare |
| ZERO | daily zero | reset daily zero |
| STAR | accepted manual-output request, no transport | status page |
| HASH | quick NET/GROSS toggle | reserved, no action |

Rejected zero/tare actions show centralized noCAL, unstable, overload, or
action-error codes. Permanent calibration cannot be changed from the normal
page. Calibration starts only through the menu.

## 5. CommandService

`CommandService_Execute(const CommandRequest *, CommandResponse *)` is shared by
local keys and future Modbus, BLE, USB, and diagnostic adapters. Requests have
only fixed integer fields, never dynamic pointers. The service delegates weight
actions to `MetrologyManager`, configuration to `ConfigEdit` and
`ConfigApplication`, and calibration construction to `CalibrationModel`.

GET_WEIGHT returns net, gross, and status. ZERO/RESET_ZERO/TARE/CLEAR_TARE map
the existing weight action results to one command result vocabulary.
SET_WEIGHT_VIEW uses the controlled SystemContext setter. Manual output returns
ACCEPTED but sends no UART data. SAVE and FACTORY_RESET return
`STORAGE_UNAVAILABLE`; they never claim success.

The service includes no HAL, GPIO, display, UART, or concrete transport parser.

## 6. RAM configuration transaction

`ConfigEdit_Begin` copies current configuration into fixed working storage.
Every field write is range checked before narrowing. Validation uses
`MetrologyConfig_Validate`, calibration consistency checks, CS1237 enum bounds,
and brightness bounds. Cancel discards the copy. A successful command commit
passes the complete candidate to `ConfigApplication`.

`ConfigApplication` rejects unsupported sample-rate or gain changes (Stage 4A
strategy A), applies brightness and a prepared replacement WeightEngine, then
updates the private SystemContext copy and marks `config_dirty`. Failure restores
the old brightness/engine before returning. New calibration clears the old
daily-zero and tare state; ordinary supported parameter edits preserve them.

Supported runtime fields include brightness, filter, stability, zero range,
overload threshold, division, capacity/decimals, and tare retention. CS1237
sample rate and gain are visible but read-only because changing their structure
without a complete stop/FIFO/config/readback/settling flow would be unsafe.

## 7. Display model and formatting

`DisplayModel` stores six logical segment masks, 12 annunciator bits,
brightness, enable, page, and revision. Identical content does not increment
revision. `DisplayController` reads `WeightSnapshot`, battery state, and app
state, maps logical segments with `TM1628_BoardMapSegments`, and writes only the
existing TM1628 shadow RAM. Stage 2B diagnostics suppress formal rendering.

`WeightValue` is already the configured least display unit. The formatter does
no unit conversion or weight calculation and uses no floating point or
`sprintf`. Decimal points share a digit position. Examples:

```text
decimal_places=3: 0 -> 0.000, 1234 -> 1.234,
                  12345 -> 12.345, -1234 -> -1.234
decimal_places=2: 1234 -> 12.34
```

Leading digit positions are blank. `INT32_MIN` is handled through unsigned
magnitude. Values that do not fit show HI/Lo rather than truncated digits.
Missing valid calibration/weight shows noCAL; overload shows OL.

Annunciators are centralized:

| SG_UP 1..6 | NET | GROSS | TARE page | STABLE | ZERO | BATTERY page |
|---|---|---|---|---|---|---|
| SG_DN 1..6 | no calibration | tare active | overload | menu/calibration | comm reserved | battery reserved |

STABLE, ZERO, tare, overload, and calibration state come directly from
`WeightSnapshot`; the UI never recomputes them. These are not the external
red/yellow/green limit lamps.

## 8. Non-blocking startup self test

With Stage 2B diagnostics disabled, device initialization is followed by:

```text
clear 100 ms -> six-digit 8 walk -> six SG_UP walk -> six SG_DN walk
-> internal buzzer 80 ms -> "A33 4A" 500 ms -> WARMUP/RUN
```

All transitions use wrap-safe time comparisons. `App_Run` continues fast device
processing and bounded MeasurementBridge consumption. The test never drives the
external buzzer, external lamps, or W02 PWRKEY. Entering FAULT cancels the test
and turns all outputs off. `SELF_TEST_INTERNAL_BEEP_ENABLED` can disable the
internal beep.

When Stage 2B diagnostics are compiled in, diagnostics take priority and the
formal self-test/display are skipped. The repository's existing diagnostic
macro default was not changed; production formal UI builds must override it to
0 as already supported by the preprocessing guard.

## 9. Menu

```text
Calibration
Capacity
Division
Decimals
Filter
Stability hold
Zero range
Overload
Brightness
Sample rate (read-only in Stage 4A)
Gain (read-only in Stage 4A)
Tare retention
Save (noSAVE)
Exit
```

HASH/STAR move next/previous or increase/decrease. FUNCTION confirms, TARE
cancels/returns, ZERO changes numeric edit step, and FUNCTION long exits without
committing an active edit. STAR/HASH repeat is accepted in edit mode. A 30-second
timeout cancels uncommitted work. Menu processing never blocks or stops sampling.

## 10. Interactive two-point calibration

```text
MENU -> confirm empty -> wait raw stability -> average zero window
-> enter span weight -> prompt load -> wait raw stability
-> average span window -> build candidate/preview -> confirm RAM commit
-> rAnOnL -> MENU
```

The controller consumes `WeightSnapshot.filtered_raw` only after FILTER_READY.
It does not require a valid weight calibration. `RawCalibrationStability` uses
an independent 8-sample raw-count window, 50-count entry threshold, and 500 ms
hold; when stable, all window samples are averaged with checked integer rounding.
Those are **DEVELOPMENT DEFAULT - NOT VERIFIED ON SCALE HARDWARE** values.

Span weight must be positive and at most capacity. `CalibrationModel_Build`
enforces raw span safety and supports positive or negative sensor direction.
The candidate exists in PREVIEW without modifying the old configuration.
Cancel, too-small span, and other errors preserve old calibration. Confirm sends
fixed calibration commands through CommandService; application updates RAM,
reinitializes filter/stability, clears old volatile zero/tare, and sets dirty.

Sampling and bounded FIFO consumption continue throughout calibration.

## 11. Flash and persistence

All Stage 4A edits and calibration exist in RAM only. `ConfigStore_Save` remains
an IO error placeholder; there is no Flash operation or linker reservation.
The UI shows `rAnOnL` after calibration and `noSAVE` for Save. Reboot loss is
expected, and `config_dirty` remains true. No code reports permanent success.

## 12. Test and build results

The host target compiles production Stage 2A/2B/3/4A sources using MSVC C11
`/W4 /WX`; CTest result is 1/1 passed. Coverage includes:

- mapping validation, debounce, short/long/repeat, multi-key, wrap, queue full;
- signed/decimal formatting, INT32_MIN, noCAL/OL, revision and annunciators;
- command mappings across sources, save/manual-output behavior, GET_WEIGHT;
- RAM begin/set/validate/commit/cancel, narrowing guards, unsupported CS changes;
- menu navigation/edit/cancel/timeout;
- self-test progression, wrap, internal beep off, no external/W02 action;
- raw stability average/wrap and uncalibrated first calibration;
- positive/reverse sensors, Preview isolation, cancel, span guards, small span;
- all existing Stage 2A, Stage 2B, and Stage 3 regressions, including REFOUT.

ARM GNU clean builds completed with zero errors and zero warnings:

| Build | Stage 4A FLASH | Stage 4A RAM | Stage 3 baseline | Delta |
|---|---:|---:|---:|---:|
| Debug | 46368 B | 4384 B | 30952 B / 3568 B | +15416 B / +816 B |
| Release | 26156 B | 4392 B | 17768 B / 3568 B | +8388 B / +824 B |

STM32F103RB limits are 128 KiB Flash and 20 KiB RAM. Stage 4A uses 35.38% Flash
and 21.41% RAM in Debug, and 19.96% Flash and 21.45% RAM in Release, leaving
substantial resource margin.

Static scans found no dynamic allocation, floating point, recursion, FreeRTOS,
`HAL_Delay`, or `sprintf` weight formatting. Domain remains HAL-free. UI has no
HAL or physical GPIO dependency. CommandService has no HAL or transport
implementation dependency. CubeMX `.ioc`, Core, startup, linker, and HAL files
were not modified.

## 13. Compatibility and hardware status

Stage 2B raw hexadecimal, battery, raw-key-mask, bounded output, W02 tests, and
`App_ExitDiagnostics` remain compiled. Diagnostic raw mask bypasses KeyService.
Formal display is reinitialized after diagnostic exit. GRID1..GRID6 remain
left-to-right digits 1..6.

`CS1237_REFERENCE_OUTPUT_ENABLED` remains 1. A-channel, PGA128, 10 Hz, internal
REFOUT still encodes `0x0C`; existing host regressions pass.

No target board was available in this work session. All panel mapping, debounce
feel, repeat rate, digit/DP/negative ordering, message readability, annunciator
positions, self-test walk, internal buzzer polarity/volume, calibration capture,
real weights, sampling during menu/calibration, and FIFO overrun observations are
**NOT TESTED ON HARDWARE**.

Capacity, division, decimals, raw span, zero/overload range, filter, stability,
drift, repeatability, and corner loading remain **NOT VERIFIED ON SCALE
HARDWARE**. Host/synthetic tests are not physical metrology validation.

## 14. Stage 4B recommendation

Stage 4B should implement validated Flash A/B records with schema, CRC, sequence,
atomic selection, power-loss tests, and explicit save/factory-reset commands.
Only after that should `noSAVE`/`rAnOnL` become persistent-success behavior.
Keep Modbus/BLE adapters thin and route them through the existing CommandService.
Implement complete non-blocking CS1237 reconfiguration before making rate/gain
editable. Hardware verification should precede changing key map, filter,
stability, or raw calibration defaults.
