# Stage 5P Unified-R10C panel observation

Date: 2026-10-12 Asia/Shanghai

## Scope and evidence

Independent branch `stage5p-unified-r10c-panel-observation` began at
`5dddbd9c927ccccaa8ecb4f4721b8cc912100a10`. No firmware, SWD, power,
SAVE, ZERO, weighing TARE, calibration, PLC configuration or R5 mode action
was performed by the host. The original worktree and its user files were
left untouched.

The 0x0524 source was checked before issuing instructions. On a RUN page,
short FUNCTION cycles NET/GROSS/TARE/BATTERY pages; long FUNCTION enters
the menu at UnIt. At UnIt, short STAR/HASH/STAR/HASH unlocks advanced items
and enters CAP. In the advanced list, HASH/STAR navigate forward/backward.
Short FUNCTION on drIFt or ALArn opens the current choice; another short
FUNCTION confirms an unchanged candidate and returns to the list. Short
TARE cancels only when definitely on a menu edit/list page; on a RUN page
it would execute weighing tare. Long FUNCTION in the menu would request
SAVE and was explicitly prohibited.

One read-only preflight at `2026-10-12T09:56:09.635910Z` and one read-only
postflight at `2026-10-12T10:01:11.134392Z` are archived in
`Results/stage5p_unified/hardware/stage5r10c_preflight.json` and
`Results/stage5p_unified/hardware/stage5r10c_postflight.json`. There was
no continuous recorder, no waiting tool and no host write command.
The preflight SHA-256 is
`8AD7FC53A4A3A1C52AFAD86370B03DC30ABFEF9F72EC0DAE99CDAFC93FF8CEFC`;
the postflight SHA-256 is
`21AFB78218774D8E16D6418EC8DEE35EC192A74A0BA0C94C5F34B0377B50F787`.

The operator was given one complete sequence: view battery page and return,
enter advanced menu, view and confirm unchanged drIFt and ALArn candidates,
verify HASH/STAR list navigation, and cancel only on an identifiable menu
page. They were instructed to stop at the first unexpected display. The
operator's complete reply was: **"所有操作已完成，无异常"**. No actual panel
character sequence, percentage, indicator color, individual key timestamps
or brightness observation was supplied. The reply is a user visual report,
not a Modbus readback of those details; its exact response UTC is unrecorded.

## Read-only comparison

| Field | Before | After |
| --- | --- | --- |
| Firmware/Map | 0x0524/0x010C | 0x0524/0x010C |
| Profile/filter | 10 Hz, filt1/strength3 | unchanged |
| Calibration valid | 1 | 1 |
| R5 running application/mode | ACTIVE/STATIC | ACTIVE/STATIC |
| R5 state/limited | 5/0 | 5/0 |
| Volatile offset | +14,426 ug | +13,246 ug |
| Checkweigh mode | OFF | OFF |
| Revision/saved | 20/20 | 20/20 |
| Dirty/fault/overrun/SAVE request count | 0/0/0/0 | 0/0/0/0 |
| MCU uptime | 3,704,077 ms | 4,005,525 ms |

The two snapshots support unchanged configuration, no observed SAVE or
safety counter increase, and no offset reset between their endpoints. The
small offset change is consistent with STATIC compensation, but the interval
was not continuously observed. The probe does not independently expose the
saved R5 request or reference in these snapshots; their individual values
are **not confirmed** here.

## Results and limits

| Item | Result |
| --- | --- |
| Battery page entry and return | USER-OBSERVED PASS overall; exact characters, percentage and indicator detail not reported |
| drIFt/ALArn unchanged candidate confirmation and list navigation | USER-OBSERVED PASS overall; each exact character and key sequence not independently logged |
| Menu candidate cancel | USER-OBSERVED PASS overall; exact menu-page text not reported |
| No unexpected SAVE/configuration/mode/offset reset at endpoints | PASS by two read-only snapshots; no continuous claim |
| Battery +400 mV correction/percentage numeric comparison | NOT RUN: public Modbus has no battery ADC/corrected voltage, and user supplied no percentage |
| Actual brightness timing and wake measurement | INCOMPLETE: earlier visual dimming report retained, no timed brightness readback or new specific observation |
| Physical low-voltage threshold/recovery | NOT RUN |
| PLC/menu concurrent mode modification | NOT RUN: volatile R5 state preserved |
| 0x0524 500 g load/unload and nonzero-offset DOSING | NOT RUN |

Historical boundaries remain: R9 SAVE and real power recovery PASS;
single-epoch resource qualification INCOMPLETE; cross-record nine-path
engineering evidence PASS; A13B long-duration efficacy INCONCLUSIVE.
Neither formal metrology nor broader sensor/profile qualification is
advanced by this panel observation.
