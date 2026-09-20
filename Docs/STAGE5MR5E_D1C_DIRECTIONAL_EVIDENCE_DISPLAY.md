# Stage 5M-R5E-D1-C Directional-Evidence Display

## Result

**STAGE 5M-R5E-D1-C DIRECTIONAL DISPLAY CANDIDATE SOFTWARE READY; FIRMWARE
0x0514 V4 FLASHED; NONZERO-OFFSET HARDWARE QUALIFICATION PENDING; R5
ALGORITHM UNCHANGED; STAGE 5N ENTRY DEFERRED.**

The original v1 artifact was application-only flashed and device-verified after
explicit authorization. Its configuration SHA remained unchanged. The new
hardware holdout has not been opened: the first OFF + SHADOW recorder preflight
correctly rejected cross-frame sequence skew. A compact, single-frame v2
engineering diagnostic refresh was flashed after renewed confirmation, but its
51-register response perturbed the main-loop sample timing. The 34-register v3
was flashed after confirmation, but STATIC capture still covered only 586 of
599 device sequences. A 24-register v4 was subsequently authorized, flashed
and verified. All artifacts contain the same frozen display
algorithm and parameters.

The branch started from
`549fd4c4a0e46ca9817f17716393f695566c2d7b` on
`stage5mr5e-d1b-display-candidate`. D1-C work is on
`stage5mr5e-d1c-directional-evidence-display`; final code, evidence and
manifest commits are reported by Git at closeout.

## Failure contract and data classification

The D1-B 147-record capture is DEVELOPMENT / OPENED REGRESSION. Its raw CSV
SHA-256 is
`2EFD5FEBBBB259CAE3DC5B271558F2FB32D03635DA57E4BD44616D267F480FA6`.
It is used for search and regression, never as a new holdout. The rejected
0x0513 exact-count algorithm repeatedly restarted its one-second timer as the
target moved among adjacent counts, even though direction stayed negative.
The complete immutable facts and rollback evidence are in
`STAGE5MR5E_D1B_FAILURE_CONTRACT.md`.

## Candidate and ranking

The bounded search evaluated all 216 combinations from the defined parameter
grid; 63 passed. Ranking selected the least complex passing candidate:

- same-direction increment: 1 per unique 10 Hz sample
- zero-direction leak: 1
- reverse-direction cancellation: 1
- trigger threshold: 5
- trigger action: move 1d and clear evidence
- large step: strictly `abs(delta) > 8d`, immediate authoritative release

Evidence is signed. Adjacent target counts on the same side of the panel add
evidence instead of resetting it. Opposite direction first cancels old
evidence, and zero delta leaks toward zero. A repeated 20 ms display-task call
with the same measurement sequence cannot add evidence. The product adapter
normalizes current UnitConverter output into division indices, so a 1d update
uses the configured unit, decimal places and division digit rather than a
hard-coded 0.01 g. Page/unit/decimals/division changes reset the source.

An invalid conversion contract issue was found before hardware freeze: invalid
input now leaves the follower uninitialized, forcing the next valid sample to
re-establish the normalized domain. Parameters and all scored outputs remained
unchanged after the correction and full rerun.

## Offline gates

On the D1-B capture, the panel updated nine times. First correct update was at
4,978 ms, final panel and target were both -9 counts, wrong-direction updates
were zero and maximum slow jump was 1d. Thus adjacent-count noise no longer
clears persistent directional evidence.

The 60-second alternating boundary test produced zero panel changes, zero
A-B-A returns, zero peak-to-peak panel movement and zero transition violations.
Positive and negative slow drift both had no record above 1d stale, no >2d/5s
interval, maximum 1d update and final error at most 1d. A real direction
reversal began moving in the new direction after 400 ms with zero old-direction
extra movement. Synthetic 500 g load and unload both released in the same
sample with zero step loss and no division-by-division chase.

All historical R5D, R5E and D1-A runs had zero invalid display transition:
small transitions were at most 1d and in the instantaneous target direction;
large transitions exactly released to the target. OFF and SHADOW preserve the
existing baseline output sample by sample.

## Fixed-point and resources

Python/C comparison covers 49,105 samples, including the prior 48,120 corpus,
the 147 D1-B records, boundary noise, direction reversal, signed INT32 limits
and repeated scheduler calls. Mismatches are zero across desired/current/delta,
direction, evidence, anchor, lock/stable/active, large-step/reason/source and
unit/decimal/application/mode context. Frozen R5 comparison remains 25,057
samples with zero mismatch.

The state is 12 bytes: display count 4, last sample sequence 4, signed evidence
2, flags 1 and source 1. Beta linker RAM is 19,448 bytes versus 19,432 for
0x0512, a 16-byte increase. `.data` is 12 bytes and `.bss` is 18,408 bytes.
Display build local stack rises from 104 to 120 bytes, exactly the 16-byte
gate; the main maximum chain remains 1,128 bytes. Conservative stack is 1,488
bytes and collision margin is 568 bytes, above the 512-byte minimum. The
follower is loop-free, allocation-free and division-free; its 338 bytes of
Thumb code have a conservative bound below 20 us at 72 MHz.

## Firmware and frozen paths

The authorized 0x0514 v1 Beta uses Map 0x0104, Schema 2 and Persistent Format 3:

- BIN: 102,280 bytes, SHA-256
  `E87759986372207FBDC915F67F03117B701D46FE79AC0C93E7751A085AB894F1`
- ELF: 1,966,864 bytes, SHA-256
  `6C930C78AAE7B0D34C3DF2356A8770FD3F501B32D0AF46424A2E3D476F515CB6`

The compact-diagnostic v2 artifact is 102,768 bytes with SHA-256
`53461D36C78447811D9530739A51AE2710E392ACDED825CF2C0082C2265218B8`;
its ELF is 1,972,108 bytes with SHA-256
`189CBBFA5C961545E8A3BFCF4EB77012915C88E9BC988AB97D8AA439B1B8917A`.
The only v2 firmware change is a packed, read-only engineering snapshot for
one-frame 10 Hz evidence capture. It does not change algorithm, parameters,
RAM, authoritative paths or public Map 0x0104.

The v2 recorder covered sequence numbers 100/100, but sample timestamps exposed
0, 176, 178 and 200 ms intervals: its response was still long enough to block
main-loop measurement processing. This is not an acceptable true-10-Hz
holdout. The v3 response is reduced to 34 registers by encoding five mass
fields as signed int32 values at 100 ug resolution, ten times finer than the
0.001 g step-loss gate. Fault/overrun and revision counters retain sufficient
qualification range. The v3 BIN is 102,728 bytes, SHA-256
`EDD903D6B7194211F2C5040ADF0C9B22A243330E57AED2DF40BE57E9477C0E4D`;
its ELF is 1,971,912 bytes, SHA-256
`F3B6E4C4E99F608C47FE52B345B65EE5BE3B1AE5F3B24510FBFC0BBE353CF2B1`.

The v4 block removes transmitted delta and anchor because both are exactly
derived from desired/display, and packs qualification counters with saturation:
any nonzero fault, overrun, SAVE or rebase remains a hard failure. It is 24
registers, while preserving 100 ug mass resolution. The v4 BIN is 102,656
bytes, SHA-256
`65B9CE4D6D9383DAC6F05E842BFA24C38BB2F246EA1770DA7ABB5B6C7C028877`;
its ELF is 1,971,572 bytes, SHA-256
`C9CA064F36A602425DA10126D4F0D33622024AC302AEC2834CADEA67CEBF430A`.

Standard Release remains byte-identical at
`82E726F5B32A0DE36A5E686F62A937EC4FD9CBB488DB9733E83D2062673EF486`.
The frozen R5 C/header/Python Git blobs remain
`a4c88335be16d288d8d1bbe91bbfe7e32e4aea04`,
`7a2b1b4ff9776a4f60891a2f74973b7c09bd9bfe` and
`5e4b9335c0d87f4dc179251c5275c6b9882c864f`.

Only the ACTIVE Beta panel output and engineering-only diagnostic range are
added. Authoritative gross/net, PLC/Modbus/BLE weight registers, R5 input and
algorithm, formal stable, alarms, calibration and persistent format are
unchanged. Debug, Release and Beta builds are warning-clean; Host CTest is
28/28 and all listed Python regressions pass. ASan and UBSan are NOT RUN.

## Device and next gate

A preflash read confirmed the on-device 0x0512 application hash
`BA8F02B2024042D601FD7F2D75BEF9E1004AACAE16852DD97CD2B28777BAF6B9`.
The v1 application download erased only sectors 0-99 and device Verify passed.
The postflash device runs 0x0514, Map 0x0104, OFF + SHADOW,
offset/reference/evaluation = 0, revision/saved = 8/8 and
fault/overrun/dirty/SAVE = 0. Configuration SHA before and after flash is
`A615A475C2D7B5D64126EAEB3AAE9E3D094348FA4C4AEC7BDB8EF0010D4A3BB4`.

The multi-frame recorder produced zero accepted records and three explicit
sequence-alignment errors; this raw failed preflight is retained. Timing showed
that a 21-register D1-C-only block captures every device sample (50/50 over
five seconds), while the complete multi-block transaction takes about 250 ms.
The v2 51-register block packs all required display, R5 and safety evidence
into one coherent response so it can be validated at the real 10 Hz rate.

The v4 application-only download erased sectors 0-100 and device Verify passed.
Configuration SHA remained
`A615A475C2D7B5D64126EAEB3AAE9E3D094348FA4C4AEC7BDB8EF0010D4A3BB4`.
OFF preflight covered 300/300 sequences and SHADOW+STATIC covered 600/600.
The 900.062-second reference build covered 8,984/8,984 sequences, average
device interval 100.184 ms, maximum 139 ms, and ended in TRACKING.

The fresh ACTIVE holdout captured 18,560/18,560 consecutive sequences over
1,859.288 seconds. Desired display naturally spanned 9d. The candidate made
897 valid 1d updates, with zero wrong-direction update, zero invalid
transition, zero A-B-A return in any 10-second window and at most 902 ms above
2d lag. Fault, overrun, dirty, SAVE and automatic rebase remained zero;
revision/saved remained 8/8. This is a display and safety pass on the observed
low-offset data.

Maximum natural offset was only 2,200 ug, below the required 20,000 ug, so the
strict nonzero-offset trigger did not occur. The 500 g DOSING step was not run.
After evidence capture, ACTIVE was changed to SHADOW, STATIC to OFF, and an MCU
reset cleared evaluation count. Final state is OFF + SHADOW with
offset/reference/evaluation = 0 and unchanged configuration. Stage 5N-A is not
authorized by this result.
