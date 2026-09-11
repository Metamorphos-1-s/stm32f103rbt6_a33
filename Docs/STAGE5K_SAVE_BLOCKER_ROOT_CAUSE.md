# Stage 5K Deferred SAVE blocker root-cause status

The failed hardware SAVE returned mailbox `ACCEPTED` (token 251), then Deferred
SAVE terminal `INVALID_STATE` with dirty state unchanged. The raw Modbus fault
words at `0x0039-0x003A` were `[0x0040, 0x0000]`. Configured high-word-first
decoding yields mask `0x00400000`, which `FaultManager_Bit()` maps to
`FAULT_UI_STATE_ERROR` (fault code 23). The earlier report's
`FAULT_CS1237_DATA_ERROR` label was incorrect: CS1237 data error is `0x20`,
while calibration invalid is `0x40`.

Production source has no `FaultManager_Set(FAULT_CALIBRATION_INVALID)` call.
The only setter for `FAULT_UI_STATE_ERROR` is the App_Main branch where a menu
calibration request is present but `CalibrationController_Begin()` fails. This
supports the causal chain UI state error -> App fault/safe state -> failed
storage-maintenance entry -> Deferred SAVE `INVALID_STATE`, but the original
menu/key trigger was not captured. No production SAVE or fault-clearing behavior
was changed. A bounded fault-setter trace or SWD watchpoint is required before
choosing a behavioral fix.

Reproducible mask decoding is in `Tools/stage5k_hw/fault_mask.py`. Until the
first fault write and a targeted successful SAVE are captured, status remains
`STAGE 5K SAVE BLOCKER ROOT CAUSE UNCONFIRMED`.
