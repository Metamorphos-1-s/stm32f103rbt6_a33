# Stage 5L-R 40 Hz Root-Cause Status

Conclusion: root cause is not yet confirmed.

## Proven from existing evidence

- MCU processed sequence delta 1,975 over 119.723 seconds: 16.4964 samples/s.
- Host captured 917 rows (46.38% coverage), but host poll speed does not explain the MCU sequence/time ratio.
- Maximum observed sequence jump is 6.
- Raw median is -41,008 counts, MAD 60 counts.
- One near-rail sample occurred at -7,864,384 counts; two adjacent single-sample jumps exceed 1,000,000 counts.
- Software FIFO overrun and mapped fault deltas were zero. This does not prove DRDY conversions were not missed before FIFO insertion.
- After restoring 10 Hz, measured processed rate was 9.981 Hz, but the earlier empty baseline did not recover after software reset or a one-hour physical cold run.

## Diagnostic correction

The Stage5LDiagnostics path no longer equates requested with effective rate. It records the expected and verified CS1237 configuration bytes and reports effective only after readback equality, RUNNING state, and non-persistent MetrologyManager rebuild. Switching pauses MeasurementBridge until completion and records settling discard count.

The static diagnostic snapshot now records driver/processed counts, read errors, FIFO overrun, backlog/current maximum, EventQueue drops, App/Bridge maximum service intervals and switch timestamps. A fixed 16-entry ring retains pre-event context plus eight post-event samples for a near-rail or >=1,000,000-count jump. It performs no allocation, blocking output or Flash write and is absent from Release.

## Open causal branches

- If measured DRDY is approximately 40 Hz but driver/processed remains near 16.5 Hz, investigate main-loop service, missed level-sensitive DRDY, configuration transaction timing and communication load.
- If DRDY itself is near 16.5 Hz, investigate CS1237 configuration encoding/readback, clock, reference, supply and physical ADC behavior.
- If DRDY and driver/processed are 40 Hz but exported sequence differs, investigate snapshot/export accounting.
- The near-rail event requires synchronized DOUT/SCLK capture and the new raw evidence ring to distinguish frame misalignment, electrical glitch and real analog excursion.

## Required hardware evidence

No logic analyzer, oscilloscope or synchronous voltage instrumentation is available in the current environment. DRDY frequency, SCLK waveform, reference/excitation and differential input remain unmeasured. Therefore no new 40 Hz switch or hardware requalification is authorized in this stage turn.

## G1 no-switch 10 Hz control

The first cold-start control, `20260914_control1_10hz_cold_60m`, completed for 3,600.11 seconds with Profile 0 and no rate switch, configuration write, ZERO, TARE, SAVE, calibration or Flash operation. Temperature was not measured. Its capture Manifest validates, and the frozen Release ELF hash remains `82E726F5B32A0DE36A5E686F62A937EC4FD9CBB488DB9733E83D2062673EF486`.

MCU sequence/time gives 9.98220 processed samples/s. The five required windows range from 9.98175 to 9.98293 samples/s, with maximum adjacent sequence jump 2. Raw moved from -43,795 to -43,780 counts; whole-run mean/stddev/peak-to-peak are -43,781.45 / 15.23 / 118 counts and the whole-run linear fit is -20.91 counts/hour. Stable ratio is 100%, device overrun delta is zero, mapped fault is always zero, and display count remains -22.

The second cold-start control, `20260914_control2_10hz_cold_60m`, also completed without a rate switch or write. It measured 9.98241 processed samples/s with zero overrun and no mapped fault, but raw moved from -43,813 to -43,892 counts and fitted at -149.21 counts/hour. Its 30-60 minute raw mean is -43,871.19, 83.26 counts below control 1; the corresponding net-mass means differ by 93,795 micrograms. A short, non-near-rail raw excursion occurs at about 1,611 seconds, with maximum adjacent jump 92 counts, before a sustained baseline movement develops in the 30-45 minute region.

The two valid no-switch observations therefore do not establish a repeatable cold-start envelope. Temperature was unmeasured in both runs, so thermal and mechanical contributions cannot be separated. The discrepancy is not evidence that a 40 Hz transition caused the drift, because neither run switched rate; it also does not explain the earlier near-rail 40 Hz event. Algorithm compensation remains prohibited.

Before any future 40 Hz run, acceptance thresholds must be declared from the CS1237 datasheet and a no-switch 10 Hz cold-control envelope. Run no-communication, RS485-only and RS485+BLE cases separately. Any run ends with 10 Hz restore, physical power cycle, unchanged V3 slots/revision/dirty, and raw baseline inside the predeclared envelope.

Current containment: Profile 0 / 10 Hz allowed; Profile 1 / 40 Hz diagnostic-only; 640/1280 Hz prohibited.
