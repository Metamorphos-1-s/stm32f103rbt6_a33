# Stage 5P 0x0530 zero-return read-only hardware evidence

## Conclusion

**OFFLINE REVIEW COMPLETE; ZERO-RETURN HARDWARE EVIDENCE COLLECTED;
CAUSALITY AND FORMAL ZERO-RETURN QUALIFICATION REMAIN OPEN.**

The fixed firmware was preserved. No firmware, configuration, R5 mode,
ZERO, TARE, calibration, SAVE, reset or SWD write was performed. The task
branch is `stage5p-zero-return-r5-review-hardware` at fixed source
`fbdffa59e55d3fcb7efc08bee3eb90edd4f1d7ac`.

## Software and preflight

The 1,197 Git source/helper blob IDs and 25 review evidence entries from
the fixed commit matched the supplied review manifest. All 19 fake-serial
simulation tests passed. The actual instrument port was reidentified as
COM5, CH340 VID 1A86/PID 7523; no historical COM3 was present.

Fresh preflight read 0x0530/0x010F, high-word-first, profile 0 at
40 Hz/filt1/strength3/gain3, calibration raw zero/span 41868/485780 with
500 g span, R5 ACTIVE+STATIC non-LIMITED, checkweigh STATIC, active slot A
sequence 23, revision/saved 23/23, dirty/fault/overrun 0. Initial empty
platform weight was approximately 0.377 g. A 100-request FC03 link gate
returned 100/100 valid responses with zero timeout, CRC, exception or port
errors.

## Capture integrity

The primary read-only recorder ran from 03:42:26 UTC for its 3,600-second
limit and produced 25,771 JSONL rows. The tail extension produced a further
1,272 rows from 04:46:01 to 04:49:01 UTC after the primary process ended.
Both segments had zero read errors. Across all 27,043 rows:

* authority equations had 0 errors;
* `gross = uncompensated - applied_offset` and `net = gross - tare` held in
  every atomic authority response;
* fault and overrun rows were 0;
* R5 application/mode and revision/saved remained `(ACTIVE, STATIC, 23/23)`;
* no configuration or SAVE count change occurred.

The primary capture SHA-256 is
`20574BEC82C8F5154E3CB7A4D07D2CBA2C43781ED51A877D4B736BBF7E33D9A2`.
The tail extension SHA-256 is
`D0BE4DA3E3017CEF538435E1B88D2F55B64AE73D84155BD75D387F244252CE51`.
The FC03 authority block is atomic; diagnostic/display blocks are separate
requests and are not treated as one atomic CSV row. Host polling omitted
unobserved 40 Hz samples; no interpolation was performed.

## Three complete load/unload cycles

Edges below are adjacent observations crossing the 250 g authority
threshold. Operator message times are retained separately and are not
physical edge timestamps.

| Event | Data-inferred interval UTC |
| --- | --- |
| Load 1 | 03:48:34.677-03:48:34.805 |
| Unload 1 | 03:52:05.046-03:52:05.181 |
| Load 2 | 04:25:35.248-04:25:35.391 |
| Unload 2 | 04:29:47.648-04:29:47.785 |
| Load 3 | 04:33:22.512-04:33:22.651 |
| Unload 3 | 04:37:19.075-04:37:19.214 |

All requested 120-second load and unload waits were covered in the primary
record. The final 600-second post-Unload-3 requirement was **not continuous**:
the primary recorder ended after roughly five minutes and the extension
started around 04:46:01, leaving a multi-minute gap. The extension continued
past 600 seconds from the physical unload, but this gap makes the complete
continuous-tail item **INCOMPLETE**.

Using the fixed `[−45,−15)` load-pre window and `[15,45)` post-unload
window, the atomic authority medians were:

| Cycle | Load-pre net | Loaded net | Early net increment | Unload post net | Post-unload minus pre |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 0.359401 g | 500.347052 g | 499.987651 g | 0.370006 g | +0.010605 g |
| 2 | 0.379768 g | 500.351283 g | 499.971515 g | 0.422741 g | +0.042973 g |
| 3 | 0.414480 g | 500.388624 g | 499.974144 g | 0.405351 g | −0.009129 g |

The 60-90 s and 90-120 s post-unload medians are retained in
`analysis_local.json`; they vary with waiting age. No formal tolerance or
砝码 certificate was supplied, so these are observations, not zero-return
PASS results.

For the same early windows, the means decompose as follows:

| Cycle | Change before compensation | Change in offset | Change after compensation |
| --- | ---: | ---: | ---: |
| 1 | +0.028715 g | +0.022224 g | +0.006491 g |
| 2 | −0.000207 g | +0.011614 g | −0.011822 g |
| 3 | not promoted to a causal claim without a fixed independent OFF control | | |

The arithmetic is valid within each atomic R5 response, but there was no
OFF control and no synchronized temperature/mechanical reference. The data
cannot establish whether the return differences arise from R5, mechanical
recovery, temperature, or a shared platform drift.

## Final state

Final read-only preflight remained 0x0530/0x010F, 40 Hz/filt1/strength3,
R5 ACTIVE+STATIC, checkweigh STATIC, slot A sequence 23,
revision/saved 23/23, dirty/fault/overrun 0. The platform was empty after
the owner removed the final 500 g load. Final volatile offset was about
−9,786 ug; it is not a saved configuration value. Temperature was not
measured. No firmware, configuration, or mode change was made.

## Evidence boundary

| Item | Result |
| --- | --- |
| Fixed commit/blob and 19 simulated tests | PASS |
| Fresh COM5 identity and 100/100 link gate | PASS |
| Application/configuration writes | NOT RUN by design |
| Three complete physical load/unload sequences | PASS in observed scope |
| Atomic R5 weight equations | PASS, 0 errors |
| Continuous 600 s final empty tail | INCOMPLETE due recorder-duration gap |
| Formal zero-return qualification | NOT CLAIMED |
| R5 causal improvement/deterioration | INCONCLUSIVE without OFF/thermal/mechanical control |
| Long-duration drift, cross-sensor and metrology qualification | NOT RUN |

Raw JSONL, RTU frames, operator markers, pre/post snapshots, analyses and
SHA manifest are under
`Results/stage5p_zero_return_readonly/20261019T033815Z/`.
