# CS1237 Noise Test Record

- Date: 2026-08-05 12:45:59 +08:00
- Firmware baseline: 2cf8f3739e9ec9ed6c05dfb76e5c196f32d57a85
- Firmware build: AdcDiagChannelA, uncommitted adc-noise-diagnostics branch
- Status: EXTERNAL COMMON-MODE SHORT TESTED
- PCB version: not recorded
- Sensor: disconnected from AIN for external common-mode test
- Common-mode network: E+--10 kohm--VCM--10 kohm--E-
- E+-E-: 3.3 V
- VCM-E-: 1.655 V
- AINP-E-: 1.655 V
- AINN-E-: 1.655 V
- AINP-AINN: 0.01 mV
- VCM capacitors: not recorded
- Equal AIN series resistors: not recorded
- AVDD and DVDD: not recorded
- Environment temperature: not recorded
- Display: periodic refresh enabled
- RS485 polling: COM5, address 1, 115200 N1, 20 ms host poll interval
- Capture: 60 s excluded interval plus 599.968 s valid interval
- Raw CSV: B_external_common_mode_20260805_124559.csv
- Statistics: 6095 samples, mean -1064.021 counts, std 21.667 counts,
  detrended std 19.153 counts, peak-to-peak 151 counts, MAD 15 counts,
  drift +3.512 counts/min
- Integrity: 18 sequence gaps, 0 FIFO overruns, 3 timestamp-period anomalies
- Equivalent mass: not calculated because active calibration was invalid
- Exceptions: three Modbus timeout events recovered within bounded retries
- Comparison: versus internal short, detrended std +16.1%; approximate
  independent external-path increment 9.73 counts std
- Conclusion: external input/common-mode layer adds measurable random noise and
  substantially more drift, but this run cannot separate PCB, divider, wiring,
  supply, coupling, and thermal contributors
- Next action: precision dummy bridge using the same excitation/reference path
