# Firmware 0x050C menu save-ownership baseline

This document records the Firmware 0x050C software and read-only hardware
baseline after the local menu persistence-ownership fix. It does not authorize
or claim a real menu SAVE, PC Stage 2B persistence qualification, or a power
cycle validation.

## Identity and commits

- Production commit: `2af4abe39ddb3336d91be64fe8c75425c0dbc1aa`
- Evidence commit: created by this document commit
- Firmware: `0x050C`
- Register Map: `0x0104`
- Persistent Schema: `2`
- Release ELF SHA-256: `895999B7547935827FC64DFF70EE5F4DF7B00E1FD413706E1BFDBEFAD5925E44`
- Branch: `fix-usart3-command-source-validation`
- Previous 0x050B evidence remains historical and is not rewritten.

## Software change

Confirmed local menu edits now retain an in-memory `local_pending_save` marker
and the exact `local_pending_revision` across TARE/timeout exit and menu
re-entry. `MenuController_Init` clears ownership. A successful asynchronous
SAVE clears it only when both current and saved revisions equal the frozen
request revision. A failed, timed-out, or uncertain SAVE leaves valid ownership
for a later explicit user action, without automatically retrying or reapplying
configuration. Any intervening foreign revision permanently invalidates the
marker. Unknown dirty state present on menu entry cannot be claimed by a later
local edit; explicit `SAUE` remains the intentional whole-snapshot path.

Profile completion records the revision returned by the profile manager. The
manager rejects an external revision change before applying the profile and
publishes the actual resulting revision; the menu does not predict a revision.
ConfigStore persists a complete configuration/runtime snapshot, so no field
merge is attempted.

## Offline verification

- C host CTest: **16/16 suites passed**.
- Stage 4A host: **all checks passed**; Stage 5B host executable: **1213 checks passed**.
- Stage 5B Python: **30/30 tests passed**.
- Stage 5C Python: **12/12 tests passed**.
- Register Map check: passed; source reports `0x050C`, `0x0104`, Schema `2`.
- Debug clean build: passed, 0 warnings/errors, Flash `95,576 B`, RAM `17,792 B`.
- Release clean build: passed, 0 warnings/errors, Flash `82,284 B`, RAM `17,760 B`.
- Relative to `34cd363d30d6b0f271f6f9b9ba73c8bd91262dc6`: Debug `+688/+24 B`, Release `+648/+24 B` (Flash/RAM).
- `git diff --check`: passed.

The host tests cover cross-session dP ownership, TARE and timeout exits,
unknown dirty rejection, foreign revision invalidation, second local revision
updates, revision wrap, SAVE target revision, success/failure/timeout behavior,
no duplicate SAVE, profile result ownership, and initialization clearing.

## Read-only hardware record

Hardware operations were limited to one application-area ST-Link program,
one verify, one reset, configuration-area reads, one RAM read, and read-only
Modbus FC03 sampling. No FC06, FC16, Mailbox command, configuration write,
SAVE, menu SAVE, or physical power cycle was performed in this evidence run.

- ST-Link: one program, verify succeeded, one reset; no retry.
- Device: STM32F101/F102/F103 medium-density, Flash 128 KB, 3.29 V.
- Linker application region: `0x08000000-0x0801EFFF`.
- Config slots preserved: Slot A `0x0801F000-0x0801F7FF`; Slot B `0x0801F800-0x0801FFFF`.
- Configuration region SHA-256 before and after programming:
  `27A057A6973DFB3E6AE7EBF16B4FC722820340A1A975CC325EEBF8D278D32F16`.
- Active register reads: 64/64, two identical rounds.
- Active canonical big-endian SHA-256:
  `4BA7DA269DECB38D631ED8076A4FE90B7EF70AA04123CF15D5662B7DBBD4DD98`.
- Compact active JSON SHA-256:
  `B7D78D5BD4A6DE0BE2C0DA201C167A0178608FCFF49F6297664C87C79018EE73`.
- Brightness: `3`; dirty: `0`; current/saved revision: `19/19`.
- Active slot: `A`; sequence: `19`; ConfigStore: `IDLE`.
- Mailbox: all zero/idle; FC03: `7 attempted, 7 succeeded, 0 failed`.
- FC06/FC16/commands: `0/0/0`.
- RAM divider read: `47000/10000 ohm`; Flash slot payload agrees.
- Fault mask and CS1237 overrun: `0`; power-safe: `1`.

## Manual confirmation

At `2026-09-09T12:04:46.1712504Z`, the operator confirmed:

`Firmware 0x050C基本按键与显示确认通过，无配置修改、无SAVE`

The confirmation covered stable display, normal-page STAR short no-op, STAR
long entry to `FIr`, STATUS TARE exit, menu entry to `UnIt`, menu TARE exit,
and no parameter change or SAVE.

## Scope boundary

Real cross-menu SAVE, PC Client rebaseline to Firmware 0x050C, and any two-
reboot persistence qualification remain pending independent authorization.
