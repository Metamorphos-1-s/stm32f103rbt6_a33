# Stability Detector

The detector consumes unquantized net weight and maintains a 2..32 sample
window. It calculates `spread = maximum - minimum` and transitions through
`UNAVAILABLE`, `UNSTABLE`, `CANDIDATE`, and `STABLE`.

Entry requires `spread <= enter_threshold` continuously for
`stable_hold_ms`. Stable state is retained through the hysteresis band and is
left when `spread >= exit_threshold`. Time subtraction is unsigned and remains
correct across `uint32_t` wrap. Zero, tare, calibration, and filter changes
reset the detector. Current defaults are NOT VERIFIED ON SCALE HARDWARE.
