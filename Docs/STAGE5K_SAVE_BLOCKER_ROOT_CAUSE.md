# Stage 5K Deferred SAVE blocker root-cause status

The original failed SAVE returned mailbox `ACCEPTED` (token 251), then Deferred
SAVE terminal `INVALID_STATE`. Its raw fault words `[0x0040, 0x0000]` decode in
high-word-first order as `0x00400000`, `FAULT_UI_STATE_ERROR` (fault code 23),
not `FAULT_CS1237_DATA_ERROR`. The latter is `0x00000020`; calibration invalid
is `0x00000040`.

Static source has one production setter for `FAULT_UI_STATE_ERROR`: the
`App_Main` branch where a menu calibration request exists but
`CalibrationController_Begin()` fails. The hotfix retains the user-facing
invalid-state result but no longer escalates this recoverable UI failure into a
global Fault/SafeState that can disable storage maintenance. After flashing the
hotfix, the post-reset fault mask was zero, confirming the stale UI fault was
not present at boot.

The targeted revalidation then exposed a second unresolved issue: a fresh
Modbus calibration session successfully began, accepted 500 g, and captured
empty zero, but `CAPTURE_CALIBRATION_SPAN` repeatedly returned
`INVALID_STATE` instead of reaching `LOAD_READY`. No commit, SAVE, or power
cycle was attempted after that failure. The session-state transition or its
runtime reset is therefore not yet proven, and the hardware closure remains
blocked. Evidence is in
`Results/stage5k_hw/20260912T_stage5k_hotfix_preflash/targeted_revalidation.json`.

The minimum production fix was to map `PersistenceManager_RequestSave()`'s
`COMMAND_RESULT_OK` (the no-change terminal) to Deferred SAVE
`COMM_SAVE_RESULT_NO_CHANGE` instead of `FAILED`. After rebuilding and
flashing, a no-change SAVE returned result `3` with token/source/revision
`450/Modbus/1`; a real brightness `3->4` SAVE returned `SUCCESS` with
`503/Modbus/2`, and a second `4->3` SAVE returned `SUCCESS` with
`603/Modbus/3`. Both physical power-cycle recoveries passed. The SAVE blocker
is therefore closed; the remaining full hardware gates are tracked separately.

Status: `STAGE 5K SAVE BLOCKER CLOSED; FULL HARDWARE VALIDATION PENDING`.
