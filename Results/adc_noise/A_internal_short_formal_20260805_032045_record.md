# CS1237 Noise Test Record

- Date: 2026-08-05 03:20:45 +08:00
- Firmware baseline: 2cf8f3739e9ec9ed6c05dfb76e5c196f32d57a85
- Firmware build: AdcDiagInternalShort, uncommitted adc-noise-diagnostics branch
- Status: INTERNAL SHORT TESTED
- PCB version: not recorded
- Sensor model: sensor may remain connected; bypassed by internal-short channel
- Capacity: not applicable to ADC-count result
- Sensitivity: not recorded
- Bridge resistance: not recorded
- Excitation voltage: not recorded
- AVDD: not recorded
- External VCM: not applicable
- Environment temperature: not recorded
- Load: not applicable
- Wiring: normal board wiring; CS1237 internal-short channel
- Display: periodic refresh enabled
- RS485 polling: COM5, address 1, 115200 N1, 20 ms host poll interval
- Preheat: at least 20 minutes continuous power
- Capture: 60 s excluded warmup plus 599.984 s valid interval
- Raw CSV: A_internal_short_formal_20260805_032045.csv
- Statistics: 6107 samples, mean -1007.716 counts, std 16.498 counts,
  detrended std 16.497 counts, peak-to-peak 120 counts, MAD 11 counts,
  drift -0.0168 counts/min
- Integrity: 12 sequence gaps, 0 FIFO overruns, 1 timestamp-period anomaly
- Equivalent mass: not calculated because active calibration was invalid
- Exceptions: two Modbus timeout events recovered within bounded retries
- Conclusion: valid board-level internal-short baseline; insufficient alone to
  identify the dominant system noise source or support a CS1232 replacement
- Next action: external common-mode short after powered-off wiring change and
  measured common-mode verification
