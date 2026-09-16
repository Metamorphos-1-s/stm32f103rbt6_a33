# Stage 5M-R4 Sensor Portability

The current physical sensor is a nameplate-confirmed 3 kg load cell. Its device
configuration is also 3 kg. The 6 kg value in the task text is a documented text
error.

Current-sensor independent validation failed before cross-sensor qualification.
No second physical sensor was installed or tested, and synthetic count scaling
is not reported as hardware portability evidence.

Raw ADC counts remain diagnostic inputs only. Any future candidate must convert
raw delta through the active two-point calibration before applying mass-domain,
division-domain, or noise-domain thresholds. Calibration, sensor, PGA, or sample
rate changes must reset all estimator state and runtime offset.

Status: **CROSS-SENSOR HARDWARE VALIDATION NOT RUN**.
