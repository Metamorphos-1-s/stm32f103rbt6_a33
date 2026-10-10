# Stage 5P Unified-R diagnostic closure errata

This is an additive correction to the historical R report. The original
report and raw logs are retained unchanged.

## Profile command identity

The external Modbus mailbox command table is not the C `CommandId` enum. The
mailbox maps external command `7` to `COMMAND_SWITCH_WEIGHING_PROFILE`; the C
enum value `23` is not the mailbox profile command. The previous R host probe
sent `command=23`, received a mailbox `result=0`, and incorrectly described
that as a successful profile switch. It did not request profile 1.

The corrected contract is:

| Start profile | Requested profile | Mailbox command | Expected result |
| ---: | ---: | ---: | --- |
| 0 | 1 | 7 | `ACCEPTED=1`, followed by asynchronous completion |
| 1 | 0 | 7 | `ACCEPTED=1`, followed by asynchronous completion |
| 0 | 0 | 7 | `OK=0` |
| 1 | 1 | 7 | `OK=0` |

The contract tests are in
`Tools/stage5_unified/test_profile_switch_contract.py` and pass. The prior
R result is therefore corrected from “profile request returned OK but did not
switch” to “the host sent the wrong mailbox command ID; profile-switch
execution was not tested in R.”

## Build-command wording correction

The archived `compile_commands.json` is authoritative. Its 133 C compile
commands use `-Os`; the startup assembly command uses `-O3 -DNDEBUG`. The
historical R report's generic wording that called the whole diagnostic build
Release `-O3 -DNDEBUG` was imprecise. The source, binary, map, logs and SHA
values are not changed by this wording correction.
