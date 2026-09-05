# PC Client Stage 2A Runtime Operations

## Scope

This branch adds safe runtime-operation infrastructure for ZERO, TARE, CLEAR
TARE and the contract-supported NET/GROSS view command. It does not implement
parameter editing, communication changes, SAVE, calibration or factory reset.
Hardware writes remain `NOT RUN — explicit hardware-operation authorization required`.

Baselines: client Stage 1 `6dae392`, STM32
`9e242c7649ad79ac4cbae347b3259847f4bc09e`, CH579
`3cd21ecefe6486d28a664ea356cb4ed6e7b3090a`.

## Command contract

The firmware mailbox is `0x0040–0x004B`. One FC16 request submits 12 registers:
nonzero token at `0x0040`, command ID at `0x0041`, zero/default arguments,
flags at `0x004A`, and `0xA55A` in final execute register `0x004B`. IDs are
ZERO `1`, TARE `3`, CLEAR TARE `4`, SET_WEIGHT_VIEW `5`; NET/GROSS use argument0
0/1. FC16 echo is only transport acknowledgement. The client reads response
registers `0x004C–0x0057` with FC03 and requires matching response token and
last command ID before accepting the firmware result code. See
`contracts/modbus-v0104/runtime-operations.json`.

## Transactions and arbitration

`RuntimeCommandService` exposes validation, transport wait, sending, business
result wait, state verification, success, rejection, failure and
`RESULT_UNCERTAIN`. Commands require fresh `MONITORING`, compatible Map `0x0104`
and no prior uncertainty. Tokens increment with wrap and are never changed for
a retry. The monitoring loop yields while the command is active; no polling
request can interleave the FC16 mailbox submission, business response read or
state readback. Write timeout, post-write disconnect, token/command mismatch or
readback failure locks further writes and shows the operator the explicit
uncertainty message. There is no automatic retry after bytes may have been sent.

Successful commands read realtime state again. ZERO checks the zero flag; CLEAR
TARE checks tare zero; NET/GROSS checks the weight-view register. TARE preserves
the resulting snapshot for operator comparison.

## WPF

The WPF runtime area contains ZERO, TARE, Clear TARE, NET and GROSS buttons.
Every action requires a confirmation dialog, disables all other runtime actions
through execution, displays transaction/result/readback status and retains a
bounded audit entry. `Refresh state (clear uncertainty)` is the only way to
clear a `RESULT_UNCERTAIN` lock after a complete FC03 refresh. No SAVE,
configuration, calibration or factory-reset control is present.

## Verification

The existing Protocol/Core tests pass 33/33 after adding mailbox FC16 and
concurrent-operation mock coverage. Debug/Release, TypeScript and contract
regressions must be rerun before any hardware write. Stage 2A hardware evidence
files are intentionally NOT RUN and contain no write frames.

## Hardware plan

Before authorized execution, confirm a safe non-production setup, record empty
and stable initial weight/TARE/zero state, and prepare rollback with CLEAR TARE,
NET view and an empty platform. Run TCP, RS232 and RS485 independently with
five successful operations per supported command, state readback, panel
comparison and post-command 600-second read-only monitoring. Never use SAVE or
calibration. Stage 2B starts only after all evidence is complete.

## Hardware validation status

Hardware authorization was received for the non-production test setup. The
first RS485 ZERO attempt on COM5 changed the instrument to `0.00 g STABLE` and
matched the physical panel, but the client reported `RTU CRC mismatch` while
parsing the FC16 response. The attempt is not accepted as a PASS and all
subsequent writes on that transport were stopped. The RTU receiver was fixed to
read the complete FC06/FC16 response length; the test must be repeated with a
fresh WPF process before continuing the matrix. See
`Results/pc_stage2a_hw/rs485_runtime_operations.json`.

After the RTU fix and new-mainboard replacement, RS485 on COM5 passed the
complete command matrix: ZERO 5/5, five TARE cycles including GROSS/NET/CLEAR
TARE, and one fast-double-click suppression test for each operation. The
post-command 600-second read-only window also passed with no timeout, CRC,
MBAP, unit, exception, bad-frame, transport or reconnect errors. Evidence is
in `Results/pc_stage2a_hw/rs485_new_board_zero.json` and
`Results/pc_stage2a_hw/rs485_post_command_600s.json`. TCP and RS232 hardware
matrices remain pending.
