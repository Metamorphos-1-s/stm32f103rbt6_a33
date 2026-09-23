# Stage 5P-A1B R5 Profile-Switch Audit

## Scope And Baseline

The branch starts from `87dc8310bfda11f5a45a55ce2a9e538b2b5125e3`.
Stage 5P-A1 already closed the Modbus throughput root cause; this stage does
not change or reopen the Modbus view, R5 parameters, display conditioner,
Checkweigh, persistence, or any product source.

The candidate rebuild is byte-identical: BIN 107,532 bytes,
`2101D1F344B63AA9A6983B56A90EBFF3AF8167B838B506611776D71C37D3C6AA`;
ELF 2,024,160 bytes,
`A2832B10FF9F815EACE61E7A13FE63E09987EA912EB781E6012ED55F338A72DE`.

## Controlled Reconfiguration Semantics

The successful transaction order is:

`CS1237 reconfigure -> WeightEngine rebuild/replay -> R5 profile event -> first
new hardware sample`.

`MetrologyManager_Reconfigure()` emits the event only after the rebuild
succeeds. Replayed raw input is consumed only by WeightEngine and is not passed
to R5, so the event cannot occur after a new-profile R5 sample.

The product profile event preserves mode, manager-owned application, offset,
automatic-rebase count, and evaluation count. It clears the previous sequence
and timestamp baseline, incomplete second bucket, 100 ms slot, reference and
observation input, and step-confirmation state. STATIC restarts the 15-second
holdoff. DOSING remains DOSING without learning. OFF remains OFF with its
existing zero-offset invariant.

This is a one-sample rebaseline, not a continuity-check relaxation. Once the
first post-event sample establishes sequence/time, the next sample is checked
against the unchanged `delta sequence == 1`, nonzero timestamp delta, and
`delta timestamp <= 250 ms` rules.

## Safety Tests

Host C covers 10/40 changes, all filter transition semantics through repeated
profile events, simultaneous rate/filter rebuild semantics, profile 0/1/0,
OFF, STATIC and DOSING, bucket start/middle/end, timestamp wrap, positive and
negative nonzero offset, corrected/uncompensated relation, and repeated events.

Without a profile event it injects skipped, large-jump, duplicate and backward
sequences, plus equal, backward and greater-than-250 ms timestamps. It also
injects a real gap after one or multiple legal events. All enter the expected
SEQUENCE or TIMESTAMP LIMITED state. Python tests independently cover the
one-sample rebaseline and positive/negative offset contracts.

## Software Gates

- Host MSVC `/W4 /WX`: 36/36 PASS.
- Python reference-lock tests: 10/10 PASS.
- Legacy R5 parity: 25,057 total, 22,557 real, zero mismatch.
- Product R5 sample parity: 95,200 synthetic, zero mismatch.
- D1-D parity: 49,533, zero mismatch.
- Stage 5N-A parity: 45,711, zero mismatch.
- Stage 5B/5C/5L Python: 3/3, 12/12, 8/8 PASS.
- Debug, Release, candidate and strict ARM builds: PASS.
- Candidate product RAM: 19,464 bytes.
- Conservative stack: 1,504 bytes; collision margin: 536 bytes.
- Frozen 0x0517 SHA remains
  `9D8C5881CD880D2C5D6A7D0C499A4EACF9495A550B08B237A81D4BB2F71641C5`.

## Hardware Execution

Modbus-only preflight confirms the device remains 0x0517, Map 0x0104,
10 Hz/filt3, OFF+SHADOW, offset/reference/evaluation zero, fault/overrun/dirty
zero, revision/saved 8/8, and approximately 500 g loaded.

The authorized hardware closure completed without changing the approximately
500 g load. The 4 KB region at `0x0801F000` was backed up and remained unchanged.
The 107,532-byte candidate occupied application pages 0-105
(`0x08000000-0x0801A7FF`); configuration pages were excluded. Candidate and
0x0517 rollback Verify both passed. No SAVE/ZERO/TARE/calibration action was
performed. Detailed results are in
`Docs/STAGE5PA1B_R5_PROFILE_SWITCH_HARDWARE_CLOSURE.md`.
