# Stage 5M-A Adaptive Filter Design

The evaluated fixed-point reference is a robust dual-path IIR. It is intentionally isolated under `Experimental/stage5ma` and linked only to Host tests.

Fast mass uses `alpha=1/2`; slow mass uses `alpha=1/8`. A 16-observation fast-history window supplies range and trend. A single isolated deviation over 1 g is median-of-three suppressed and reported as DISTURBANCE; a confirmed second sample becomes TRANSIENT, bounding real-step delay to one observation. TRANSIENT and SLOW_CHANGE display the fast path. SETTLING/STATIC blend continuously toward the slow path with `alpha=1/4`. During a confirmed transient the slow state is synchronized to fast to avoid a later switch discontinuity.

Frozen thresholds are 0.2 g transient innovation/step, 0.06 g quiet range, 0.03 g 16-observation slow trend, 1 g isolated disturbance, 0.6 s minimum settling and 1.5 s static hold. These are screening parameters, not product constants. The states are STATIC, TRANSIENT, SETTLING, SLOW_CHANGE and DISTURBANCE. Stable is allowed only in STATIC after hold.

All mass state is signed int64 micrograms. Divide-by-power-of-two rounds symmetrically; differences saturate explicitly at int64 limits. History and loops are bounded. State size is 256 bytes, ARM `-Os` object text is 1724 bytes, Process stack is 104 bytes, and the module performs no dynamic allocation or int64 division.

Reset reasons cover power-on, calibration commit, profile/filter change, zero, tare, actual sample gap and fault recovery. Unit change requires no reset because internal mass stays in micrograms. A later integration must separately review whether ZERO/TARE should retain fast/slow continuity while immediately changing offsets; Stage 5M-A does not integrate this module.

There is no deadband, integer attraction, automatic zero, tare learning, calibration adjustment or low-frequency trend subtraction. Near-rail remains observable. Persistent or consecutive anomalies cannot be ignored indefinitely because median suppression applies only when the two previous inputs agree.
