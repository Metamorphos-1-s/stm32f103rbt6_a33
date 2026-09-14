# Stage 5M-R Static Drift Compensator Design

The HAL-free candidate operates on unrounded calibrated gross mass before zero/tare display semantics in the proposed future architecture. It never edits raw, calibration, zero, tare or Active configuration. Current product integration is absent.

States are DISABLED, ARMING, COMPENSATING, LOAD_CHANGE, HOLD_OFF and LIMITED. Default ARM/HOLD_OFF are 15 seconds. A 0.020 g single step or 16-observation 0.020 g displacement freezes offset; any input ambiguity also freezes. Update is at most 0.000050 g/s: 0.003 g/min, 0.18 g/h, 0.0005 g/10 s and 0.00075 g/15 s. Offset limit is 0.5 g and becomes LIMITED.

Enable is bumpless and builds the current reference. Disable is designed as stop-and-bypass with offset reported as zero; this can jump by the accumulated shadow offset, so a product UI should likely use stop-learning/retain-offset until an explicit RESET. RESET clears runtime offset only. ZERO/TARE/filter changes re-arm; calibration begin disables; calibration commit clears and re-arms; non-10 Hz profile must be rejected by integration.
