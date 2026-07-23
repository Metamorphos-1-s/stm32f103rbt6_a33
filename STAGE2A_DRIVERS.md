# Stage 2A Local Drivers And Board Validation Handoff

## Status

Stage 2A source implementation, host logic tests, and ARM Debug/Release builds
are complete. The hardware owner subsequently confirmed two wiring facts:
PB12 is active-high enable, and GRID1-GRID6 are panel digits 1-6 from left to
right. No target board, oscilloscope, logic analyzer, or meter was available in
this session. Runtime electrical measurements remain:

**NOT TESTED ON HARDWARE**

Compilation and host tests are not presented as board validation.

## Added And Modified Files

Modified:

- `App/app_main.c`
- `BSP/bsp_adc.c`, `bsp_board.c`, `bsp_gpio.c/.h`, `bsp_time.c/.h`
- `Config/default_config.c`, `device_config.h`, `project_config.h`
- `Services/event_queue/event_queue.h`, `fault_manager/fault_manager.h`
- `Drivers/w02/README.md`, `CMakeLists.txt`

Added:

- `.gitignore`, `App/device_manager.c/.h`
- `Drivers/cs1237/cs1237.c/.h`, `cs1237_types.h`
- `Drivers/tm1628/tm1628.c/.h`, `tm1628_types.h`,
  `tm1628_board_map.c/.h`
- `Drivers/battery_adc/battery_adc.c/.h`
- `Drivers/output_gpio/output_gpio.c/.h`
- `Drivers/w02/w02_pwrkey.c/.h`
- `Diagnostics/stage2a_driver_diagnostics.c/.h`
- `Tests/host/CMakeLists.txt`, `README.md`, `mock_hal.h`, `mocks.c`,
  `test_stage2a.c`
- `STAGE2A_DRIVERS.md`

The four replaced Stage 1 driver placeholder READMEs were removed. The repository
already tracks `build/`. Debug and Release were built and measured, then their
generated working-tree changes were restored to the current `main` baseline so
this source change has no binary/cache churn. `.gitignore` does not untrack
existing history; a later maintenance change should do that separately.

## Directory Tree

```text
stm32f103rbt6_a33/
|-- App/
|   |-- app_main.c/.h
|   |-- app_state.h
|   |-- device_manager.c/.h
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
|-- Diagnostics/
|   `-- stage2a_driver_diagnostics.c/.h
|-- Drivers/
|   |-- battery_adc/battery_adc.c/.h
|   |-- cs1237/cs1237.c/.h, cs1237_types.h
|   |-- output_gpio/output_gpio.c/.h
|   |-- tm1628/tm1628.c/.h, tm1628_types.h,
|   |   tm1628_board_map.c/.h
|   |-- w02/README.md, w02_pwrkey.c/.h
|   |-- uart_port/README.md
|   |-- CMSIS/
|   `-- STM32F1xx_HAL_Driver/
|-- Services/
|   |-- config_store/
|   |-- event_queue/
|   |-- fault_manager/
|   |-- scheduler/
|   `-- watchdog/
|-- Tests/host/
|   |-- CMakeLists.txt
|   |-- README.md
|   |-- mock_hal.h, mocks.c
|   `-- test_stage2a.c
|-- Core/                         CubeMX-owned, unchanged in Stage 2A
|-- Domain/                       placeholders only
|-- Protocol/                     placeholders only
|-- UI/                           placeholders only
|-- cmake/
|-- CMakeLists.txt
|-- STAGE1_ARCHITECTURE.md
|-- STAGE2A_DRIVERS.md
`-- stm32f103rbt6_a33.ioc        unchanged in Stage 2A
```

## Driver Responsibilities

- CS1237: raw 24-bit acquisition, configuration transaction/state machine,
  settling discard, and a fixed sample FIFO. It performs no weight conversion.
- TM1628: 7-grid/10-segment transport, 14-byte shadow RAM, raw five-byte key
  scan, and board wiring conversion. It performs no formatting or key semantics.
- Battery ADC: eight one-shot conversions every 500 ms, integer average,
  divider/calibration conversion, and raw voltage event. It controls no alarm.
- Output GPIO: basic on/off shadow state for lamps and buzzers only.
- W02 PWRKEY: one non-blocking active-low pulse with validation and hard release.
- DeviceManager: initializes and schedules drivers; it contains no weighing rules.

Drivers include no `main.h`, HAL GPIO API, physical ports, or pin constants.

## Microsecond Timing

`BSP_TimeInitMicrosecondCounter` enables Cortex-M3 DWT CYCCNT after the 72 MHz
clock is configured. `BSP_DelayUs` derives cycles from `SystemCoreClock`, returns
immediately for zero, and clamps requests to 1000 us before overflow checking.
CS1237 and TM1628 use it only for bounded serial edges.

Cycle timeout uses `(uint32_t)(now - start) >= required`; the same unsigned
subtraction remains correct when CYCCNT wraps. Host vectors cover a start at
`0xFFFFFFF0` and end after wrap. DWT operation on this board remains unverified.

## CS1237 Timing And Configuration

The implementation follows CHIPSEA CS1237 Rev 1.1:

- DRDY low gates a transaction; each `CS1237_Process` handles at most one frame.
- Data is read MSB first over 24 clocks and sign-extended from two's complement.
- Clocks 25/26 capture update status and clock 27 forces DOUT high.
- Configuration uses two transition clocks, seven command bits (`0x65` write or
  `0x56` read), direction clock 37, eight register bits, and final clock 46.
- SCLK high and low phases are each at least 1 us. PRIMASK is saved/restored for
  each high phase only; the whole transaction never masks interrupts at once.
- PB11 changes direction only in BSP. Output transition first matches released
  high, then enables high-speed push-pull; input is no-pull.

Config bits are B6 `REFO_OFF`, B5:B4 rate, B3:B2 gain, B1:B0 channel; B7 stays
zero. A + PGA128 + 10 Hz + REFOUT enabled encodes to reset value `0x0C`.
Rate and gain enums cover 10/40/640/1280 Hz and 1/2/64/128. Write configuration
is followed on the next ready conversion by readback verification. Four samples
are discarded after each configuration for every supported rate.

The FIFO contains 16 `CS1237_Sample` values. Full means reject-new and increment
overrun; old samples are never overwritten. One availability event is sent only
for an empty-to-nonempty transition, not for every 1280 Hz sample.

### AD_EN Result

The hardware owner confirmed PB12 is high to enable and low to disable.
`CS1237_EXTERNAL_ENABLE_PRESENT=1`, `CS1237_ENABLE_ACTIVE_LEVEL=1`, and
`CS1237_ENABLE_POLARITY_CONFIRMED=1` now enable the CS1237 initialization state
machine. CubeMX already initializes PB12 low, which is the confirmed safe-off
level. This wiring statement is recorded as owner-provided hardware information;
the signal has not been independently measured in this session.

## TM1628 RAM, Segments, And Keys

Initialization releases all open-drain lines, sends mode `0x03`, data command
`0x40`, writes fourteen zero bytes from `0xC0`, then sends `0x88 | brightness`.
Bytes C0/C1 through CC/CD map to GRID1 through GRID7. Even bytes hold SEG1-8;
odd byte bits 0-1 hold SEG9-10. GRID7 bytes CC/CD are forced to zero.

Transport is LSB first. DIO is prepared before each rising edge. Key reading
sends `0x42`, releases DIO, waits at least 2 us, and reads exactly five bytes.
STB is raised before CLK is released at transaction end, avoiding an extra edge.

Board segment mapping:

```text
UP->SEG1/b0  G->SEG2/b1  F->SEG3/b2  E->SEG4/b3  D->SEG5/b4
DOWN->SEG6/b5  A->SEG7/b6  C->SEG8/b7  DP->SEG9/odd-b0
B->SEG10/odd-b1
```

Raw keys map BYTE1.b0/b3, BYTE2.b0/b3, BYTE3.b0 to board mask bits 0-4.
The hardware owner confirmed the left-to-right panel order is GRID1, GRID2,
GRID3, GRID4, GRID5, GRID6, represented by `{0,1,2,3,4,5}` internally.
TM1628 provides no ACK, so an electrical disconnect cannot be proven from a
single write transaction; API/state failures are counted and thresholded.

## Battery Sampling

Every 500 ms the driver performs eight bounded ADC conversions and integer
averaging. Default VDDA is centrally defined as 3300 mV. Conversion uses the
runtime 30k/10k divider, then applies signed gain ppm and battery-side offset:

```text
battery_mv = adc_mv * (top + bottom) / bottom
corrected = battery_mv + battery_mv * gain_ppm / 1000000 + offset_mv
```

All multiplication uses 64-bit intermediates. The tested vector is
3150 mV -> 12600 mV. `near_full_scale` is informational only; no low-voltage
threshold, lamp, or buzzer action is implemented.

## W02 PWRKEY Safety

Initialization releases PA8 and never requests a pulse. Requests below 50 ms or
above 200 ms are rejected, as are concurrent requests. Normal completion uses
wrap-safe millisecond subtraction. Both BSP and driver enforce 250 ms release;
the driver enters ERROR if processing resumes at or after that hard limit.
Host tests cover 49/50/200/201 ms validation, normal completion across tick wrap,
and delayed 250 ms safety release. Oscilloscope timing is not yet verified.

## App Scheduling And Events

`App_Run` calls `DeviceManager_ProcessFast` before the cooperative scheduler;
CS1237 returns immediately if DRDY is high and reads at most one frame otherwise.
The 1/10/20/500 ms tasks service W02, keys, dirty display, and battery respectively.
DEVICE_INIT initializes once. WARMUP now waits for CS1237 configuration,
readback, and settling because its enable polarity is confirmed. A 3-second
timeout raises `FAULT_CS1237_NOT_READY`; no sample readiness is fabricated.
FAULT safe entry executes once.

Event arguments:

- `EVENT_CS1237_SAMPLE_AVAILABLE`: arg0 buffered count.
- `EVENT_TM1628_KEY_RAW_CHANGED`: arg0 five-bit key mask, arg1 matrix mask.
- `EVENT_BATTERY_SAMPLE_UPDATED`: arg0 battery mV, arg1 raw average.
- `EVENT_W02_PWRKEY_PULSE_DONE`: arg0 requested low milliseconds.
- `EVENT_DRIVER_READY/ERROR`: arg0 driver/error mask.

All events use `source=NULL`; no stack or mutable driver buffer pointer crosses a
layer. `EVENT_NEW_WEIGHT_SAMPLE` is retained but not used for raw ADC codes.

## Diagnostics

`ENABLE_STAGE2A_DIAGNOSTICS` defaults to 0 and no production path calls the
diagnostic module. When manually enabled and invoked from a debugger, Init clears
the display and Process snapshots one CS sample, battery state, and raw key mask.
It never automatically sounds a buzzer, pulses W02, or changes CS configuration.

## Host Test Result

MSVC 19.43, C11, `/W4 /WX`: **PASS**, 1 CTest target, 0 failures.
Covered vectors: four sign-extension boundaries; config encode/decode and `0x0C`;
RAM/grid/GRID7/segment/key maps; 3150->12600 mV; W02 range/wrap/hard limit;
millisecond and cycle wrap; FIFO reject-new/order and one-shot notification.

## ARM Build Result

GNU Arm 13.3.1 with `-Wall`, zero errors and zero warnings:

| Build | FLASH | Stage 1 delta | RAM | Stage 1 delta |
|---|---:|---:|---:|---:|
| Debug | 20,140 B | +8,776 B | 2,848 B | +360 B |
| Release | 12,208 B | +5,544 B | 2,840 B | +352 B |

## Board Validation Checklist

The PB12 polarity and GRID order are owner-confirmed as stated above. All runtime
items below are **NOT TESTED ON HARDWARE**:

1. CS1237: measure PB12 low/high behavior; confirm SCLK idle low and every high
   below 100 us; observe DRDY low, changing raw codes, config readback, four rates, PGA128,
   settling discard, and FIFO overrun behavior.
2. TM1628: prove blank power-up, independent six digits, A-G/DP/UP/DOWN mapping,
   confirmed grid order in operation, brightness 0-7, five raw keys, DIO release,
   and STB idle.
3. Battery: record meter battery voltage, PC0 voltage, software voltage, and error
   at low/mid/near-12.6 V points.
4. W02: scope released-high idle, an 80 ms request, <=200 ms normal limit, and
   delayed-processing release at 250 ms; confirm boot produces no pulse.
5. DWT: confirm initialization and measured 1/2 us driver timing at 72 MHz.

## Unimplemented And Stage 2B

Still excluded: weight conversion, calibration math, filters, stability, zero,
tare, limit logic, buzzer patterns, formatting/menu/key semantics, protocols,
UART buffering, W02 AT flow, Flash, watchdog, USB, DMA, and RTOS.

Stage 2B should begin only after the board checklist verifies segment/key wiring,
CS1237 operation, battery accuracy, W02 pulses, and measured timing. Then add a raw-sample
consumer boundary and HAL-free domain tests before implementing calibration and
measurement logic. Do not add weight semantics to these Stage 2A drivers.
