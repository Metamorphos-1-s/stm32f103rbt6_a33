# Stage 5L-R Rate and Evidence Recovery

Status: `STAGE 5L-R EVIDENCE REPAIRED; 40 HZ ROOT CAUSE PENDING`

## Baselines

- STM32 start: `4ec6415fa6f017d77d319d95a0fe729e7ca3658a`.
- Stage 5K freeze ancestor: `61ab01330c19ec2f01f7af0f3723ce055ce931a8`.
- Client start: `c8b8f585203ea94ab4f0778713ef28582ad39ccc`.
- CH579 audit-only start: `eb888925e4fcc9dcd9bf89e8cc42e5b28679e520`.

## Evidence portability

The Git-blob audit found 0/44 historical run Manifests valid. All 220 bound files reproduce the historical expected hash exactly after LF-to-CRLF conversion; no non-EOL content difference exists. The old aggregate Manifest is 3/10 valid and 7/10 invalid for the same reason.

Capture writers now emit UTF-8 with an explicit final LF, JSONL uses explicit LF bytes, CSV uses `lineterminator="\n"`, and `.gitattributes` fixes Stage 5L text to LF while keeping slot binaries `-text`. Historical manifests and measurements are unchanged.

Commit `bba48dfdfb0649b8cafe5f6788854ddd9d034b35` contains 44 repository-byte V2 manifests. All 44 validate against that commit's Git blobs. They are explicitly `RECOVERED ENGINEERING EVIDENCE`, not newly executed hardware tests.

The aggregate binding is `Docs/evidence/STAGE5LR_REPOSITORY_MANIFEST_V2.json`. It supersedes the stale `evidence_head_before_final_report` as a repository-byte repair layer without modifying the historical Stage 5L Manifest.

## Client repair

The client baseline had 0/8 exact source hashes and 8/8 EOL-only differences. The new branch binds all eight hashes to LF Git blobs, normalizes text hashing across CRLF/LF checkouts, and adds explicit 0x0510 evidence attributes. Isolated clones with `core.autocrlf=false`, `input`, and `true` all load the trusted baseline successfully.

Active hashes remain `91D346E87BD112EFAC3B513A8CAFBBDDE9642069A15280DB7565374378BA43E1` and `F596F7460C911607FA8328A6D4BA5725EA5A0E5B14765D61EE7889A0D62F48A4`.

## Contract correction

Map stays `0x0104`. Client metadata now matches firmware:

- `status_flags`: fixed low-word-first.
- `display_weight`, net/gross/tare display, raw, filtered raw, fault mask: configurable word order except the fixed status field.
- `cs1237_overrun`: uint32 at `0x002D..0x002E`.
- sample sequence/timestamp, current/saved revision and storage sequence: configurable word order.

Asymmetric signed and unsigned vectors test both configured word orders.

## Temporary product boundary

Profile 0 / 10 Hz is the only currently allowed product path. Profile 1 / 40 Hz is engineering-diagnostic only. The Stage5B tool rejects it without `--allow-engineering-40hz` and explicit `ENGINEERING_40HZ` authorization. Profiles 2/3 (640/1280 Hz) are always rejected. The frozen `0x0510` Release behavior is not rewritten.

## Software gates

- Stage5B Python: 3/3 PASS.
- Stage5C Python: 12/12 PASS.
- Stage5L Python: 8/8 PASS.
- MSVC `/W4 /WX` Host CTest: 17/17 PASS.
- ARM Debug, Release, BoardDiagnostics and Stage5LDiagnostics clean builds: PASS.
- Register-map/source consistency: PASS.
- Release SHA-256 remains `82E726F5B32A0DE36A5E686F62A937EC4FD9CBB488DB9733E83D2062673EF486` and contains no Stage5L symbols.
- Clang strict, ASan and UBSan: UNAVAILABLE in the current Windows environment; WSL service is unavailable.

No Stage 5M/5N/5O production algorithm is implemented.

## Hardware G1 progress

Cold-start control 1 is preserved under `Results/stage5lr_hardware/20260914_control1_10hz_cold_60m`. Its capture Manifest passes and binds 31,353 records over 3,600.11 seconds. No 40 Hz switch or device write occurred; environment temperature is explicitly `UNMEASURED`.

The measured 10 Hz processed rate is 9.98220 samples/s. Across the required 0-5, 5-10, 10-20, 20-30 and 30-60 minute windows, rates are 9.98260, 9.98293, 9.98243, 9.98273 and 9.98175 samples/s. Whole-run raw mean/stddev/peak-to-peak are -43,781.45 / 15.23 / 118 counts, with raw first/last -43,795 / -43,780 and a fitted drift of -20.91 counts/hour. Stable ratio is 100%; overrun delta and mapped fault observations are zero; the display remains -22 counts.

The post-run read-only probe confirms Firmware 0x0510, Map 0x0104, Schema 2, Persistent Format 3, Profile 0, storage sequence 7 and `dirty=0`. The supplemental environment and segment record is `Results/stage5lr_hardware/20260914_control1_supplement.json`.

Cold-start control 2 is preserved under `Results/stage5lr_hardware/20260914_control2_10hz_cold_60m`; its capture Manifest also passes. It contains 31,471 records over 3,600.12 seconds and measures 9.98241 processed samples/s, with zero overrun and no mapped fault. Whole-run raw first/last are -43,813 / -43,892 and the fitted drift is -149.21 counts/hour. The 30-60 minute raw mean is -43,871.19, 83.26 counts below control 1; the corresponding net-mass means differ by 0.093795 g. A short non-near-rail raw excursion around 1,611 seconds has a maximum adjacent jump of 92 counts.

The required two controls are complete, but their late-run baseline and fitted drift are materially different. Temperature was unmeasured in both. The comparison is `Results/stage5lr_hardware/20260914_g1_cold_start_comparison.json`; it explicitly rejects a repeatable cold-start envelope and algorithm compensation readiness. DRDY/SCLK and synchronized diagnostic evidence remain unavailable, so 40 Hz root cause is still pending and no new 40 Hz switch is authorized.
