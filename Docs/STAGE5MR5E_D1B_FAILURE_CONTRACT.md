# Stage 5M-R5E-D1-B Failure Contract

The D1-B hardware capture is classified **DEVELOPMENT / OPENED REGRESSION**.
It may be used to explain the failure, search D1-C parameters and prevent a
recurrence. It is not a D1-C holdout and cannot support an independent D1-C
hardware qualification claim.

The immutable capture contains 147 records. `slow_display_gate/samples.csv`
has SHA-256
`2EFD5FEBBBB259CAE3DC5B271558F2FB32D03635DA57E4BD44616D267F480FA6`.
The panel remained at -4 counts while the authoritative quantized target
ranged from -11 to -6 counts and ended at -9 counts. Offset reached
+172,545 ug. Maximum panel lag was 7 divisions and panel update count was zero
during approximately 146 seconds.

The rejected algorithm required one exact target count to remain unchanged for
1,000 ms. Real 10 Hz measurement noise moved the target among adjacent counts,
so each change replaced the candidate and restarted confirmation even though
every target remained below the panel. D1-C therefore tests persistent
direction relative to the current panel count, not exact-count persistence.

The rejected 0x0513 application BIN is 101,480 bytes with SHA-256
`25921F905BBD03F1EE3F21309479DF311005379A440A7164F882C0245F600E6F`.
It was application-only flashed and verified, then rejected by the fresh
hardware holdout. Before rollback, the final preserved snapshot included
offset +140,683 ug, reference -126,008 ug, evaluation count 252, panel mass
-191,911 ug and display anchor -191,911 ug.

The application was rolled back and device-verified as 0x0512. Its BIN is
100,792 bytes with SHA-256
`BA8F02B2024042D601FD7F2D75BEF9E1004AACAE16852DD97CD2B28777BAF6B9`.
The configuration-region SHA-256 remained
`A615A475C2D7B5D64126EAEB3AAE9E3D094348FA4C4AEC7BDB8EF0010D4A3BB4`.
Final rollback state was OFF + SHADOW, offset/reference/evaluation = 0,
revision/saved revision = 8/8 and fault/overrun/dirty/SAVE = 0.

Any D1-C parameter selected after opening this capture must be frozen before a
new hardware run. No result from this capture may be relabeled as new holdout
evidence.
