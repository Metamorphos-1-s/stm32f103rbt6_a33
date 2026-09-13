# Stage 5L Measurement Characterization

Status: hardware data collection in progress. Production measurement algorithms are unchanged.

## Frozen baseline

- Stage 5K frozen commit: `61ab01330c19ec2f01f7af0f3723ce055ce931a8`.
- Stage 5L branch start: same commit.
- Firmware / Map / Public Schema / Persistent Format: `0x0510 / 0x0104 / 2 / 3`.
- Active canonical SHA-256: `91D346E87BD112EFAC3B513A8CAFBBDDE9642069A15280DB7565374378BA43E1`.
- Actual frozen persistence metadata after user-authorized restore saves: revision `7/7`, Slot A/B sequence `7/6`.
- Profile 0: 10 Hz, gain 128, filter mode 3, strength 3.
- Calibration: raw zero -43,989; raw span -487,850; span mass 500,000,000 ug; sequence 1.

## L1 empty baseline

Run `20260912T192400Z_empty_10m` captured 5,266 records over 600.146 seconds. Device-side overrun delta was zero, stability was 100%, and no write or Flash operation occurred. Host observation skipped 723 device sequences while performing periodic display-condition and runtime-drift reads; this is polling coverage, not a CS1237 overrun.

| Metric | Raw ADC | Filtered raw | Net mass | Final display |
|---|---:|---:|---:|---:|
| Mean | -44,023.084 counts | -44,023.026 counts | -0.025880 g | 0.02 g |
| Standard deviation | 13.421 counts | 9.793 counts | 0.011032 g | 0 g |
| Peak-to-peak | 100 counts | 62 counts | 0.069842 g | 0 g |
| Linear drift/hour | +155.161 counts | +158.798 counts | -0.178882 g | 0 g |

The 10-minute slope is not treated as a thermal-drift conclusion because the following 60-minute run shows a much smaller long-window slope.

## L2 hot-machine empty drift

Run `20260912T193600Z_empty_hot_60m` captured 31,588 records over 3,600.010 seconds. It is a hot-machine baseline; a controlled cold-start run remains pending. Device-side overrun delta was zero and stability was 100%.

Whole-window results:

- Raw ADC mean -44,009.950 counts, standard deviation 12.800, peak-to-peak 112, linear drift -10.948 counts/hour.
- Filtered raw mean -44,009.978 counts, standard deviation 8.634, peak-to-peak 63, linear drift -10.997 counts/hour.
- Net mean -0.040578 g, standard deviation 0.009726 g, peak-to-peak 0.070968 g, linear drift +0.012387 g/hour.
- Non-overlapping Allan-equivalent deviation of net mass: 1 s 0.003069 g; 10 s 0.003626 g; 60 s 0.002940 g; 300 s 0.005052 g.
- Final display was initially held at +0.02 g, then released and remained at -0.05 g. This separates display-condition behavior from the much smaller long-window raw slope.

Segment slopes vary substantially over short windows, including raw +74.4 counts/hour at 0-5 minutes and -81.4 counts/hour at 20-30 minutes. Therefore a preheat shorter than 20 minutes can materially bias a drift estimate; the full-hour result is the current credible hot-machine baseline.

## Current limits

Battery voltage and internal zero-offset raw are not exposed by Map `0x0104`. Calibrated unfiltered mass is not retained in the production snapshot. These fields remain blank rather than being invented. Cold-start drift, 500 g creep, zero return, repeated cycles, filter comparison, slow fill, and 40 Hz exploration remain pending.

## L3 500 g constant-load creep

Run `20260913T064617Z_creep_500g_30m` captured 15,547 records over 1,800.097 seconds after the operator reported the load stable. Device overrun was zero and stability was 100%. Because acquisition began after stabilization, this run does not support load-step t10/t90 claims.

- Net mass at 1/5/10/20/30 minutes: 500.0454 / 500.0814 / 500.0829 / 500.0839 / 500.0656 g.
- Whole-window net mean 500.0753 g, standard deviation 0.0145 g, peak-to-peak 0.0969 g, linear drift +0.0166 g/hour.
- Raw and filtered-raw slopes were both approximately -14.7 counts/hour.
- Final display changed from 499.98 g near one minute to 500.06 g by five minutes and then remained there.

## L4 unload and zero return

Run `20260913T072000Z_zero_return_15m` captured 7,878 records over 900.039 seconds. Device overrun was zero and stability was 100%. The operator confirmation occurred after physical removal, so the real unload edge precedes the first record by an unknown human-response delay.

| Time after capture start | Raw ADC | Filtered raw | Net | Final display |
|---|---:|---:|---:|---:|
| immediate | -44,066.9 | -44,067.5 | +0.0242 g | +0.02 g |
| 1 min | -44,066.1 | -44,064.3 | +0.0206 g | +0.02 g |
| 5 min | -44,031.8 | -44,031.9 | -0.0159 g | +0.02 g |
| 10 min | -44,004.7 | -44,005.0 | -0.0462 g | +0.02 g |
| 15 min | -44,004.4 | -44,003.8 | -0.0476 g | -0.05 g |

Raw and filtered raw return together; there is no evidence of an independently retained filter tail in this interval. The slow raw movement instead implicates the analog measurement chain, load-cell/mechanical recovery, thermal movement, or mounting stress. Without a synchronous reference measurement it is classified only as analog measurement-chain return. The display conditioner independently held +0.02 g before releasing to -0.05 g.
