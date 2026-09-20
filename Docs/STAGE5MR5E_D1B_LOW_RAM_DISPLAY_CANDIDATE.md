# Stage 5M-R5E-D1-B Low-RAM Display Candidate

## Final result

**STAGE 5M-R5E-D1-B NO ACCEPTABLE LOW-RAM DISPLAY CANDIDATE; FIRMWARE
REMAINS 0x0512; R5 ALGORITHM UNCHANGED; STAGE 5N ENTRY DEFERRED.**

The 1-division, 1,000 ms confirmation candidate passed synthetic replay and
Python/C parity, but failed the new hardware holdout. The real 10 Hz desired
count moved repeatedly between adjacent counts, resetting confirmation often
enough that the panel remained at -4 divisions while the authoritative value
reached -11 to -6 divisions. The maximum observed lag was 7 divisions and the
panel made no transition in 147 consecutive one-second records. This violates
the slow-display gate. The run was stopped as a candidate failure before the
DOSING load/unload phase; its data was not used to tune or retest a replacement.

The original recorder summary remains `RUNNING` because the read-only monitor
was deliberately stopped after the gate failure was conclusive. The immutable
CSV contains 147 records from 2026-09-20T13:37:17.783Z through
2026-09-20T13:39:43.742Z and has SHA-256
`2EFD5FEBBBB259CAE3DC5B271558F2FB32D03635DA57E4BD44616D267F480FA6`.
The separate failure summary records why the capture ended and prevents the
raw recorder state from being mistaken for a completed qualification.

## Candidate history

Eight candidates were compared: 1d/2d hysteresis crossed with 1/2/3/5 second
confirmation. The selected 1d + 1 second model had maximum 1d lag and 1d step
in synthetic +/-0.22 g replay, no boundary A-B-A oscillation, and no added
delay or count loss on synthetic 500 g steps. Final C/Python parity was
48,120 samples with zero mismatch; frozen R5 C/Python parity remained 25,057
samples with zero mismatch. These results remain useful research evidence but
do not override the independent hardware failure.

The candidate used 16 bytes of state. Its firmware RAM estimate was 19,448 B,
with 568 B conservative stack collision margin. It changed only the Beta panel
path while ACTIVE; authoritative WeightEngine values, PLC/Modbus/BLE, alarms,
stability, R5 input, and the R5 algorithm were unchanged.

## 0x0513 hardware evidence

The conditional 0x0513 Beta was 101,480 bytes with SHA-256
`25921F905BBD03F1EE3F21309479DF311005379A440A7164F882C0245F600E6F`.
It was flashed only after explicit authorization, using application sectors
0-99 with successful device Verify. The configuration region was not written.

The device reached ACTIVE + STATIC + TRACKING without fault, overrun, dirty or
SAVE activity. The nonzero trigger was natural: offset exceeded 0.020 g and
the authoritative display crossed more than two divisions. During the formal
slow gate, offset moved from approximately +0.168 g to +0.173 g while desired
display count ranged from -11 to -6. The panel stayed at -4 throughout. The
final hardware snapshot before rollback preserved offset +0.140683 g,
reference -0.126008 g, evaluation count 252, authoritative corrected weight
-0.243179 g, panel -0.19 g, and display anchor -0.191911 g.

## Rollback and frozen identity

After explicit authorization, only application sectors 0-98 were erased and
the previously verified 0x0512 Beta was programmed and device-verified:

- BIN: 100,792 bytes
- SHA-256: `BA8F02B2024042D601FD7F2D75BEF9E1004AACAE16852DD97CD2B28777BAF6B9`
- Firmware/Map/Schema/Persistent Format: 0x0512 / 0x0104 / 2 / 3

The configuration-region SHA-256 was
`A615A475C2D7B5D64126EAEB3AAE9E3D094348FA4C4AEC7BDB8EF0010D4A3BB4`
both before and after rollback. Final device state is OFF + SHADOW with
offset/reference/evaluation = 0, fault/overrun/dirty/SAVE = 0, and
revision/saved revision = 8/8.

The repository product target was also restored to 0x0512. Its rebuilt Beta
BIN is byte-identical to the rollback artifact. Standard Release ELF remains
172,200 bytes with SHA-256
`82E726F5B32A0DE36A5E686F62A937EC4FD9CBB488DB9733E83D2062673EF486`;
the 0x0512 Beta ELF remains
`FE5444CD2DA50E1F58BF8E6BF95F8BC53255628805B816A4A271493E5500485F`.
No subsequent hardware testing or candidate tuning was performed.

## Regression

Release and 0x0512 Beta warning-clean builds pass. Host CTest is 28/28.
The retained display-model C/Python comparison is 48,120/48,120 exact and R5
comparison is 25,057/25,057 exact. The failed hardware holdout, rollback logs,
configuration images and final state are bound by the stage evidence manifest.

Deferred items remain: an acceptable low-RAM display candidate, fresh
independent nonzero-offset hardware qualification for such a candidate, and
Stage 5N entry.
