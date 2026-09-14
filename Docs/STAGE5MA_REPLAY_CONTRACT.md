# Stage 5M-A Replay Contract

Replay version `stage5ma-replay-v2` consumes MCU timestamp, source sample sequence, raw count, calibration context, run filter/profile context and a separate physical label. Historical files are read-only and are bound by Git-blob SHA-256.

Development, holdout and regression are split only by complete run. No adjacent samples from one continuous capture cross a split. Slow-fill run 1 and cold control 1 are development; slow-fill run 2, cold control 2, filt1/filt3 comparisons, 500 g creep and cycles 7-10 are holdout. The split is frozen in `dataset_split.json`.

The v1 attempt treated FC03 observation omissions as WeightEngine input gaps and consequently never asserted stable. That result is preserved. G1/G2 demonstrate that these sequence gaps are host observation gaps, not device processing gaps. V2 processes only observed values in their original order, creates no replacement sample, records every omitted source sequence, and uses a contiguous replay-input sequence. Actual input timestamp gaps over 250 ms still enter SETTLING. This one interpretation correction is the documented reason holdout was reopened; parameters did not change.

Output columns are calibrated mass, fast mass, display mass, state, stable candidate, innovation, 16-sample slope, noise range and gap/disturbance flags. JSON is UTF-8/LF with a final LF; CSV uses explicit LF. Same input/config/version is deterministic. Timestamp and sequence arithmetic is unsigned 32-bit. Truncated CSV, missing columns, split overlap and invalid data fail closed.

Current filt baselines use the device-published net output because omitted raw samples prevent exact internal filter reconstruction. Candidate results are offline subsequence characterization, not hardware results. Python and HAL-free C integer state/output must match exactly per record before a candidate can be considered.
