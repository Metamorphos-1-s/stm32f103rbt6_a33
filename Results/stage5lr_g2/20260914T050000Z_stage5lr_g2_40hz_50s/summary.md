# Run summary

Result: PASS for the MCU-internal software path, not physical 40 Hz requalification. Config `0x1C` and restore `0x0C` both read back correctly. Ready/read counts are 2006/2006; two config frames and four settling frames explain the six-frame difference to FIFO. FIFO push/pop, MeasurementBridge, WeightEngine accept and sample-sequence increments are all 2000. Effective ready rate is 39.8966 Hz. There are no read failures, FIFO overruns, EventQueue drops, rejected samples, near-rail values or million-count raw jumps. The exact product Release was restored and byte verified; configuration Flash remained identical through the user-confirmed final physical power cycle.

SWD cannot prove the external DRDY, SCLK or DOUT electrical waveform. The historical 16.496 Hz observation was not reproduced, so external timing evidence remains required.
