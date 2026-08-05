# CS1237 Noise Test Record

- Date: 2026-08-05 14:25:41 +08:00
- Firmware baseline: 2cf8f3739e9ec9ed6c05dfb76e5c196f32d57a85
- Firmware build: AdcDiagChannelA, uncommitted adc-noise-diagnostics branch
- Status: 500 G UNLOAD RETURN TESTED
- Sensor and environment details: not recorded
- Excitation: 3.3 V reported from preceding common-mode measurement
- Load: nominal 0 g after removing nominal 500 g
- Display: periodic refresh enabled
- RS485 polling: COM5, address 1, 115200 N1, 20 ms host poll interval
- Capture: 60 s excluded recovery plus 599.978 s valid interval
- Raw CSV: F_sensor_zero_after_500g_20260805_142541.csv
- Statistics: 6114 samples, mean -43347.639 counts, std 19.455 counts,
  detrended std 17.422 counts, peak-to-peak 132 counts, MAD 13 counts,
  drift +2.999 counts/min
- Integrity: 0 sequence gaps, 0 FIFO overruns, 0 timestamp anomalies
- Apparent return offset: +51.44 counts, provisionally +0.058 g
- Provisional equivalent noise: 0.0197 g detrended RMS and 0.149 g p-p
- Calibration warning: cross-run response estimate only; not stored or traceable
- Conclusion: post-unload short-term noise matches the loaded run closely; the
  first zero run was likely insufficiently settled. The apparent return offset
  cannot yet be separated from time drift or classified as static hysteresis
- Next action: repeat a controlled 0-500-0 cycle with equal dwell times and
  record ambient temperature and weight accuracy class
