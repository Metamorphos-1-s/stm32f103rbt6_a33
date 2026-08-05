# CS1237 Noise Test Record

- Date: 2026-08-05 13:09:39 +08:00
- Firmware baseline: 2cf8f3739e9ec9ed6c05dfb76e5c196f32d57a85
- Firmware build: AdcDiagChannelA, uncommitted adc-noise-diagnostics branch
- Status: REAL SENSOR ZERO-LOAD TESTED
- PCB version: not recorded
- Sensor model: not recorded
- Capacity: not recorded
- Sensitivity: not recorded
- Bridge resistance: not recorded
- Excitation: 3.3 V reported from preceding common-mode measurement
- AVDD and DVDD: not recorded
- Ambient temperature: not recorded
- Load: 0 g nominal, unloaded
- Mechanical mounting, shielding, and airflow: not recorded
- Display: periodic refresh enabled
- RS485 polling: COM5, address 1, 115200 N1, 20 ms host poll interval
- Preheat: board reported preheated; sensor stabilization duration not recorded
- Capture: 60 s excluded interval plus 599.997 s valid interval
- Raw CSV: D_sensor_zero_20260805_130939.csv
- Statistics: 6094 samples, mean -43399.081 counts, std 48.896 counts,
  detrended std 24.742 counts, peak-to-peak 273 counts, MAD 28 counts,
  drift +14.590 counts/min
- Integrity: 20 sequence gaps, 0 FIFO overruns, 3 timestamp-period anomalies
- Equivalent mass: not calculated because active calibration was invalid
- Exceptions: three Modbus timeout events recovered within bounded retries
- Comparison: versus external common-mode short, detrended std +29.2%;
  approximate combined sensor/excitation/mechanical increment 15.66 counts std
- Conclusion: the real-sensor zero-load system adds measurable low-frequency
  variation and substantial drift; this run cannot isolate sensor, excitation,
  cable, mounting, airflow, vibration, or temperature
- Next action: repeat zero-load after longer mechanical stabilization, then
  capture one certified fixed-load point without moving the setup
