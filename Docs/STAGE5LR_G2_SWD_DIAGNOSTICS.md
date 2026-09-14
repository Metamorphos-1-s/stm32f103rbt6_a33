# Stage 5L-R G2 SWD Diagnostics

## Scope and evidence boundary

This diagnostic exists only in the `Stage5LDiagnostics` build under `STAGE5L_SWD_DIAGNOSTICS=1`. Debug, Release and BoardDiagnostics compile it out. SWD can establish where counts or data first diverge inside the MCU, but it cannot prove the electrical DRDY period, SCLK waveform, DOUT setup/hold time, ADC reference integrity or physical conversion rate.

No diagnostic command writes Active configuration, revision, saved revision, dirty state, Flash or V3 slots. Reset always loads the persisted Profile 0 / 10 Hz configuration. A temporary 40 Hz request is accepted only through the RAM control block and is automatically restored to the persisted 10 Hz profile when the bounded trace freezes.

## Source facts

PB11/DOUT is a polled GPIO input. It is not configured as EXTI. `CS1237_Process()` runs from `DeviceManager_ProcessFast()` on each `App_Run()` pass and treats DOUT low as the software ready observation. The synchronous reader then clocks 24 data bits, two status bits and one final clock. Configuration uses a discarded 27-clock data frame followed by the command/data transaction; write and readback are separate ready-gated state-machine passes.

The real data path is:

```text
PB11 low observed by CS1237_Process
-> 27-clock data read starts/completes
-> settling frame discarded, or driver sample accepted
-> CS1237 FIFO push
-> CS1237 FIFO pop
-> MeasurementBridge / RawMeasurement
-> MetrologyManager / WeightEngine accepts
-> WeightEngine sample_sequence increments
-> raw and weight events are published
```

`CS1237_GetBufferOverrunCount()` only detects a full software FIFO. A ready pulse or low interval missed before `CS1237_Process()` observes it does not increment overrun. There is no independent driver timeout state in the current architecture, so `driver_timeout_count` is retained as an explicit zero-valued semantic field; read failures are counted separately.

The 24-bit sign extension masks to 24 bits and extends bit 23. A near-rail value can therefore arise from the actual sampled bit pattern, a frame-phase error, or an electrical excursion, but not from the sign-extension expression alone. The driver does not accept incomplete frames: a bit-read failure increments read failure/error and does not push. Configuration transactions discard their leading data frame. Settling frames are read but not pushed. These facts do not prove that DOUT was sampled at a valid electrical phase.

Normal profile switching pauses bridge consumption, drains the FIFO, starts the driver configuration state machine, waits through verify and settling, and applies a canonical config through `ConfigApplication`. The diagnostic path also pauses the bridge and waits for driver RUNNING, then rebuilds metrology non-persistently; unlike the normal path it never changes Active profile or revision. Its direct ADC override is therefore diagnostic-only.

## SWD ABI

The control block is `g_stage5l_measurement_control`; the snapshot is `g_stage5l_rate_diagnostics`. Addresses are resolved from the exact ELF on every run, never hard-coded. Schema 3 control requires magic `0x354C4447`, length 88, and one-shot command authorization `0x47574453`. Repeated request sequences are idempotent; bad authorization is rejected.

The snapshot contains a 26-counter layer model and a 16-entry static ring. Each little-endian trace record is schema 1 and exactly 28 bytes: ready/read cycle timestamps, signed raw, processed sequence, FIFO depth, event flags, rate, driver state, config byte/status and read clock count. A diagnostic-only byte in the existing padded `CS1237_Sample` carries the exact trace index across FIFO push/pop without increasing structure size.

DWT CYCCNT is already initialized by the board time service, independent of debugger behavior. CPU clock is recorded as 72 MHz. Unsigned subtraction handles its approximately 59.65-second wrap for ready and read intervals.

The ring consumes 448 bytes. Together with the counters it replaces the older Stage 5L evidence ring; the complete snapshot is 660 bytes and the control block is 88 bytes. The Stage5LDiagnostics image uses 20,168 of 20,480 SRAM bytes. Because a larger pre/post ring would exceed SRAM, G2 uses rolling last-16 context and freezes on the first anomaly or sample target. Cumulative counters cover the full 256/400-sample run; retained interval percentiles cover only the last 16 entries and are labeled accordingly.

Freeze triggers are target completion, near rail, raw jump of at least 1,000,000 counts, read failure, config mismatch, ready interval outside 0.5x to 2x the requested nominal interval, FIFO push failure, or persistent push/accept divergence. Freeze prevents further overwrite. A temporary rate override schedules automatic 10 Hz restoration.

## Host tool

`Tools/stage5lr_g2/swd_diagnostics.py` resolves symbol address and size using `arm-none-eabi-nm`, checks ELF/map hashes, issues one-shot RAM commands through STM32CubeProgrammer, polls only the 88-byte control block at a default two-second interval, and uploads the 660-byte snapshot after autonomous freeze. It writes the raw control/snapshot/trace bytes, decoded JSON/CSV, counters, rate analysis and Repository Manifest V2.

The parser fails closed on truncated data, bad magic/version/length, wrong record size or invalid ring bounds. JSON is UTF-8 with final LF; CSV uses explicit LF; binary evidence is `-text`.

## Classification rules

Layer rates and deltas distinguish observed-ready, read, FIFO and WeightEngine losses. If MCU-ready itself remains near 16.5 Hz while config readback is 40 Hz, SWD cannot distinguish ADC output behavior from MCU missing the external level and an oscilloscope or logic analyzer remains mandatory. Even if all internal layers reach 40 Hz, G2 proves only the software path and cannot requalify the physical 40 Hz product function.
