# Stage 5M-R5E-D1-D Unified Display Conditioner

## Result

**STAGE 5M-R5E-D1-D UNIFIED DISPLAY CONDITIONER ENGINEERING BETA READY;
GENERAL OFF/SHADOW/ACTIVE DISPLAY COHERENCY QUALIFIED; R5 AND CHECKWEIGH
ALGORITHMS UNCHANGED; FORMAL RELEASE QUALIFICATION REMAINS SEPARATE.**

The authorized hardware session completed. The device now runs 0x0517 and
ends in OFF + SHADOW with Checkweigh OFF, all formal outputs off, offset,
reference and evaluation zero, fault/overrun/dirty/SAVE 0/0/0/0 and
revision/saved 8/8.

Start HEAD is `e6308a38796426296f2663831cefaefce5a0225a`. The frozen software
and evidence HEAD is `4e16cd08e42a636febbef24e7e0500af76e173c4`; the final Manifest
commit is recorded by the branch history and final handoff. Commits are
`e12b766` (implementation), `2d0322d` (model/parity), `74fe051` (strict host
closure) and `4e16cd0` (software evidence).

## Candidate

The D1-D build integrates signed direction evidence into DisplayConditioner
instead of running a second panel follower. It preserves TRACKING, CANDIDATE,
the nine-sample median, LOCKED, stability hold and operator-zero behavior.
LOCKED now holds adjacent noise within 1d, follows persistent 2d-8d bias by
one division per five unique stable samples, and releases greater-than-8d
steps in the same sample. Exactly 8d follows the slow path; 8d+1 count uses
the immediate path. Repeated 20 ms calls with the same measurement sequence
cannot add evidence.

The source domain includes NET/GROSS, unit, decimal places, division digit and
R5 application path. g, kg, lb, 1/2/5 divisions, positive/negative values,
sequence wrap, invalid conversion, operator ZERO and source changes are in the
C/Python tests. R5 OFF, SHADOW, ACTIVE+STATIC and ACTIVE+DOSING share the same
display semantics while selecting the already-formal operational mass.

## Opened Replay

All replay inputs are **DEVELOPMENT / OPENED REGRESSION**, not a new holdout.
Seventeen representative datasets cover R5D 12 h and unload, R5E, D1 cycles,
D1-B, Stage 5L static/load/unload/slow fill, Stage 5N cycles, pauses,
disturbance, TARE/CLEAR TARE and Stage 5N-B.

D1-B's 147-record failure trace no longer waits 146 seconds: first correct
change occurs at 971 ms in the approximately 1 Hz host trace, final error is
1d, wrong-direction updates are 0 and slow updates are at most 1d. This real
delay is reported separately from the ideal 10 Hz gate. The ideal five-sample
case completes a 1d move within 500 ms and the software limit is 600 ms.

Across the opened replays, the unified candidate has zero wrong-direction
updates, zero LOCKED 10-second A-B-A, maximum LOCKED slow jump 1d and zero
same-sample large-step loss. On R5D 12 h, unified error is max/P50/P95/P99
3/1/2/2d versus the original hold's 8/3/6/7d; the longest interval above 2d
drops from 3,185,231 ms to 3,006 ms. A development synthetic comparison shows
two A-B-A returns for the no-1d-deadzone D1-C behavior and zero for D1-D,
supporting the 1d micro-noise zone.

Python/C comparison covers 49,533 records and every required state field with
0 mismatch. Frozen D1-C remains 49,105/0, R5 25,057/0 and Stage 5N-A
45,711/0.

## Software And Resources

MSVC `/W4 /WX` builds clean and Host CTest passes 33/33. ARM
`-Wall -Wextra -Werror` passes. Stage 5B is 30/30, Stage 5C 12/12, Stage 5L
8/8 and G2 8/8. R2/R3/R4/R5, menu/PLC concurrency, V3 persistence, Modbus and
display regressions pass. ASan and UBSan are **NOT RUN** because the required
runtime remains unavailable.

The 0x0517 Beta uses 19,448 B linker RAM, 24 B less than 0x0516. Static RAM to
stack top is 2,056 B. The largest main chain is 1,144 B and IRQ chain 72 B;
with the existing exception and indirect-call allowance, conservative stack is
1,504 B and collision margin 552 B, above both the 512 B hard gate and 528 B
baseline. DisplayConditioner_Update has a 72 B local frame. The algorithm is
bounded O(1) except for the fixed nine-entry insertion sort when locking. It
uses no dynamic allocation, recursion, floating point or history growth.

## Firmware Identity

- Firmware: 0x0517 Engineering Beta
- Map: 0x0104
- Schema: 2
- Persistent Format: 3
- BIN: 107,084 B, SHA-256
  `9D8C5881CD880D2C5D6A7D0C499A4EACF9495A550B08B237A81D4BB2F71641C5`
- ELF: SHA-256
  `DD1549D2AC80C023F2B3524AE4A742203072532CF4523F6A09B6891A15DC77BF`
- map: SHA-256
  `5C0F376997FB4538C6F987824DCBDF852CB580DBA7FA498B6F6202D98C41117B`
- Release remains
  `82E726F5B32A0DE36A5E686F62A937EC4FD9CBB488DB9733E83D2062673EF486`
- 0x0516 rollback image remains
  `759AF431C2F7379296817419090DA305C55D18110A6E1733312B8C5388BF98FE`
- Application pages: 0-104; configuration remains 0x0801F000-0x0801FFFF.

## Hardware Qualification

The authorized session backed up the application and configuration over SWD,
verified V3 slot A sequence 7 and active slot B sequence 8, and erased only
application pages 0-104. 0x0517 programming and Verify passed. Configuration
SHA was identical before, after flash and at final cleanup:
`A615A475C2D7B5D64126EAEB3AAE9E3D094348FA4C4AEC7BDB8EF0010D4A3BB4`.

OFF + SHADOW hardware records covered 60 s empty, a 500 g large step, three
complete 500 g load/unload cycles, mechanical disturbance, NET/GROSS page
change and TARE/CLEAR TARE. The panel released large steps in the same sample
and remained within one display division at stable endpoints.

R5 SHADOW reference establishment ran 960 s with 960 records, no read errors,
and reached TRACKING. ACTIVE + STATIC then ran 1,200 s with 1,200 records and
naturally reached offset `0.025288 g`; no diagnostic stimulus or tuning was
used. The display stayed within one division of corrected authoritative
weight, with no rebase, fault, overrun, dirty or SAVE change.

ACTIVE + DOSING froze offset at `0.025977 g`. Three valid load/unload cycles
were completed. Two earlier windows where the user's confirmation arrived
after a fixed recording window are retained as incomplete and explicitly
excluded from PASS evidence. The valid cycles used a stop-request tail so the
physical event and stable tail are both inside the same original CSV.

The host tool was extended to accept an explicit expected firmware identity;
the default remains 0x0516. This is a host evidence correction only and does
not alter firmware behavior.

The current sensor engineering qualification is complete. Cross-sensor,
metrology certification, 40 Hz, physical sensor-fault qualification, ASan,
UBSan and Stage 5O remain deferred. The final state is ready for normal
engineering use; formal Release qualification remains separate.
