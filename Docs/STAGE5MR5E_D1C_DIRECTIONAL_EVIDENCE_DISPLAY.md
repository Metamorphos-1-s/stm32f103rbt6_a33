# Stage 5M-R5E-D1-C Directional-Evidence Display

## Result

**STAGE 5M-R5E-D1-C DIRECTIONAL DISPLAY CANDIDATE SOFTWARE READY; FIRMWARE
0x0514 ARTIFACT BUILT; NONZERO-OFFSET HARDWARE QUALIFICATION PENDING; R5
ALGORITHM UNCHANGED; STAGE 5N ENTRY DEFERRED.**

The artifact has not been flashed. The new hardware holdout has not been
opened. Flashing requires the exact explicit authorization defined by the
stage contract and will reset all volatile R5 state.

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

The 0x0514 Beta uses Map 0x0104, Schema 2 and Persistent Format 3:

- BIN: 102,280 bytes, SHA-256
  `E87759986372207FBDC915F67F03117B701D46FE79AC0C93E7751A085AB894F1`
- ELF: 1,966,864 bytes, SHA-256
  `6C930C78AAE7B0D34C3DF2356A8770FD3F501B32D0AF46424A2E3D476F515CB6`

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

A read-only probe at 2026-09-20T15:25:43.341Z confirmed the device still runs
0x0512, Map 0x0104, OFF + SHADOW, offset/reference/evaluation = 0,
revision/saved = 8/8 and fault/overrun/dirty/SAVE = 0. The configuration SHA
last verified at D1-B rollback is
`A615A475C2D7B5D64126EAEB3AAE9E3D094348FA4C4AEC7BDB8EF0010D4A3BB4`;
it has not yet been reread for D1-C because no flash is authorized.

After explicit authorization, configuration must be backed up and hashed,
0x0512 backed up, only application sectors written and verified, and the
configuration reread. A fresh recorder captures unique device samples from the
start and rejects measurement/follower sequence mismatch. Natural
`abs(offset) >= 0.020 g` and at least 2d authoritative movement are required
before the slow holdout can pass. DOSING 500 g load/unload follows only after
that gate. Stage 5N-A is not authorized by this result.
