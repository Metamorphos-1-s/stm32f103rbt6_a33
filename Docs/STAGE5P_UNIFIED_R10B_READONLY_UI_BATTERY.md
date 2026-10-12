# Stage 5P Unified-R10B read-only UI and battery evidence

Date: 2026-10-12 Asia/Shanghai

## Baseline and scope

Independent worktree started at
`b7e3e9dfdaacc9ac22beeb9490dc85a020060b6e`.
No firmware, configuration, R5 mode, calibration, ZERO, TARE, SAVE or power
operation was performed. Existing user `.pyc` changes were not staged.

Source audit confirms that a short FUNCTION press on a RUN page rotates
NET/GROSS/TARE/BATTERY pages. An unchanged menu edit is only a candidate until
the separate long-FUNCTION SAVE path; short TARE in an identified menu edit or
list cancels, while short TARE on the RUN page performs weighing tare. The
single on-site instruction explicitly prohibited TARE when the page was
uncertain. Menu behavior was not called hardware PASS without visual feedback.

## Preflight and capture

The initial readback was `0x0524/0x010C`, 10 Hz/filt1/strength3, valid
calibration, R5 ACTIVE+STATIC, Checkweigh OFF, revision/saved 20/20,
dirty/fault/overrun 0/0/0, and volatile offset `-40,430 ug`.

An independent read-only capture contains 778 valid samples over 719.531 s,
from `2026-10-12T08:58:50.968177Z` through
`2026-10-12T09:10:50.504751Z` (16:58:50 to 17:10:50 local). No read errors
were recorded. Official stable was true in all 778 polls. Within this
continuous window, firmware/Map and R5 ACTIVE+STATIC remained unchanged;
revision/saved stayed 20/20, dirty/SAVE/fault/overrun stayed zero. Offset
varied naturally from +5,980 to +18,039 ug, ending +16,385 ug, without an
observed reset inside the window. The initial preflight and this capture are
not continuous evidence across the intervening time, so offset preservation
outside the capture window is not claimed.

Raw status evidence:
`Results/stage5p_unified/hardware/stage5r10b_20261012/samples.jsonl`.
SHA-256:
`822226FAFEA45015AE69DEED76835C16033AD521832186A6701C66B9DDFC7FB3`.

The final readback was `0x0524/0x010C`, 10 Hz/filt1/strength3, R5
ACTIVE+STATIC, Checkweigh OFF, revision/saved 20/20, dirty/fault/overrun
0/0/0, SAVE count 0, offset +14,506 ug. No reset or forced zero offset was
attempted.

## UI and battery limits

No on-site key/display report arrived during the capture. Consequently the
drIFt/ALArn candidate confirmations, STAR/HASH navigation, menu TARE cancel,
and observed battery page percentage are NOT RUN, not inferred from Modbus.
PLC/menu concurrency was also NOT RUN because it requires a mode change that
could clear the volatile offset.

The public 0x010C Modbus map exposes configured brightness (3), but not the
actual TM1628 brightness, battery ADC raw value, corrected voltage or panel
text. The recorder explicitly stores these unavailable fields as null. The
firmware's frozen software rule is corrected_mv <= 12,800 -> 0%, >= 16,800
-> 100%, else rounded `(corrected_mv - 12800)*100/4000`; the nominal battery
correction is +400 mV. Neither an actual voltage nor a panel percentage was
available to compare in this run. The percentage is a voltage estimate, not
a state-of-charge measurement.

Although stable stayed true for more than 10 minutes, no actual brightness
or no-key event source was recorded, and no visual confirmation was returned.
Idle dimming and wake are therefore INCOMPLETE. Low-voltage stimulation was
NOT RUN because no safe controllable supply was available or authorized for
this read-only stage.

| Gate | Result |
| --- | --- |
| Fresh/final read-only identity and safety | PASS |
| Continuous 12-minute status capture | PASS |
| No unexpected config/SAVE/R5 mode change in capture | PASS |
| Menu confirmation/navigation/cancel | NOT RUN: no on-site key report |
| PLC/menu concurrency | NOT RUN: volatile offset preserved |
| Battery voltage/percentage physical comparison | NOT RUN: no public voltage field or panel report |
| Stable-time precondition | PASS: 719.531 s observed stable |
| Actual dimming/wake | INCOMPLETE: hardware brightness not observable |
| Physical low-voltage threshold | NOT RUN |
| 0x0524 physical 500 g load/unload and nonzero-offset DOSING | NOT RUN |

Historical boundaries remain unchanged: R9 SAVE/power-cycle PASS;
single-epoch resource qualification INCOMPLETE; cross-record engineering
evidence PASS; A13B long-duration efficacy INCONCLUSIVE. This is not a formal
Release or metrology qualification.
