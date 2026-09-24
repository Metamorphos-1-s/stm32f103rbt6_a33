# Stage 5P-A2B Local Menu Closure: Software Stop

## Result

**STAGE 5P-A2B FAILED; LOCAL MENU TRANSACTION OR PERSISTENCE CONTRACT NOT
SATISFIED; 0x051A NOT APPROVED FOR ENGINEERING PRODUCT USE.**

This is a pre-flash software finding. No 0x051A hardware run was opened, no
SWD configuration read, application erase, SAVE, key operation, or power cycle
was performed in A2B. The device was not changed; its read-only COM5 snapshot
still reports firmware 0x0517. The A2 PLC/Modbus results remain historical A2
evidence, not local-menu qualification.

## Baseline And Reproduction

- Branch: `stage5pa2b-local-menu-closure`, from
  `d2b4742dac57dc7cb77fe72059cd533d8557447e`.
- Product source changes in A2B: none. The audited menu controller blob is
  `1d92ee2ca81a33750379693702b82bb247aee05d`; R5 local control is
  `a865f280d47d3c6cd20b65a2737684fd92215609`; Checkweigh local control is
  `6812589eabfd74cc1b9aa0db3b151685d76de604`.
- Clean `Stage5PA2Candidate` ARM build: RAM 19,464 B, Flash/BIN 107,532 B.
  Rebuilt BIN SHA-256:
  `32A3184D93A0E1CC92C1F786455623176223DC2FC2430748F924B020AEF428B8`.
  Rebuilt ELF: 2,024,200 B, SHA-256
  `5937DF5E29E6B65A7595449B8E786A1B68F0BBD331A6EBCB1E4BDEE2DECC74DA`.
  A2 conservative stack 1,504 B and collision margin 536 B are reused because
  the product source and RAM result are unchanged; no new stack analysis was
  performed in A2B.
- Debug Host build and CTest: 36/36 pass, including existing menu, R5 local,
  Checkweigh local, V3 persistence and Modbus tests. Release Host strict build
  fails in existing test sources on MSVC unused-variable warnings (for example
  `test_ble_command_protocol.c`, `test_ble_command_service.c`, and
  `test_startup_auto_zero.c`). This is a separate incomplete software gate;
  it is not evidence that the local-menu contract passes.

## Actual Menu Contract

| Item/text | Current behavior |
|---|---|
| `drIFt` | `OFF` = SHADOW+OFF; `SHAdO` = SHADOW+STATIC; `StAtIC` = ACTIVE+STATIC; `doSInG` = ACTIVE+DOSING. Selection text is defined in `UI/r5_local_control.c`. |
| `ALArn` | `OFF`, `StAtIC`, `dynAnI` (the actual six-character DYNAMIC label). |
| `SPd` | Read-only; not editable to 10/40 Hz in this menu. |
| `FILt` | Numeric edit values 0-3, corresponding to filter modes 0-3; not `filt0` through `filt3` text. |
| strength | No independent menu item. Confirming a filter choice writes strength 0 for filt0, 2 for filt1, and 1 for filt2/filt3. |

Short FUNCTION confirms the current edit as a candidate. Long FUNCTION in the
menu applies an R5/Checkweigh candidate immediately, or requests an asynchronous
candidate SAVE for ordinary `DeviceConfig` edits. STAR/HASH select/change; once
an R5 or Checkweigh candidate is confirmed, navigation is locked until apply or
cancel. TARE cancels the edit or menu transaction and is consumed by the menu,
not dispatched as weighing TARE. A 30-second inactivity timeout discards the
candidate. These paths are in `UI/menu_controller/menu_controller.c`.

## Blocking Findings

1. **R5 and Checkweigh long-confirm does not SAVE.** In
   `MenuController_HandleKeyEvent`, the R5/Checkweigh branches call their
   `Apply()` functions, show `donE`, and set `s_exit_after_save`; neither calls
   `RequestSave()` or waits for `PersistenceManager`. Their command handlers
   update requested fields and mark `DeviceConfig` dirty, so `donE` means APPLY,
   not durable commit. After the menu exits, a fresh session does not own that
   dirty revision; a long press with no new candidate reaches the `bUSY` guard
   in `RequestSave()`. `MenuController_AllowCurrentDirtySave()` is only called
   after calibration, not after R5 or Checkweigh apply. H4-7 cannot meet the
   local SAVE/power-cycle contract on the frozen 0x051A image.
2. **Rate and strength hardware matrix is unreachable from the menu.**
   `MENU_ITEM_SAMPLE_RATE` is skipped by advanced navigation and is explicitly
   read-only. There is no strength item. The filter edit unconditionally sets
   strength to 0/2/1, so a menu change to filt1 or filt3 cannot preserve the
   specified strength3. H4-6 and both H4-7 combinations are impossible by the
   stipulated local-menu-only path.
3. **R5 stale-generation protection is incomplete.** `R5LocalControl_Apply()`
   compares only application and mode. An external change away from and back to
   the same pair can leave an old candidate accepted, unlike Checkweigh's
   generation compare. The requested H4-8 generation contract is not proved.
   No hardware concurrency test was attempted after findings 1 and 2.

The ordinary configuration edit path does use `DeviceConfig`, revision checks,
`PersistenceManager_RequestCandidateSave()` and the existing two-slot V3 store.
The R5/Checkweigh menu paths use the command service and the same requested
config fields, but they do **not** complete the same local SAVE transaction.
Runtime offset, reference, evaluation, and Checkweigh classification/output
remain volatile under the A2 persistence design; A2B did not requalify them.

## Device And Stop Boundary

Read-only `Results/stage5pa2b/software_failure/preflight_0517.json` was captured at
2026-09-24 14:09:46 UTC. COM5 reports firmware 0x0517, Map 0x0104,
Persistent Format 3, 10 Hz, profile 0, display 500.05 g, R5 OFF+SHADOW,
offset/reference/evaluation zero, fault/overrun/dirty/SAVE requests zero,
revision/saved 8/8, active slot B sequence 8. The expected pre-test config
region SHA-256 is
`A615A475C2D7B5D64126EAEB3AAE9E3D094348FA4C4AEC7BDB8EF0010D4A3BB4`
from A2; it was **not reread** by SWD in A2B, so no new config-SHA equality
claim is made.

The A2B hard-stop rule applies before any authorization prompt or flash. No
configuration backup, hardware H4 matrix, physical power cycle, or final-device
choice was performed. The frozen 0x051A binary must not be patched and reused
as if it were an independently qualified candidate. A repair requires a
separate stage and new firmware identity, then fresh software and hardware
qualification. A2's 0x051A PLC/Modbus qualification remains preserved; formal
release, metrology and cross-sensor qualifications remain deferred.
