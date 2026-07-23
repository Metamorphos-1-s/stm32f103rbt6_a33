# Stage 3 Metrology Core Handover

## 1. Scope and baseline

Stage 3 adds the HAL-free weighing core: checked integer arithmetic, raw-count
filtering, two-point calibration, gross/net calculation, daily zero, tare,
division quantization, stability detection, status flags, App orchestration,
rate-limited events, host synthetic inputs, and an SWD diagnostic snapshot.

The implementation baseline is main commit `269def2`. It does not implement
menus, production display pages, Modbus/BLE application protocols, alarm state
machines, Flash writes, automatic zero tracking, multi-point calibration,
temperature/non-linearity compensation, USB, or IWDG.

## 2. Relevant project tree

Generated build trees and vendor CMSIS/HAL contents are omitted; CubeMX-owned
files were not changed.

```text
stm32f103rbt6_a33/
|-- App/
|   |-- app_main.c/.h
|   |-- app_state.h
|   |-- device_manager.c/.h
|   |-- measurement_bridge.c/.h
|   |-- metrology_manager.c/.h              [Stage 3]
|   `-- system_context.c/.h
|-- BSP/                                     [unchanged boundary]
|-- Config/
|   |-- default_config.c/.h
|   |-- device_config.h
|   |-- project_config.h
|   `-- runtime_state.h
|-- Core/                                    [CubeMX, unchanged]
|-- Diagnostics/
|   |-- stage2a_driver_diagnostics.c/.h
|   |-- stage2b_board_diagnostics.c/.h
|   |-- stage2b_display_font.c/.h
|   `-- stage3_metrology_diagnostics.c/.h    [Stage 3]
|-- Domain/
|   |-- README.md
|   |-- calibration/
|   |   |-- README.md
|   |   `-- calibration_model.c/.h
|   |-- filter/
|   |   |-- README.md
|   |   `-- weight_filter.c/.h
|   |-- limit_checker/README.md              [reserved]
|   |-- measurement/
|   |   |-- README.md
|   |   |-- metrology_config_validator.c/.h
|   |   |-- raw_measurement.c/.h
|   |   |-- weight_engine.c/.h
|   |   |-- weight_math.c/.h
|   |   `-- weight_types.h
|   |-- stability/
|   |   |-- README.md
|   |   `-- stability_detector.c/.h
|   `-- zero_tare/
|       |-- README.md
|       `-- zero_tare.c/.h
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
|   |-- synthetic_scale_source.c/.h          [host only]
|   |-- test_stage2a.c
|   `-- test_stage3.c
|-- CMakeLists.txt
|-- CMakePresets.json
|-- STAGE1_ARCHITECTURE.md
|-- STAGE2A_DRIVERS.md
|-- STAGE2B_BOARD_BRINGUP.md
`-- STAGE3_METROLOGY_CORE.md
```

## 3. Added and modified files

New production files are `App/metrology_manager.*`,
`Diagnostics/stage3_metrology_diagnostics.*`, all Stage 3 files shown under
`Domain`, and the corresponding READMEs. New host-only files are
`Tests/host/test_stage3.c` and `synthetic_scale_source.*`.

Modified integration/configuration files are `App/app_main.c`,
`App/measurement_bridge.c`, `App/system_context.*`, `Config/device_config.h`,
`Config/project_config.h`, `Services/fault_manager/fault_manager.h`, root
`CMakeLists.txt`, and the host test CMake/mocks/runner. `FilterMode` values were
appended without changing `DeviceConfig` layout, so schema version remains 2.

## 4. Internal reference constraint

Current A33 hardware uses CS1237 internal REFOUT. The build freezes:

```c
#define CS1237_REFERENCE_OUTPUT_ENABLED 1U
#if (CS1237_REFERENCE_OUTPUT_ENABLED != 1U)
#error "Current A33 hardware requires CS1237 internal REFOUT."
#endif
```

The default A-channel, gain-128, 10 Hz, REFOUT-enabled configuration encodes as
`0x0C`; the host regression test confirms this and prevents `0x4C` regression.
Stage 2B configuration readback remains compiled and covered.

Future hardware TODO only: a planned external 3.3 V ratiometric reference will
require REFOUT to be disabled and the register value and analog path to be
revalidated. It has no effect on current code.

## 5. Weight representation and parameters

`WeightValue` is `int32_t` and represents one least displayed unit selected by
`unit` and `decimal_places`. With kg and three decimals, `1` means 0.001 kg,
`1000` means 1.000 kg, and `12345` means 12.345 kg.

`capacity`, `division`, span weight, zero/overload thresholds, stability
thresholds, tare, gross, net, and later alarm limits use the same integer unit.
`division` is the legal increment in those units; `capacity` is full-scale
capacity. Changing `unit` or `decimal_places` requires migrating every related
parameter. Stage 3 does not implement unit conversion or parameter migration.

Current default capacity, division, decimals, zero/overload/stability values
are development settings and are **NOT VERIFIED ON SCALE HARDWARE**.

## 6. Processing chain and public interfaces

```text
CS1237 24-bit sample
  -> MeasurementBridge (maximum four samples per App_Run)
     -> RawMeasurement statistics
     -> MetrologyManager_AcceptRawSample
        -> WeightEngine
           -> raw filter
           -> calibration with daily zero offset
           -> unrounded gross
           -> tare subtraction / unrounded net
           -> division quantization
           -> stability on unrounded net
           -> ZERO / TARE / OVERLOAD status
        -> WeightSnapshot
  -> MetrologyManager_Process20ms
     -> rate-limited weight and stability-change events
```

Every valid consumed sample runs the engine with its original timestamp;
invalid samples are rejected. The 20 ms task publishes state but never drives
filtering or conversion. `WeightSnapshot` is the only formal weight output for
future UI and protocol modules.

Primary APIs are `WeightEngine_Init/ProcessRawSample/GetSnapshot`,
`MetrologyManager_Init/AcceptRawSample/Process20ms/GetSnapshot`, the manager's
`Zero/ResetZero/Tare/ClearTare/ApplyCalibration/ReconfigureFilter` actions,
controlled `SystemContext_SetTareState/SetConfigDirty/SetWeightView` setters,
and `Stage3MetrologyDiagnostics_Update/GetSnapshot` for SWD Watch.

## 7. Filtering

| Mode | `filter_strength` | Ready rule |
|---|---|---|
| `NONE` | ignored | first sample |
| `AVERAGE` | window length 2..32 | full window |
| `IIR` | right shift 1..8 | first sample |
| `MEDIAN3_IIR` | IIR right shift 1..8 | three samples |

The moving average uses a checked ring and `int64_t` sum. IIR applies symmetric
nearest rounding to `(input-output)/2^strength`. Median3 removes one isolated
spike before IIR. Invalid values are rejected rather than corrected. The
production default remains `NONE`; filter selection is **NOT VERIFIED ON SCALE
HARDWARE**.

## 8. Calibration and quantization

The two-point model is:

```text
raw_delta       = raw_span - raw_zero
effective_zero  = raw_zero + zero_offset_raw
measurement     = filtered_raw - effective_zero
gross_unrounded = measurement * span_weight / raw_delta
```

All intermediates are checked integer operations. Division rounds to nearest,
with exact halves away from zero. A negative `raw_delta` supports reverse
sensor direction. The raw span magnitude must be greater than 1000 counts; this
software safety minimum is **NOT VERIFIED ON SCALE HARDWARE**.

Gross and net are quantized independently to `division`. For division 5:
12 -> 10, 13 -> 15, -12 -> -10, and -13 -> -15.

## 9. Zero, tare, gross, and net

Calibration zero (`CalibrationConfig.raw_zero`) belongs to the permanent
two-point model. Daily zero stores a volatile raw-count offset and never changes
that model. Daily zero requires valid calibration, a ready and stable sample,
no active tare, and `abs(gross_unrounded) <= zero_range`. Reset zero clears only
the volatile offset.

Tare requires valid calibration, stability, and no overload. It stores current
unrounded gross; repeated tare updates it. The relationships are:

```text
net_unrounded = gross_unrounded - tare_weight
net_weight    = quantize(net_unrounded, division)
```

Clear tare restores net to gross. Tare may be restored from runtime state only
when `tare_power_loss_retention` is enabled. Stage 3 performs no Flash write.
Zero, reset-zero, tare, clear-tare, calibration, and filter changes reset
stability history.

## 10. Stability, ZERO, and OVERLOAD

The stability detector consumes `net_unrounded`, not quantized weight. It waits
for its 2..32-sample window, enters CANDIDATE at
`spread <= enter_threshold`, becomes STABLE after the real-time hold interval,
and exits at `spread >= exit_threshold`. Between thresholds it remains stable.
Unsigned elapsed-time arithmetic supports timestamp wrap.

`ZERO` is set when `abs(net_unrounded) <= zero_range`. `OVERLOAD` is set for
either sign when `abs(gross_unrounded)` exceeds nonzero `overload_threshold`,
or capacity when that threshold is zero. Instability, overload, rejected user
actions, and an uncalibrated instrument are business states, not program faults.

Current stability thresholds and hold time are **NOT VERIFIED ON SCALE
HARDWARE**.

## 11. WeightSnapshot

The snapshot contains raw and filtered counts; unrounded and quantized gross;
tare; unrounded and quantized net; status flags; sample timestamp/sequence;
filter accepted count; and stability spread. Example for 7.000 kg gross with a
5.000 kg tare under kg/3-decimal configuration:

```text
gross_unrounded = 7000
gross_weight    = 7000
tare_weight     = 5000
net_unrounded   = 2000
net_weight      = 2000
status_flags    = RAW_VALID | FILTER_READY | CALIBRATION_VALID |
                  WEIGHT_VALID | TARE_ACTIVE
```

The exact flags also depend on current stability, zero, and overload state.

## 12. Events and diagnostics

`EVENT_NEW_WEIGHT_SAMPLE` is emitted at most once per 20 ms period and only for
a new valid weight. `arg0` is the net `int32_t` bit pattern, `arg1` is status,
and `source` is NULL. `EVENT_WEIGHT_STABLE_CHANGED` is emitted only when the
stable Boolean changes. A full queue does not advance publication state, so
both event types retry next period.

Stage 2B diagnostic behavior and the existing
`ENABLE_STAGE2B_BOARD_DIAGNOSTICS` preprocessing logic are unchanged. Stage 3
adds a read-only HAL-free SWD snapshot and does not alter the display.

Default uncalibrated operation continues CS1237 sampling, bounded FIFO
consumption, raw statistics, filtering, and raw diagnostics. Raw fields update,
but `WEIGHT_VALID` remains clear, no formal weight event is published, and zero
or tare returns `CALIBRATION_INVALID` without entering FAULT.

## 13. Validation results

Host clean build uses MSVC `/W4 /WX`. CTest result: 1/1 passed. Production
Domain sources are compiled directly. Coverage includes the requested math,
four filters, positive/reverse calibration, overflow rejection, stability and
time wrap, zero/tare guards, engine flags/resets/sequences, bounded 16-sample
bridge consumption in four calls, event limiting/change detection/retry,
internal REFOUT `0x0C`, and Stage 2A/2B regressions.

Synthetic tests cover constant zero with noise, steps, ramps, isolated spikes,
vibration, negative-direction sensors, variable timestamps, and timestamp wrap.
The tested scenario reaches stable load, tare-to-zero, added net load, and clear
tare. These are host simulations, not physical metrology results: **NOT
VERIFIED ON SCALE HARDWARE**.

ARM clean builds completed with zero errors and zero warnings:

| Build | FLASH | RAM | Stage 2B baseline | Delta |
|---|---:|---:|---:|---:|
| Debug | 30952 B | 3568 B | 21392 B / 2960 B | +9560 B / +608 B |
| Release | 17768 B | 3568 B | 12912 B / 2960 B | +4856 B / +608 B |

Static checks found no dynamic allocation, floating point, recursion,
FreeRTOS, `HAL_Delay`, or weight `sprintf`. Domain includes no HAL, CS1237,
GPIO, EventQueue, UART, display, or SystemContext dependencies. CubeMX `.ioc`,
`Core`, startup, linker, and HAL sources were not modified by Stage 3.

## 14. Deferred work and Stage 4 recommendation

Automatic zero tracking fields remain present and disabled. Automatic tracking
is **NOT IMPLEMENTED IN STAGE 3** because physical noise and drift rates are
unknown and incorrect tracking could absorb slowly applied small loads.

Physical scale verification remains outstanding for calibration span safety,
capacity/division/decimal selection, zero range, overload threshold, filter
mode/strength, stability window/thresholds/hold time, temperature drift, creep,
linearity, repeatability, and corner loading. All are **NOT VERIFIED ON SCALE
HARDWARE**.

Recommended Stage 4 work is a single shared command/application layer for keys,
display, Modbus, and BLE to consume `WeightSnapshot` and call
`MetrologyManager` actions, followed by a deliberate persistent configuration
design with validation and migration. Do not duplicate weight calculations in
UI or protocol code. Physical calibration and noise capture should precede
enabling non-default filters or automatic zero tracking.
