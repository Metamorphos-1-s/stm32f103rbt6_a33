# ADC Noise Comparison

Status: SOFTWARE ANALYSIS OF PROVIDED RUNS

| test_id | mode | load_g | valid_samples | duration_s | mean_count | std_count | detrended_std_count | peak_to_peak_count | mad_count | drift_count_per_min | equivalent_std_g | equivalent_peak_to_peak_g | lost_samples | fifo_overruns |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| D_sensor_zero | channel_a | 0 | 6094 | 599.997 | -43399.0809 | 48.8959598 | 24.7421508 | 273 | 28 | 14.5897891 | N/A | N/A | 20 | 0 |
| E_sensor_500g | channel_a | 500 | 6109 | 600.012 | -485934.001 | 18.1859168 | 17.2191392 | 147 | 12 | -2.02666515 | N/A | N/A | 5 | 0 |
| F_sensor_zero_after_500g | channel_a | 0 | 6114 | 599.978 | -43347.6389 | 19.4547574 | 17.4218145 | 132 | 13 | 2.99882587 | N/A | N/A | 0 | 0 |

## Approximate variance layers

- `var_adc`: N/A
- `var_pcb_increment`: N/A
- `var_bridge_supply_increment`: N/A
- `D_sensor_zero` sensor/mechanical increment: N/A
- `E_sensor_500g` sensor/mechanical increment: N/A
- `F_sensor_zero_after_500g` sensor/mechanical increment: N/A

The decomposition assumes approximately independent noise sources. Correlated noise, drift, temperature effects, and mains interference cannot be isolated by simple variance subtraction. A single short run does not prove any component has reached its performance limit.
