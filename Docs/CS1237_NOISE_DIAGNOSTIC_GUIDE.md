# CS1237 Layered Noise Diagnostic Guide

## Status and baseline

- Software baseline: `2cf8f3739e9ec9ed6c05dfb76e5c196f32d57a85`.
- Branch: `adc-noise-diagnostics`.
- Status: **SOFTWARE IMPLEMENTED**.
- Hardware status: **INTERNAL SHORT TESTED**, **EXTERNAL COMMON-MODE SHORT
  TESTED**, **REAL SENSOR ZERO-LOAD TESTED**, **REAL SENSOR 500 G TESTED**, and
  **500 G UNLOAD RETURN TESTED** on 2026-08-05. Dummy bridge, repeated load
  cycles, other fixed-load points, and display interference remain **NOT TESTED
  ON HARDWARE**.

This diagnostic branch is intended to locate the dominant source of an
approximately 0.06 g observed weight variation. It does not improve display
behavior or add a product feature. Conclusions must remain in ADC counts first;
equivalent mass is an estimate derived from calibration.

## Isolation layers

Run the experiments in this order:

1. Internal short: CS1237 PGA/modulator plus the board's reference, supply,
   clock, local layout, and digital readout environment.
2. External common-mode short: adds PCB analog input routing, input RC,
   leakage, common-mode source, external wiring, and ground return.
3. Precision dummy bridge: adds excitation/reference path, bridge thermal
   noise, ratiometric wiring, and bridge-current return.
4. Real sensor at zero and fixed loads: adds sensor, cable, mounting, pan,
   airflow, vibration, creep, stress, and temperature.
5. Display/RS485 matrix: varies digital serial activity while keeping the same
   analog source.

Internal short is not "pure ADC intrinsic noise" because the actual board
power, reference, clock, nearby layout, and digital environment remain active.

## Diagnostic images

The diagnostic override is compile-time only. It is applied to a private
runtime metrology copy and CS1237 driver configuration. It never changes
DeviceConfig, revision, dirty state, Schema V2, A/B Flash, calibration, zero, or
tare.

| Preset | Identity | Channel | Rate | Gain | Filter | REFOUT | Display refresh |
|---|---:|---|---:|---:|---|---|---|
| Debug/Release/BoardDiagnostics | 0 | product A | product profile | product profile | product profile | on | on |
| AdcDiagInternalShort | 1 | internal short | 10 Hz | 128 | NONE | on | on |
| AdcDiagChannelA | 2 | A | 10 Hz | 128 | NONE | on | on |
| AdcDiagChannelA_DisplayOff | 3 | A | 10 Hz | 128 | NONE | on | stopped |

Build with:

```powershell
cmake --preset AdcDiagInternalShort
cmake --build --preset AdcDiagInternalShort --clean-first
cmake --preset AdcDiagChannelA
cmake --build --preset AdcDiagChannelA --clean-first
cmake --preset AdcDiagChannelA_DisplayOff
cmake --build --preset AdcDiagChannelA_DisplayOff --clean-first
```

The driver writes the configuration, reads it back, compares the complete
register, enters ERROR on mismatch, and discards four settling conversions
before RUNNING. Reflashing product Release restores product behavior because no
diagnostic setting is persisted.

"Display refresh stopped" means periodic display model writes and TM1628 RAM
flushes stop. It does not remove TM1628 power or stop CS1237, timekeeping,
USART2 DMA, Modbus, or the TM1628 hardware's autonomous segment scan.

## Read-only Modbus data

Register map version remains `0x0100`. The diagnostic extension occupies
previously reserved read-only locations:

| PDU | Value |
|---|---|
| `001C-001D` | signed int32 raw CS1237 count |
| `001E-001F` | signed int32 filtered raw count |
| `0020-0021` | uint32 sample sequence |
| `0022-0023` | uint32 device timestamp ms |
| `002B` | CS1237 driver state; RUNNING is 4 |
| `002C` | FIFO sample count |
| `002D-002E` | uint32 FIFO overrun count |
| `002F` | verified CS1237 config register in diagnostic images; product returns 0 |
| `003C` | image identity 0/1/2/3 |

Multi-register values follow active `word_order`. The capture script reads the
order instead of assuming high-word-first. It validates CRC, slave address,
function, byte count, image identity, channel, 10 Hz, gain 128, and REFOUT. It
waits for RUNNING, deduplicates by sample sequence, supports uint32 sequence and
timestamp wrap, records gaps as lost samples, and never treats Modbus polls as
ADC samples.

## Capture workflow

Install PC dependencies:

```powershell
python -m pip install -r Tools\stage5b_hw\requirements.txt
```

Example 30-minute internal-short capture with a 60-second excluded warmup:

```powershell
python Tools\adc_noise_diag\capture_adc_noise.py `
  --port COM8 --baud 115200 --parity N --stop-bits 1 --slave 1 `
  --mode internal_short --duration 1800 --warmup 60 --poll-ms 20 `
  --test-id A_internal_short `
  --notes "20 min board preheat; display on; normal RS485 polling" `
  --output Results\adc_noise\A_internal_short.csv
```

Modes are `internal_short`, `channel_a`, and
`channel_a_display_off`. Optional metadata includes `--load-g`,
`--sensor-capacity-g`, `--sensor-sensitivity-mv-v`, `--excitation-v`, and
`--counts-per-g`. Without `--counts-per-g`, the script uses active calibration
only when calibration is valid, raw span differs from raw zero, and span mass
is positive. Reverse sensors use the absolute slope for noise conversion.

The CSV contains host ISO/monotonic time, elapsed time, test/mode/load,
sequence, device timestamp, raw and filtered counts, driver/FIFO state,
overruns, cumulative lost samples, verified config, word order, warmup
`excluded`, conversion metadata, and notes. It flushes at least once per second,
writes a temporary file, and atomically replaces the requested CSV on normal
exit, bounded communication failure, or Ctrl+C. Errors are logged beside the
CSV. Filter NONE should make raw and filtered counts equal under the current
filter definition.

## Single-run analysis

```powershell
python Tools\adc_noise_diag\analyze_adc_noise.py `
  Results\adc_noise\A_internal_short.csv
```

By default results go to `A_internal_short_analysis/`. The tool creates JSON,
Markdown, and separate PNG files:

- `raw_vs_time.png`
- `detrended_raw_vs_time.png`
- `histogram.png`
- `rolling_peak_to_peak.png`
- `averaging_noise.png`
- `allan_deviation.png`
- `psd_0_5hz.png`

Statistics include sample count/duration/rate, mean, median, min/max,
peak-to-peak, sample standard deviation, MAD, 1/5/25/75/95/99 percentiles,
zero-mean RMS, linear drift in counts/min, detrended standard deviation,
maximum adjacent change, lost samples, FIFO overruns, timestamp anomalies,
non-overlapping 2/4/8/16/32-sample averaging, Allan deviation, and PSD.

At 10 Hz output, Nyquist is 5 Hz. A 0-5 Hz output spectrum cannot directly
separate 50 Hz from 60 Hz mains interference. A future 640 Hz short experiment
would have different noise bandwidth and must not be compared directly by
standard deviation or peak-to-peak. Complete 640 Hz export would require a
batch buffer; ordinary Modbus polling must not be presented as a continuous
640 Hz record.

## Multi-run comparison

```powershell
python Tools\adc_noise_diag\compare_noise_runs.py `
  Results\adc_noise\A_internal_short_analysis\A_internal_short.json `
  Results\adc_noise\B_external_common_mode_analysis\B_external_common_mode.json `
  Results\adc_noise\C_dummy_bridge_analysis\C_dummy_bridge.json `
  Results\adc_noise\D_sensor_zero_analysis\D_sensor_zero.json `
  --output-dir Results\adc_noise\comparison
```

This creates `ADC_NOISE_COMPARISON.md/.json` and separate standard deviation,
peak-to-peak, and drift PNGs. It reports approximate layers:

```text
var_adc = var_internal_short
var_pcb_increment = max(0, var_external_short - var_internal_short)
var_bridge_supply_increment = max(0, var_dummy_bridge - var_external_short)
var_sensor_mechanical_increment = max(0, var_real_sensor - var_dummy_bridge)
```

This subtraction assumes approximately independent sources. Correlated noise,
drift, thermal effects, and mains coupling cannot be separated this way. A
single short run cannot prove a device has reached a physical limit.

## Electrical safety and measurements

Power the board off before every wiring change. Never:

- connect AINP or AINN directly to E+ or E-;
- apply an unmeasured external common-mode voltage;
- hot-plug sensor signal wires;
- exceed the CS1237 input/common-mode limits at gain 128;
- change REFOUT without understanding the board reference/excitation path;
- clip USB, oscilloscope, or supply ground to a bridge signal terminal;
- use a long solderless breadboard setup for final low-noise conclusions.

Before each external experiment record AVDD, DVDD, E+, E-, AINP, AINN,
external VCM, E+-E-, and VCM relative to AGND. Confirm the measured VCM is
inside the CS1237 data-sheet common-mode range at gain 128.

### A: internal short

Use `AdcDiagInternalShort`. The sensor may remain connected because the
internal channel bypasses external AIN. Keep normal excitation and REFOUT.
Preheat at least 20 minutes, wait for RUNNING, exclude at least 60 seconds, and
capture at least 10 minutes, preferably 30 minutes.

### B: external common-mode short

Use `AdcDiagChannelA`. Power off and disconnect sensor S+/S- from AIN. Do not
connect AIN to E+. Create a verified low-noise common mode, for example equal
10 kohm resistors between E+ and E- with 100 nF plus 1 uF from VCM to E-, then
connect VCM to AINP and AINN through equal 100 ohm to 1 kohm series resistors.
Use matched parts and symmetric short wiring. Measure VCM before capture. If E+
is not a valid source for the board's ratiometric reference arrangement, use a
separately verified low-noise common-mode source instead.

### C: precision dummy bridge

Use four same-lot low-temperature-coefficient precision resistors near the real
bridge resistance, typically 350 ohm, preferably 0.01% or better. Excite it
from the same E+/E- path and connect its differential midpoint pair to AINP and
AINN. Record actual resistor values, excitation, and temperature. Do not hide a
saturated or highly offset bridge with software zero.

### D-G: real sensor

Capture zero and fixed 1000/3000/5000 g points with `AdcDiagChannelA`. Preheat
20-30 minutes, allow initial creep to settle, use a draft shield, isolate table
vibration, secure the cable, avoid touching the pan, and record temperature and
weight class. Keep Filter NONE, display lock off, and standard-weight rounding
off. Each point needs at least 10 minutes, preferably 30 minutes.

### H: digital coupling matrix

Use one unchanged external common-mode short or dummy bridge for all cases:

- H1 display refresh on, normal RS485 polling;
- H2 display refresh stopped, normal RS485 polling;
- H3 display refresh stopped, low-rate RS485 polling;
- H4 display refresh on, high-rate RS485 polling.

Stopping refresh is not removing TM1628 power. High polling must not overflow
the FIFO. Compare raw count standard deviation, peak-to-peak, low-frequency
spectrum, lost samples, and overruns. Improvement with refresh stopped points
toward supply/ground/digital-edge coupling; sensitivity to RS485 rate points
toward transceiver supply, DE edges, or communication ground return.

## Interpretation and CS1232 decision

- Internal short similar to real sensor: prioritize CS1237/PGA, reference,
  supply, local PCB, or nearby digital environment.
- Quiet internal short but noisy external short: prioritize PCB input path,
  input RC, leakage, common mode, wiring, and ground return.
- Quiet external short but noisy dummy bridge: prioritize excitation,
  reference, bridge current return, and ratiometric connection.
- Quiet dummy bridge but noisy sensor: prioritize sensor, cable, mounting,
  airflow, vibration, creep, and temperature before changing ADC.
- Similar short-term deviation but different long drift: treat random noise and
  thermal/mechanical drift as separate problems.

There is evidence to evaluate CS1232 only after repeatable internal-short and
dummy-bridge results show the ADC/reference/local-board layer dominates under
controlled supply, temperature, and digital conditions. Without that data,
"CS1232 is definitely better" and "the sensor has reached its limit" are not
supported conclusions.

Use [CS1237_NOISE_TEST_RECORD.md](CS1237_NOISE_TEST_RECORD.md) for every run.

## Software verification record

Verified on 2026-08-05 against baseline commit
`2cf8f3739e9ec9ed6c05dfb76e5c196f32d57a85`:

| Preset | Flash used | Static RAM used | Result |
| --- | ---: | ---: | --- |
| Debug | 102028 B | 10672 B | PASS |
| Release | 55392 B | 10688 B | PASS |
| BoardDiagnostics | 99792 B | 10664 B | PASS |
| AdcDiagInternalShort | 55440 B | 10688 B | PASS |
| AdcDiagChannelA | 55440 B | 10688 B | PASS |
| AdcDiagChannelA_DisplayOff | 55440 B | 10688 B | PASS |

All six clean ARM builds passed. The clean host build and all 10 CTest cases
passed, including four compile-time mode-policy variants and eight Python unit
tests executed by the Python CTest case. The formal Release increase versus the
baseline is 184 B Flash and 0 B RAM. No `.ioc`, CubeMX-generated `Core`, HAL,
clock, DMA, interrupt-priority, or persistent-schema change is part of this
branch.

Hardware capture, external-short wiring, dummy-bridge testing, real-load
testing, and the display/RS485 coupling matrix are **NOT TESTED** in this
software verification record. Do not treat the software test results as noise
performance measurements.

## Hardware result: internal short

After at least 20 minutes of continuous preheat, the
`AdcDiagInternalShort` image was verified over Modbus as mode 1 with CS1237
configuration register `0x0F` (internal short, 10 Hz, gain 128, REFOUT on).
A 60-second excluded interval followed by 599.984 seconds of valid capture
produced 6107 unique samples. Mean was -1007.716 counts, sample standard
deviation 16.498 counts, detrended standard deviation 16.497 counts,
peak-to-peak 120 counts, MAD 11 counts, and linear drift -0.0168 counts/min.
The host observed 12 sequence gaps, zero device FIFO overruns, and one analysis
timestamp-period anomaly. Active calibration was invalid, so no equivalent
mass result was calculated.

This is a measured board-level internal-short result, not pure intrinsic ADC
noise. It includes the actual reference, supply, clock, PCB environment, and
digital activity. Without the external-short, dummy-bridge, and sensor layers,
it does not identify the dominant source of the observed weight variation and
does not justify changing to CS1232.

## Hardware result: external common-mode short

With power removed for wiring, equal 10 kohm resistors were connected from E+
to VCM and VCM to E-. The measured values before capture were E+-E- 3.3 V,
VCM-E- 1.655 V, AINP-E- 1.655 V, AINN-E- 1.655 V, and AINP-AINN 0.01 mV.
The `AdcDiagChannelA` image was verified as mode 2 with register `0x0C`
(channel A, 10 Hz, gain 128, REFOUT on).

A 60-second excluded interval followed by 599.968 seconds of valid capture
produced 6095 unique samples. Mean was -1064.021 counts, sample standard
deviation 21.667 counts, detrended standard deviation 19.153 counts,
peak-to-peak 151 counts, MAD 15 counts, and linear drift +3.512 counts/min.
The host observed 18 sequence gaps, zero device FIFO overruns, and three
analysis timestamp-period anomalies. Calibration was invalid, so no equivalent
mass result was calculated.

Compared with the formal internal short, raw standard deviation increased
31.3%, detrended standard deviation increased 16.1%, and peak-to-peak increased
25.8%. Independent-source variance subtraction gives an approximate external
input/PCB/common-mode incremental variance of 94.68 count squared, equivalent
to 9.73 counts standard deviation. This estimate excludes neither correlated
coupling nor thermal drift. The much larger +3.512 counts/min drift is a
separate result and should be checked with longer stabilization and repeated
runs. The data supports investigating the external input path and common-mode
network, but does not yet isolate PCB routing from the divider, wiring, supply,
or thermal effects.

## Hardware result: real sensor zero load

The real sensor was reconnected with no applied load. Sensor model, capacity,
sensitivity, bridge resistance, mounting details, and ambient temperature were
not recorded. `AdcDiagChannelA` remained at channel A, 10 Hz, gain 128, Filter
NONE, and REFOUT on. The approximately -43,400-count offset was well inside
the signed 24-bit range.

A 60-second excluded interval followed by 599.997 seconds of valid capture
produced 6094 unique samples. Mean was -43399.081 counts, sample standard
deviation 48.896 counts, detrended standard deviation 24.742 counts,
peak-to-peak 273 counts, MAD 28 counts, and linear drift +14.590 counts/min.
The host observed 20 sequence gaps, zero device FIFO overruns, and three
analysis timestamp-period anomalies. Calibration was invalid, so no equivalent
mass result was calculated.

Versus the external common-mode short, detrended standard deviation increased
29.2%, while raw standard deviation increased 125.7% and peak-to-peak increased
80.8%. Under the limited independent-source approximation, the sensor,
excitation, cable, mounting, and mechanical environment together add about
245.32 count squared, or 15.66 counts equivalent standard deviation, beyond
the external-short result. The very large +14.59 counts/min slope shows that
slow drift dominates much of the uncorrected spread. This single zero-load run
does not separate sensor creep, mounting stress, temperature, airflow,
vibration, cable force, or excitation effects, and does not establish sensor
accuracy or a physical precision limit.

## Hardware result: real sensor at 500 g

A nominal 500 g fixed weight was placed without changing the diagnostic image.
The weight class and sensor specifications were not recorded. After a
60-second excluded creep interval, 600.012 seconds of valid capture produced
6109 unique samples. Mean was -485934.001 counts, sample standard deviation
18.186 counts, detrended standard deviation 17.219 counts, peak-to-peak 147
counts, MAD 12 counts, and linear drift -2.027 counts/min. The host observed
five sequence gaps, zero device FIFO overruns, and one analysis timestamp-period
anomaly.

The difference between the separately acquired zero-load and 500 g means gives
an approximate response of 885.07 counts/g. This is not a formal calibration:
the zero and loaded points were acquired at different times, the zero run had
large drift, and weight traceability was not recorded. Using this provisional
slope only for interpretation, the loaded detrended standard deviation is
about 0.0195 g RMS, peak-to-peak is about 0.166 g, and drift is about -0.00229
g/min.

The loaded detrended standard deviation was 30.4% lower than the earlier
zero-load result, and raw standard deviation was 62.8% lower. The data therefore
does not support load-proportional noise. The earlier zero-load run was likely
more affected by time-dependent mechanical or thermal settling. An unload and
repeat-zero capture is required before drawing conclusions about hysteresis,
return-to-zero, or repeatability.

## Hardware result: zero after unloading 500 g

The nominal 500 g weight was removed without otherwise changing the setup. A
60-second excluded recovery interval followed by 599.978 seconds of valid
capture produced 6114 unique samples with no sequence gaps, FIFO overruns, or
timestamp anomalies. Mean was -43347.639 counts, sample standard deviation
19.455 counts, detrended standard deviation 17.422 counts, peak-to-peak 132
counts, MAD 13 counts, and linear drift +2.999 counts/min.

The post-unload mean was 51.44 counts above the earlier zero-load mean. Using
the provisional cross-run 885.07 counts/g response, this is an apparent +0.058
g return offset, or 0.0116% of the tested 500 g load. It must not be reported as
static hysteresis: the two zero runs were separated in time and had different
drift histories. The post-unload detrended standard deviation was 29.6% lower
than the first zero run and only 1.2% above the loaded result, supporting the
interpretation that the first zero run was dominated by incomplete settling.
Repeated 0-500-0 cycles with identical dwell times and controlled temperature
are required for repeatability and hysteresis estimates.
