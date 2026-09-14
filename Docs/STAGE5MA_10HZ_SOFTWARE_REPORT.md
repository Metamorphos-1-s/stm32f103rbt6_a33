# Stage 5M-A 10 Hz Software Report

Status: `STAGE 5M-A NO ACCEPTABLE CANDIDATE; REQUIREMENTS OR ALGORITHM REVISION REQUIRED`.

## Delivered

- Current raw/filter/calibration/zero/tare/stability/display/alarm/Modbus/BLE pipeline audit.
- Git-bound inventory of 38 complete runs with development/holdout/regression split and labels.
- Deterministic replay v2 with explicit timestamp wrap, actual input-gap handling and separate host-observation omissions.
- Exact reproduction of the four published hardware baseline metric sets.
- Dual-IIR and robust dual-IIR offline comparison under one metric contract.
- HAL-free fixed-point C reference and Host stream runner.
- Python/C comparison: 900 actual records, zero integer/state mismatch.
- Resource evidence: 256 B state, 1724 B ARM `-Os` object text, 104 B Process stack, no dynamic allocation or module int64 division.

## Failed attempts retained

Replay v1 incorrectly interpreted FC03 observation omissions as device input gaps and kept stable false. G1/G2 proves device processing continued, so v2 reopened holdout once to correct only that input contract; no interpolation or parameter change occurred. V2 then failed real frozen thresholds: stable 5.573 s >3.0 s, slow false-stable 24.56% >10%, and static false-positive 1.53% >1%.

The robust dual-IIR is a research reference, not a selected candidate. No Stage 5M experiment is linked into Debug, Release, BoardDiagnostics or Stage5LDiagnostics. No firmware was flashed and no product behavior, protocol field, map, schema, persistent format, Active configuration or firmware version changed.

## Boundaries

Automatic zero, tare learning, calibration attraction and low-frequency drift subtraction are not implemented. The 40 Hz path remains contained because G3 external timing is blocked. Stage 5M-B hardware validation cannot begin without a newly selected candidate. Stage 5N and Stage 5O were not entered.

## Next revision criteria

A future candidate must improve slow-trend persistence without increasing static false positives, and decouple stable qualification from a long settling tail while preserving holdout noise. It must be tuned only on the frozen development runs or declare a new dataset/version before opening any new holdout. The existing v2 failure remains part of regression evidence.

## Repository and gates

STM32 started at `91e0208c2ffc4681a6ff3dfb5b2fab1a2ebd8b74`. Evidence/manifests end at `c51fbe6878b5441857e63199392b8959a0ce5c4c`; the final report-only commit follows. Client remains clean and unchanged at `c4e4906f0a47a427793df6cfcb414756ac7984cc`; CH579 remains clean and unchanged at `eb888925e4fcc9dcd9bf89e8cc42e5b28679e520`.

Commits are `a0f4d51` for audit/replay/fixed-point reference and Host targets, `39512c1` for Git-blob Manifest generation, `5cfd655` for preserved v1/v2 results, and `c51fbe6` for both run manifests. The dataset split contains 20 development, 17 holdout and one regression run. Both Manifest V2 records pass clean checkout under `core.autocrlf=false`, `input` and `true`.

MSVC `/W4 /WX` Host CTest passes 18/18. Stage5B rate policy passes 3/3 and its full Python suite 30/30; Stage5C passes 12/12; Stage5L passes 8/8; G2 tools pass 8/8; Stage5M-A tools pass 9/9. Register-map consistency and Debug, Release, BoardDiagnostics and Stage5LDiagnostics clean builds pass. The isolated ARM reference compiles with `-Wall -Wextra -Werror`. Clang, ASan and UBSan are `NOT RUN` because supported tooling remains unavailable.

Product Release SHA-256 remains `82E726F5B32A0DE36A5E686F62A937EC4FD9CBB488DB9733E83D2062673EF486`, and its symbols contain no AdaptiveMeasurement/Stage5M entry or state. Firmware 0x0510, Map 0x0104, Schema 2 and Persistent Format 3 are unchanged. This stage performed no device I/O, firmware flash or configuration write.

No merge, tag, pull request, force push, rebase or history rewrite occurred. Stage 5M-B, Stage 5N and Stage 5O were not entered.
