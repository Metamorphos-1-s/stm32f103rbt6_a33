# Stage 5M-R5B Reference-Lock Engineering Beta

## Baseline

The requested remote branch `stage5mr5-reference-lock-offline` was not present
in the authenticated GitHub repository. The expected commit
`5892297eddc09f9a76e67a2b9fc968b64e005b63` was therefore not available. R5 was
rebuilt from the pushed R4 baseline
`e6665f2056eeacb46208483dfbc106d2b45d6e44`.

R5 is distinct from both the product `RuntimeDriftCompensator` and the earlier
Host-only `StaticDriftCompensator`. The R3/R4 failed candidate is not enabled.

## Frozen contract

This engineering beta is limited to the current 3 kg sensor, e=0.05 g,
Profile 0 and 10 Hz. Input is calibrated uncompensated gross mass in ug. Raw
ADC counts remain available for hardware diagnostics and are not used as
cross-sensor mass thresholds.

The 10 Hz stream is reduced to robust 1 Hz samples. An exact 300+600 value
implementation required 4,016 bytes of state and caused the STM32F103RB image
to exceed 20 KB RAM by 2,936 bytes. That first reconstruction is retained as
superseded evidence and is not the Beta implementation.

The RAM-qualified Beta groups each ten 1 Hz values into a median. It stores 30
reference-block medians and 60 rolling observation-block medians as signed
32-bit deltas from a 64-bit base. Rate and integration retain 1/1000 ug
resolution. Division truncates toward zero. Median averaging also truncates
toward zero.

OFF clears and does not apply the R5 offset. DOSING_NO_COMPENSATION applies the
existing offset but freezes all learning. STATIC_COMPENSATION preserves the
offset, waits 15 seconds, builds a 300-second locked reference, then evaluates
a rolling 600-second observation median every 60 seconds.

## Offline reconstruction

The exact-window reconstruction reproduced the requested result within the
published rounding precision, but is not linkable on this MCU. After adopting
the documented block-median structure, all offline gates were rerun. The six R4
captures produce median OLS improvement 9.94%, median endpoint improvement
19.69%, minimum endpoint improvement 4.37%, zero formal reverse amplifications,
and maximum 10-second offset change 0.000500 g.

The adverse second 500 g result is retained: raw -0.0193847 g/h, corrected
-0.0213215 g/h, OLS change -9.99%, endpoint improvement +4.37%.

The synthetic 12-hour scenario produces 0.203058 g final offset, 0.016942 g
loaded/unloaded residual, 500.00 g and 0.00 g displays at e=0.05 g, and
0.000051 g maximum 10-second correction. This is synthetic behavior evidence,
not a real 12-hour qualification.

Five real load/unload cycles produce exactly ten automatic rebases. All twelve
slow-dosing combinations preserve the full mass change with zero offset change.

## Qualification boundary

This is a current-sensor engineering beta only. The 2-4 hour qualification,
real 12-hour qualification and cross-sensor validation remain deferred. It is
not approved for legal metrology or production acceptance.

## Fixed-point and software gates

The bounded C state is 856 bytes on the host ABI. The ARM Cortex-M3 `-Os`
module uses 2,926 bytes of text and has a 152-byte maximum single-function
stack record. The complete Beta image links at 98,128 bytes Flash and 20,256 of
20,480 bytes RAM. Only 224 bytes RAM remain, so this build is not suitable for
additional buffered features without further resource work.

Python/C comparison covered 22,557 real R4 one-second samples and 25,057 total
samples with zero mismatches across every published snapshot field. Host CTest
passed 22/22. GCC 15.2 strict, Clang 23.1.1 strict, Stage 5B/5C/5L/R2/R3/R4/R5
Python, register-map consistency, Manifest tests and `git diff --check` passed.
ASan and UBSan are NOT RUN because the available portable MinGW distribution
does not include their runtime libraries.

The standard Release remains byte-identical at SHA-256
`82E726F5B32A0DE36A5E686F62A937EC4FD9CBB488DB9733E83D2062673EF486`.
The engineering Beta is Firmware 0x0511 with a non-frozen diagnostic extension
at 0x0280-0x02A7 and signature 0x55B5. The public Map remains 0x0104; no existing
0x0104 field changed meaning. Persistent Format remains V3 and contains no R5
mode or offset.

## Supervised hardware smoke

The application-only Beta BIN is 98,128 bytes with SHA-256
`DDB7AB025208252A7F2CAFEFC800CDB4C73EFD45FF874B9B5763A51F347B5E9C`.
It was programmed at 0x08000000 with byte verification. No mass erase was used;
the configuration region at 0x0801F000-0x0801FFFF was not written.

The supervised smoke ran from 2026-09-16T10:02:19Z to
2026-09-16T11:19:30Z. Formal per-second captures total 3,510 seconds (58.5
minutes) and 3,510 records.

- S1 OFF: 300 seconds, offset exactly zero.
- S2 empty SHADOW STATIC: 1,080 seconds; 300-second reference and 600-second
  observation completed; four evaluations; no offset movement.
- S3 500 g SHADOW: DOSING offset frozen; 1,080-second STATIC run completed
  eight evaluations; maximum 10-second candidate change 0.000127 g.
- S4 ACTIVE: 600 seconds; displayed count remained 49994; maximum absolute
  offset 0.011617 g; maximum 10-second change 0.000172 g. DOSING unload froze
  offset at 0.012485 g and preserved a 500.029 g load/unload difference.
- Returning to STATIC enforced the 15-second holdoff and completed a new
  300-second empty reference.
- Cleanup to SHADOW then OFF caused no display-count change. Final candidate
  offset is zero.

The full smoke had zero faults, zero ADC overruns, zero SAVE requests and zero
Flash writes after the initial application programming. Dirty remained zero;
revision and saved revision remained 7. The 4,096-byte configuration region
remained byte-identical at SHA-256
`D74C98D8D4221437773155E8D1ED75BC59D71AE2D285D5C2F18F6B494AA5DC86`.
Both V3 slots retain valid CRC and commit markers at sequences 7 and 6.

The device is left on the Beta image in OFF + SHADOW, with fault=0 and offset=0.

## Result

**STAGE 5M-R5B ENGINEERING BETA ENABLED; SHORT AND 12-HOUR QUALIFICATION DEFERRED**

The 2-4 hour short qualification and a real 12-hour qualification were not run.
No merge, tag or pull request was created.
