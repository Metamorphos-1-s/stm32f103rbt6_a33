# Stage 5L-R Remote and Evidence Audit

Audit date: 2026-09-13

## Repository baselines

| Repository | Remote | Branch | Start and remote HEAD |
|---|---|---|---|
| STM32 | `https://github.com/Metamorphos-1-s/stm32f103rbt6_a33.git` | `stage5l-measurement-characterization` | `4ec6415fa6f017d77d319d95a0fe729e7ca3658a` |
| Client | `https://github.com/Metamorphos-1-s/a33-instrument-clients.git` | `pc-stage2c-fw0510-stage5k-validation` | `c8b8f585203ea94ab4f0778713ef28582ad39ccc` |
| CH579 | `https://github.com/Metamorphos-1-s/CH579M_TCP_UART.git` | `stage4d-fw0510-peer-validation` | `eb888925e4fcc9dcd9bf89e8cc42e5b28679e520` |

All worktrees were clean, local/remote counts were 0/0, and Stage 5K freeze `61ab01330c19ec2f01f7af0f3723ce055ce931a8` is an ancestor of the STM32 Stage 5L start. CH579 is audit-only and no branch or empty commit is created.

## Stage 5L run Manifests

The audit reads Git blobs from `4ec6415fa6f017d77d319d95a0fe729e7ca3658a`, not platform-dependent working-tree bytes. Detailed per-file Manifest length/hash, Git length/hash, reconstructed CRLF length/hash, and classification are preserved in `Results/stage5lr_audit/stage5l_manifest_git_blob_audit.json`.

- Run Manifests: 44.
- Exact Git-byte PASS: 0.
- Exact Git-byte FAIL: 44.
- Bound files: 220.
- Files reproduced exactly by LF-to-CRLF conversion: 220.
- Non-EOL content differences: 0.

The historical manifests were generated over Windows CRLF bytes. Git stored LF blobs, so clean checkout evidence fails even though decoded measurement content is unchanged.

## Stage 5L aggregate Manifest

| Result | Path |
|---|---|
| FAIL | `20260912T192400Z_empty_10m/manifest.json` |
| FAIL | `20260912T193600Z_empty_hot_60m/manifest.json` |
| FAIL | `20260913T064617Z_creep_500g_30m/manifest.json` |
| FAIL | `20260913T072000Z_zero_return_15m/manifest.json` |
| FAIL | `20260913_load_cycles/cycles_analysis.json` |
| FAIL | `20260913_filter_compare/filter_comparison.json` |
| PASS | `20260913_slow_fill/slow_continuous/detected_fill_analysis.json` |
| PASS | `20260913_slow_fill/faster_continuous/detected_fill_analysis.json` |
| PASS | `20260913_rate_40hz/rate_analysis.json` |
| FAIL | `20260913T_cold_start_empty_60m/manifest.json` |

Result: 3/10 valid and 7/10 invalid against Git blobs. Every failed item is explained only by EOL normalization.

## Client 0x0510 baseline

The eight files bound by `source_evidence_file_sha256` are 0/8 exact against Git blobs at `c8b8f585203ea94ab4f0778713ef28582ad39ccc`. Each expected hash is reproduced exactly by converting its LF Git blob to CRLF. No Active register, Modbus response, device state, JSON value, or other non-EOL content difference was found.

The missing `.gitattributes` coverage is confirmed for `Results/pc_stage2c_0510_baseline/**` and `Results/pc_stage2c_0510_hw/**`.

## Classification

Historical capture Manifests remain immutable and are classified:

`HISTORICAL CAPTURE MANIFEST; INVALID AGAINST CURRENT GIT BYTES DUE TO EOL NORMALIZATION`

Repository-byte V2 manifests may recover these runs only as `RECOVERED ENGINEERING EVIDENCE`; they are not new hardware runs and do not claim that the capture-time Manifest was valid.

