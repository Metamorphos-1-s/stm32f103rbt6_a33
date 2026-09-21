# Stage 5N-A3 Checkweigh Missing Gates

## Current Result

**STAGE 5N-A SHADOW TARGET QUALIFICATION COMPLETE; STAGE 5N-B DESIGN ENTRY
APPROVED; ACTIVE ALARM OUTPUT REMAINS BLOCKED UNTIL STAGE 5N-B; PHYSICAL
SENSOR-FAULT QUALIFICATION DEFERRED; D1-C NONZERO-OFFSET QUALIFICATION
DEFERRED; 40 HZ REMAINS BLOCKED; STAGE 5O REMAINS DEFERRED.**

**WARNING-CLEAN ACTIVE BUILD GATE REMAINS OPEN.**

This report covers P0 through P3. P3 proceeded only after the user explicitly
authorized configuration backup, diagnostic programming and mandatory 0x0515
recovery. The qualification is target-side INVALID_INPUT coverage, not a claim
for physical sensor disconnection, short circuit or reference failure.

The stage started from
`8c68acd4cfc3094f5f494b9aa918e8263e9a364b` on the A2 branch and continues
on `stage5na3-checkweigh-missing-gates`. The existing 15 pyc changes and four
manual snapshots remain outside all commits.

## P0 Audit

The repository, upstream, A/A2 reports and Manifests matched the required
baseline. The live device was the original frozen 0x0515 Beta, Map 0x0104,
Schema 2, Persistent Format 3, Profile 0 at 10 Hz and filt3/strength3. It was
empty and stable, R5 OFF + SHADOW, with offset/reference/evaluation/rebase zero,
fault/overrun/dirty/SAVE zero, revision/saved 8/8 and every formal output zero.
No SWD operation, reset, SAVE or Flash write occurred in P0/P1.

The candidate executes only from `MetrologyManager_AcceptRawSample` after a
new WeightSnapshot. It checks sequence and sample timestamp continuity between
calls, but it has no independent periodic data-age evaluation when samples stop.
Therefore stopping sample delivery cannot validate a frozen stale timeout.

## P1 Monotonic Slow Fill

A fixed lightweight container started at 98.744588 g in LOW. The user added
water only, did not move the container and reported no unload. The run lasted
760.176 s, ended at 410.626512 g in HIGH and gained 311.881924 g. Its whole-run
average was 24.617 g/min, substantially slower than prior step loading.

The only DYNAMIC transitions were LOW to OK at sequence 206671 and OK to HIGH
at sequence 212726. Candidate and confirmed state changed in the same observed
sample; adjacent observed timestamp bounds were 76 ms and 189 ms, below the
600 ms gate. The initial stable range was 45,053 ug. Five-second robust medians
had maximum drawdown 38,858 ug and no reverse window over 100 mg, so the small
decreases remain inside the measured noise envelope rather than evidence of a
physical unload.

Across 4,631 records, DYNAMIC hysteresis violations, STATIC contract errors,
formal output activations and read errors were zero. All 2,233 unstable records
kept STATIC PENDING. Maximum host gap was 391 ms. Fault, overrun, dirty and SAVE
were zero, revision/saved stayed 8/8, and R5 offset/reference/rebase stayed zero.
P1 therefore passes without changing any candidate parameter.

## P2 Diagnostic Design

The selected minimum injection is target-side `INVALID_INPUT`, not stale and
not a physical sensor-disconnect claim. Under the A3-only compile macro, the
integration gates the naturally computed `CheckweighShadowInput.valid` after
the real WeightSnapshot and fast calibrated weight are prepared, immediately
before the unchanged `CheckweighShadow_Process` call. WeightEngine sampling,
R5, the main loop and Modbus continue. The diagnostic module cannot write the
candidate state or formal outputs. It adds no public Modbus or BLE command.

The SWD RAM control block is 92 bytes with magic `0x354E4133`, version 1,
request/applied sequences, command magic, status, injection type, start and
remaining time, completion reason and observed candidate fields. INVALID_INPUT
is bounded to 100-5,000 ms. Invalid commands and durations are rejected,
repeated sequences are idempotent, ABORT restores immediately, timeout restores
automatically and reset clears the entire control state. The P3 tool can write
the request, disconnect for the full duration, and reconnect only after expiry
to prove autonomous target recovery.

The first diagnostic build used 19,576 bytes RAM and was rejected because its
440-byte conservative collision margin was below the 512-byte gate. The final
diagnostic-only build reduces the BLE command response cache from four entries
to one; BLE is not a P3 qualification channel. Product Release and original
Beta retain four entries and identical deployable bytes. Final diagnostic RAM
is 18,848 bytes. Static end to stack top is 2,656 bytes; against the frozen
1,488-byte conservative stack bound the margin is 1,168 bytes. Dedicated stack
usage output shows unchanged 40-byte App_Run and 136-byte alarm wrapper frames;
new diagnostic functions are at most 16 bytes, so the global main chain does
not increase.

## Artifacts And Regression

The diagnostic application BIN is 105,808 bytes, SHA-256
`6FE3181EC275A122C8A7CABF4918AA4665EC028CB0C26E697A9C011487AC2B6C`.
Its ELF is 1,995,364 bytes, SHA-256
`97CCD708500EC930168FAF878DD288937DF6AD038750A527090B3738866AB485`.
The control symbol is at `0x20004284`. The diagnostic image covers application
pages 0-103.

The frozen recovery BIN remains 105,416 bytes, SHA-256
`62648C3AD79C324B1860BE3005B8EA365C2BCE98C084BE4F5FD20D7599065228`
and covers pages 0-102. Recovery must erase diagnostic page 103 as well before
programming the frozen BIN. The standard Release ELF remains 172,200 bytes,
SHA-256 `82E726F5B32A0DE36A5E686F62A937EC4FD9CBB488DB9733E83D2062673EF486`.
Release and original Beta contain no A3 control symbol.

Host CTest passed 29/29. Stage 5N-A Python passed 20/20; Stage 5N-A parity is
45,711/0, R5 parity 25,057/0 and D1-C parity 49,105/0. Stage 5B is 30/30,
Stage 5C 12/12, Stage 5L 8/8 and Stage 5L-R G2 8/8. Debug, Release, original
Beta, A3 diagnostics and the A3 ARM `-Wextra -Werror` build pass. The new A3
host target also passes MSVC `/W4 /WX`.

Full strict MSVC still fails only on the two frozen A2 debts: C4310 at
`directional_display_follower.c:118` and C4127 at
`test_stage5mr5_reference_lock.c:119`. Neither involves A3 code. Status remains
**KNOWN BUILD-HYGIENE DEBT; MUST CLOSE BEFORE ACTIVE STAGE 5N-B ARTIFACT**.
ASan and UBSan are NOT RUN.

The preauthorization evidence Manifest passed file length, SHA-256 and Git-blob
verification in independent `core.autocrlf=false`, `input` and `true` clones.
The final read-only device snapshot at 2026-09-21 14:06:52 UTC still shows the
original 0x0515, empty and stable, OFF + SHADOW, offset/reference/evaluation
zero, clean revision 8/8, fault/overrun/SAVE zero and formal outputs zero.

## P3 Target Validation And Recovery

The preflash application readback was byte-identical to the frozen 0x0515 BIN.
The 4,096-byte configuration region SHA-256 was
`A615A475C2D7B5D64126EAEB3AAE9E3D094348FA4C4AEC7BDB8EF0010D4A3BB4`.
Both V3 slots were valid: A sequence 7 and active B sequence 8. The diagnostic
ELF erased and programmed application pages 0-103 and passed device Verify.

At stable 500 g HIGH, a 2,000 ms INVALID_INPUT command was accepted. The SWD
host then remained disconnected for the complete injection interval. The MCU
injected 20 samples and autonomously completed with TIMEOUT. Modbus continued
to advance during the interval and recorded 12 suppression observations. The
recovered candidate returned to HIGH without physical action.

A second 5,000 ms command captured the active target control block with
`last_input_valid=0`, `last_static_class=INVALID` and
`last_dynamic_class=INVALID`. ABORT was then accepted and the next captured
state had valid input and both outputs HIGH. The frozen engineering Modbus block
retains the internal DYNAMIC hysteresis state in its `dynamic_confirmed` field
during suppression, while `dynamic_reason=INPUT`; the A3 control block observes
the actual current `CheckweighShadowOutput.dynamic_confirmed`, which was
INVALID. No candidate state was directly overwritten.

Both runs had zero formal alarm/lamp/buzzer output, read error, fault, overrun,
dirty or SAVE activity. Revision/saved remained 8/8 and R5 offset/reference/
rebase remained zero. Main-loop and sample sequences continued advancing.

Recovery explicitly erased application pages 0-103, programmed the frozen
0x0515 BIN into pages 0-102 and passed Verify. Application readback is
byte-identical at SHA-256
`62648C3AD79C324B1860BE3005B8EA365C2BCE98C084BE4F5FD20D7599065228`;
page 103 is all `0xFF`, so no diagnostic tail remains. Configuration before and
after is byte-identical at the SHA above and both V3 slots remain valid 7/8.

The final device is empty and stable on original 0x0515, Map 0x0104, 10 Hz,
filt3/strength3, R5 OFF + SHADOW, offset/reference/evaluation/rebase zero,
fault/overrun/dirty/SAVE zero, revision/saved 8/8 and formal outputs zero.
No SAVE was sent and configuration pages were never erased.

Physical sensor disconnection, short circuit, excitation/reference faults,
cross-sensor validation, D1-C nonzero-offset qualification, 40 Hz external
timing, metrology certification, ASan, UBSan and Stage 5O remain deferred.
