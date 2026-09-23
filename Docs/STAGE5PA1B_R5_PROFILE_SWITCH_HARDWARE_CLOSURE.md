# Stage 5P-A1B R5 Profile-Switch Hardware Closure

## Result

**STAGE 5P-A1 COMPLETE; MODBUS THROUGHPUT AND R5 PROFILE-SWITCH
COMPATIBILITY CLOSED; 10/40 HZ ENGINEERING PATH READY; STAGE 5P-A2 ENTRY
APPROVED; FORMAL QUALIFICATION WAIVERS REMAIN.**

This is an engineering closure, not a formal release or metrology result.

## Candidate And Configuration

The exact 0x0519 candidate was 107,532 bytes with SHA-256
`2101D1F344B63AA9A6983B56A90EBFF3AF8167B838B506611776D71C37D3C6AA`.
It was programmed application-only to pages 0-105 and Verify passed. The V3
configuration region SHA before test was
`A615A475C2D7B5D64126EAEB3AAE9E3D094348FA4C4AEC7BDB8EF0010D4A3BB4`.
No SAVE, ZERO, TARE, CLEAR TARE or calibration action was issued. The
approximately 500 g load remained in place.

## H1 OFF And H2 SHADOW STATIC

H1 completed the required sequence with R5 OFF+SHADOW and offset zero:

| Segment | Measured rate |
|---|---:|
| 10 Hz filt3 initial | 9.9848 Hz |
| 40 Hz filt0 | 39.9396 Hz |
| 10 Hz filt1 | 9.9849 Hz |
| 40 Hz filt2 | 39.9361 Hz |
| 10 Hz filt3 | 9.9849 Hz |
| 40 Hz filt3 | 39.9395 Hz |

H2 completed three 10-to-40-to-10 rounds across filt0-filt3. Every controlled
transition started a new admission baseline, holdoff 15, and reference fill 0.
After 20 seconds each segment had holdoff 0 and reference fill approximately 5.
Application remained SHADOW, mode remained STATIC, offset remained zero, and
no segment entered LIMITED.

## H3 Active Static And H4 Active Dosing

H3 retained ACTIVE+STATIC across both directions. Each switched segment
restarted holdoff/reference, authoritative mass and D1-D conditioned mass kept
updating, and `uncompensated - corrected == offset == 0`. The first host
analysis attempt referenced a nonexistent CSV column; its raw 99 records were
preserved and reanalyzed using `conditioned_display_ug` before testing
continued.

H4 ran 10 Hz/filt0, 40 Hz/filt3 and 10 Hz/filt1 for 30 seconds each. Mode was
always DOSING, application always ACTIVE, reference/observation remained zero,
offset was identical on every sample, and the corrected/uncompensated relation
had zero error.

## H5 Exact-Candidate Rate And Counter Regression

| Rate | filt0 | filt1 | filt2 | filt3 |
|---|---:|---:|---:|---:|
| 10 Hz | 9.98492 | 9.98397 | 9.98464 | 9.98459 |
| 40 Hz | 39.93979 | 39.93851 | 39.94113 | 39.93717 |

Every 30-second run used the normal five-block Modbus load. Boundary SWD
snapshots briefly halted the core outside each measurement window and showed
exact `CS1237 sample == MeasurementBridge consumed`, backlog 0-to-0, read-error
delta zero and overrun delta zero. Modbus recording had zero read errors and
poll gaps; device fault remained zero. Two earlier non-atomic counter snapshot
attempts differed by one count while the running core was being read; both are
preserved and classified as host evidence-infrastructure failures.

The exact product candidate intentionally contains no DWT diagnostic RAM.
Stage 5P-A1's code-equivalent optimized diagnostic directly measured zero
App_Run executions above 25 ms. This A1B run did not substitute that result for
the product regression: it reran all eight exact-candidate rate/filter cases
and obtained full target rate with exact counter conservation. The only product
change after the A1 DWT image is the profile-event call during reconfiguration;
it is inactive during steady H5 windows.

## Safety And Rollback

No legal transition entered SEQUENCE or TIMESTAMP LIMITED. Software injection
continues to detect skipped, large-jump, duplicate and backward sequences and
equal, backward or greater-than-250 ms timestamps. Nonzero positive/negative
offset preservation is software-qualified; hardware offset remained zero, so
nonzero-offset hardware qualification is not claimed.

The configuration SHA immediately before rollback and after rollback remained
exactly the pretest value. Frozen 0x0517 was programmed application-only and
Verify passed. Final state is 0x0517, Map 0x0104, 10 Hz/filt3/strength3,
OFF+SHADOW, Checkweigh OFF, offset/reference/evaluation zero,
fault/overrun/dirty/SAVE zero, and revision/saved 8/8.

Still deferred: formal Release, metrology certification, cross-sensor testing,
D1-C nonzero-offset long-duration display qualification, new 12-hour testing,
physical sensor disconnect qualification, external DRDY timing, ASan and UBSan.
