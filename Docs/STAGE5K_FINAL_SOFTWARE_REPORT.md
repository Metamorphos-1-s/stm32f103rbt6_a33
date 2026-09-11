# Stage 5K final software report

Branch: `stage5k-fw0510-correctness-refactor`

The audited baseline was `08eda9a9504a59eca9238527ee5d8952ffc235a5`.
The implementation closeout commit is
`1e7f9912d9376a5c93d3989a2621839f6e273e28`. The branch is not merged to
`main`, tagged, or force-updated.

Identity is explicit: Firmware `0x0510`, Modbus Map `0x0104`, public
configuration Schema `2`, internal persistent Format `3`, V3 payload `281`
bytes, and BLE protocol `1`. Modbus `0x013E` reports public Schema `2`;
`0x01C0` reports internal persistent Format `3`; BLE reports public Schema `2`.
Existing client register interpretation is unchanged.

Deferred SAVE is externally observable at `0x01C5` (result), `0x01C6`
(mailbox request token), `0x01C7` (source), and `0x01C8-01C9` (associated
revision in configured word order). Reads do not consume state. A new valid
SAVE starts a new pending record and clears the previous binding; rejected or
competing requests cannot overwrite an in-flight request. Terminal results
remain readable until the next valid SAVE or reboot.

`DeviceConfig_Validate` is the pure validator used by ConfigApplication,
ConfigEdit, PersistentCodec V3, communication validation, and Modbus
CONFIG_VALIDATE/APPLY. Revision arithmetic is centralized in
`App/revision_helper.c`. Reconfigure and storage restart share an explicit
Metrology rebuild helper with a raw-replay mode. The large Modbus and
CommandService entry points were retained because a behavior-changing split
would add coupling without improving the current ownership boundaries.

V1/V2 codec entry points, migration code, legacy projection modules, old
count-based configuration fields, and development normalization were removed.
V1/V2 numeric schema constants remain only so old slots are recognized and
rejected as unsupported; they are never decoded as V3 or automatically
overwritten.

Local gates passed: MSVC CTest `16/16`, Stage 5B Python `30/30`, Stage 5C
Python `12/12`, register-map consistency, four ARM builds, and software
manifest `4/4`. BoardDiagnostics remains below the 124 KiB application limit.
The new `.github/workflows/stage5k-portable.yml` runs GCC, Clang, and GCC
UBSan on Ubuntu with strict warnings. Run `34592586554` completed successfully
for implementation commit `1e7f9912d9376a5c93d3989a2621839f6e273e28`.

Not executed: firmware flashing, RS232/RS485 field regression, BLE phone
regression, CH579 board regression, physical power-cycle tests, and the final
600-second concurrency run. PC strict preflight needs a new `0x0510` baseline;
clients may later consume the SAVE diagnostics. The CH579 transparent gateway
core does not require a protocol change. Stage 5J hardware evidence is not
reused as Stage 5K/0x0510 hardware closure.
GitHub Actions workflow `Stage 5K portable host gate` run `34592586554`
completed successfully for implementation commit `1e7f9912d9376a5c93d3989a2621839f6e273e28`:
https://github.com/Metamorphos-1-s/stm32f103rbt6_a33/actions/runs/34592586554

The final UB audit found that `DeviceConfig_Validate` repeated alarm span
validation using signed subtraction. `INT64_MAX - INT64_MIN` overflowed before
the result could be checked. The duplicate calculation was removed so
`AlarmConfig_Validate` is the single alarm validator and retains its safe
unsigned span calculation. Host tests now execute enabled full-range alarms,
maximum legal hysteresis, equal/reversed thresholds, disabled extreme values,
Modbus staging VALIDATE/APPLY with unchanged-state failure checks, V3 encode,
and both LimitChecker hysteresis arithmetic branches. Disabled alarms retain
the existing policy that threshold ordering is ignored while other enum and
nonnegative-hysteresis rules still apply.

The disabled V1/V2 test blocks were physically deleted from
`Tests/host/test_stage5a.c`. Unused V1/V2 payload and body-size macros were
removed; only V1/V2 schema identifiers remain for explicit rejection.

Implementation commits are `a3e9c5e2e89416a8a2b3374d890380ae3c634fae`
and its GCC cleanup `6d92b1efa39b9c9b2d6fdf950d0f3fbb6cfbb4fe`.
Portable run `34610805291` passed GCC, Clang, and UBSan for the latter commit:
https://github.com/Metamorphos-1-s/stm32f103rbt6_a33/actions/runs/34610805291
The earlier successful run `34592586554` remains historical evidence; failed
run `34610447015` exposed and rejected a duplicate unused test helper before
tests ran.

Final status: `STAGE 5K SOFTWARE READY; HARDWARE VALIDATION PENDING`.
