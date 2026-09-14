# Stage 5M-R4 Independent Validation

R4 begins from exact R3 commit `ca3008a56bbd3577f71c65b60329c28052551285`. GitHub Actions run 34870894842 completed successfully at that SHA. A clean Python 3.13 replay reproduced the frozen selected parameters and all R3 gates.

The first blind replay will use 180-second windows, 15-second block medians, whole-window OLS, 0.002 g deadband, 1.0 g/h declared-static ceiling, 0.020 g fast step, 75 micrograms/s cap, 875 permille damping and 15-second re-enable holdoff without tuning.

New captures are pending. A pre-capture identity discrepancy must be resolved: the device configuration reports 3 kg capacity/rated load-cell capacity, while the R4 task calls the current physical sensor 6 kg. Both values will be preserved distinctly; neither is inferred.
