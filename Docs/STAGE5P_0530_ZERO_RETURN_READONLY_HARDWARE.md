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

The primary read-only recorder's first and last JSONL observations were
03:42:02.911 and 04:42:02.779 UTC, spanning 3,599.869 s within its
3,600-second limit, and produced 25,771 JSONL rows. The tail extension produced a further
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
the primary recorder ended 283.563 s after the observed Unload-3 edge and the
extension started at 04:46:01.138, leaving a 238.359 s inter-observation gap. The extension continued
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
| 1 | +0.047520 g | +0.037359 g | +0.010161 g |
| 2 | +0.083681 g | +0.041590 g | +0.042091 g |
| 3 | +0.014969 g | +0.022778 g | −0.007809 g |

**Correction after independent review:** the original Cycle 1/2 decomposition
mistakenly repeated the October 16 dataset. The table above is recomputed from
this capture's atomic authority block, for all three cycles. Means decompose;
separately calculated medians need not. No OFF control is needed for this
arithmetic, but there was no OFF control or synchronized temperature/mechanical
reference for causal comparison. The data
cannot establish whether the return differences arise from R5, mechanical
recovery, temperature, or a shared platform drift.

In the early `[15,45)` post-unload windows, applied offsets were constant at
24,780 / 59,337 / 69,968 ug respectively. New reference locks were first
observed around 46.4 / 46.4 / 48.0 s after the unload edges. The A13C source
rebuilds a step's reference from `mass - offset`; it does not retain the
original empty-platform value as a mandatory return target. This describes
the implemented mechanism and does not identify the physical source of drift.

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

Independent verification at evidence commit
`f5137c0f1eb6171d4f2e459bd28539f51fb8896f` matched all available 19 of the
manifest's 24 files, and matched six FC03 payloads for each of 27,043 rows
against 324,572 CRC-checked TX/RX frames. Five manifest-listed logs were not
committed: `capture_stdout.log`, `capture_stderr.log`,
`capture_tail_extension_stdout.log`, `capture_tail_extension_stderr.log`, and
`host_simulation_19_tests.log`. The core JSONL, frames and snapshots are
available; complete historical log archival is still pending. The reported
historical 19-test log cannot be independently checked from this commit.
The subsequent offline rerun passed all 33 zero-return tests (19 existing,
14 new), using fake clients only. Reproduction and corrected decompositions
are in `Docs/STAGE5P_0530_ZERO_RETURN_INDEPENDENT_REVIEW.md` and
`Results/stage5p_0530_zero_return_independent/analysis.json`. Original raw
evidence and its manifest were preserved unchanged.
