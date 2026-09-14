# Stage 5M-R Offline Validation

Result: no safe candidate.

The candidate improved cold control 2 slope from 0.1680 to 0.1157 g/h and 500 g static slope from 0.01637 to 0.00103 g/h. It amplified cold control 1 from 0.02351 to 0.03811 g/h, violating the frozen no-reverse-amplification rule. That run is not silently declared inconclusive because the available record shows a directional worsening.

Real load/unload final shadow loss was 0.00025/0 g. Continuous approximately 0.0466 g/s change retained its slope (432.0738 versus 432.0691 g/h) with only 0.00055 g offset movement. Python/C matched 1000 actual samples exactly.

The minimum observed positive local increment was 0.001127 g, but it is not reliably distinguishable from noise. Synthetic noise-free scans across 2/5/10-second pauses reliably detect 0.020 g; increments from 0.001 g remain protected only by the slow-rate loss budget, not guaranteed detection. At 10 seconds the theoretical maximum compensation is 0.0005 g.

Because one static run is amplified, hardware shadow validation is not authorized.
