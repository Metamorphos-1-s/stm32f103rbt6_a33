# Stage 5P-A2C Local Menu Persistent Transactions

## Software Gate Result

**STAGE 5P-A2C SOFTWARE CANDIDATE READY; LOCAL MENU SAVE/POWER-CYCLE
HARDWARE CLOSURE INCOMPLETE; 0x051B REMAINS AN ENGINEERING CANDIDATE;
DEVICE RESTORED TO FROZEN 0x0517.**

The A2B failure is preserved in `Docs/STAGE5PA2B_LOCAL_MENU_CLOSURE.md` and
was not reused as a hardware result. A2B's 0x051A position remains:
`PLC/MODBUS PERSISTENCE QUALIFIED; LOCAL MENU PERSISTENCE NOT SUPPORTED;
NOT APPROVED FOR LOCAL-MENU ENGINEERING USE`.

## Revision And Scope

- A2B start: `0f25f8b2cc5d85cba7b3f28cdb864efa5c2e4dd4`.
- A2C branch: `stage5pa2c-local-menu-persistent-transactions`.
- A2C changes are limited to menu transaction plumbing, profile controls,
  A2C identity/build wiring, an internal no-revision request restore helper,
  Host tests and evidence. R5 algorithms/parameters and admission, D1-D,
  Checkweigh classification/output gating, CS1237, Modbus, V3 layout and
  public Map/Schema were not changed.
- A2B push was retried before A2C. GitHub first returned a TLS handshake
  failure, then an `Internal Server Error` rejection. No remote, credential,
  certificate or proxy setting was changed. A2B and A2C pushes remain pending
  final delivery retry.

## Implemented Contract

R5 and Checkweigh local candidates now populate the existing `DeviceConfig`
candidate and enter the existing `PersistenceManager_RequestCandidateSave`
path. Runtime R5/Checkweigh restoration uses no-revision restore helpers, so a
single menu SAVE advances one revision. `donE` is produced only by the existing
SAVE completion path; SAVE busy, power unsafe, validation failure, stale
revision/generation and failed SAVE leave `donE` absent. SAVE failure retains
the documented dirty/runtime behavior or restores the original snapshot when
the request cannot start.

The advanced menu now exposes:

| Item | Values and behavior |
|---|---|
| `drIFt` | `OFF` = SHADOW+OFF, `SHAdO` = SHADOW+STATIC, `StAtIC` = ACTIVE+STATIC, `doSInG` = ACTIVE+DOSING; request fields only are persistent. |
| `ALArn` | `OFF`, `StAtIC`, `dynAnI`; request mode only is persistent. |
| `SPd` | Editable `10Hz` / `40Hz` only. 640/1280 Hz remain rejected by product validation. |
| `FILt` | Displays `FILt0` through `FILt3`; changing it leaves strength unchanged. |
| `StrEnG` | Independent strength candidate, bounded by the shared validator: filt0 0-8 (ignored by the filter), average 2-32, IIR/median3-IIR 1-8. |

`filt0` accepting and retaining strength 0-8 is an A2C validation-contract
extension requested by the user; the filter implementation itself is unchanged.
No new persistent field or permanent configuration copy was added.

## Tests And Builds

- Host Debug CTest: 38/38 PASS.
- Host Release CTest: 38/38 PASS with `/W4 /WX`; the prior NDEBUG-induced
  unused-variable failures are fixed by retaining assertions in Release tests.
- A2C persistence test: PASS, including filt0 strength retention, bounds and
  V3 request/calibration round trip.
- A2C menu transaction test: PASS, including candidate isolation, TARE,
  timeout, stale revision/generation, SAVE busy/power unsafe/validation
  rejection, no-change, delayed `donE`, and rate/filter/strength field
  isolation.
- ARM Debug candidate 0x051B: RAM 19,464 B, BIN 107,928 B, SHA-256
  `2EA4F7AB549820AB6B0C3A020D99DA3FFB151E0B5C5648BE013A34B7AA11FEEE`;
  ELF SHA-256
  `A0FB0936848374275C4E57C918DE0CA8BB8E2E467C90449CDF600BA1262B48D7`.
- ARM Release: BIN 91,668 B, SHA-256
  `8457F64F0315E29A872C2CD27C4C01380B0A07554ECE5671B64ACE8B6A331B78`;
  ELF SHA-256
  `A972D2CDB30F7C7CC440C1B758AD75939684FBE6F075534CAE325FB14E01718A`.
- ARM strict Release (`-Wextra -Werror`): PASS; BIN 91,668 B, SHA-256
  `77D27C311F6D41F2A5E7D43F727B11A896B649B61274270CDE49FE45CAB7B59A`.
- Fresh call-graph stack analysis: linker RAM 19,408 B at Release, heap 0,
  `_Min_Stack_Size` 1,024 B, bounded conservative stack 1,408 B and static
  collision margin 688 B. The Debug linker's 19,464 B remains below the A2C
  ceiling. No dynamic allocation calls were found in product sources.
- Historical frozen parity remains cited unchanged: R5 legacy 25,057
  samples/0 mismatch, R5 sample 95,200/0, D1-D 49,533/0 and Stage 5N-A
  45,711/0. A2C did not alter those engines. ASan/UBSan remain NOT RUN.

## Current Device And Authorization Stop

Read-only `Results/stage5pa2c/software/device_preflight_0517.json` was captured
at 2026-09-24 15:15:05 UTC. COM5 reports firmware 0x0517, Map 0x0104, about
500.07 g, 10 Hz, R5 OFF+SHADOW, offset/reference/evaluation 0, fault 0,
overrun 0, dirty 0, SAVE requests 0, revision/saved 8/8, Persistent Format 3,
active slot B sequence 8. No SWD configuration read, application erase,
0x051B flash, SAVE, TARE, calibration, ZERO or physical power cycle was done.

The pre-test configuration backup plan is a 4096-byte SWD read with SHA-256
and V3 A/B slot parse, followed by application-only erase/program/Verify. It is
not authorized yet. The planned application range is the firmware application
region only; the configuration pages remain untouched.

The next hardware step requires the exact user authorization:

```text
授权读取配置并烧录0x051B
```

Until that exact authorization is received, the device remains on 0x0517 and
all local-menu H1-H8 tests are pending. Formal release, metrology, cross-sensor,
12-hour, D1-C long-duration, external DRDY, sensor-disconnect, ASan and UBSan
qualifications remain deferred.
