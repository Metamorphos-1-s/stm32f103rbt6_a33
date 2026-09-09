# Firmware 0x050D keypad duplicate-revision fix

Firmware 0x050D is the software successor to the historical 0x050C image. It
keeps Register Map `0x0104`, Schema `2`, the 47k/10k battery divider and all
historical 0x050C evidence unchanged.

## Root cause

`MetrologyManager_Reconfigure` rebuilt the WeightEngine and synchronized its
derived tare state through `SystemContext_SetTareStateMass`. That API is the
user-operation path and increments the persistence revision when the runtime
tare tuple differs. During an internal reconfiguration, a temporarily
different engine/context tare tuple could therefore consume one revision;
`SystemContext_ApplyConfig` then consumed a second revision for the actual menu
edit. The local menu ownership marker recorded the later state and correctly
refused to save when the revision sequence no longer represented one local
edit.

## Minimal fix

Internal metrology synchronization now uses
`SystemContext_SyncTareStateMass`, which updates the runtime tare tuple without
creating a persistence revision. Real TARE and CLEAR TARE operations continue
to use `SystemContext_SetTareStateMass` and retain their revision/dirty
semantics. Menu configuration edits therefore produce exactly one revision;
TARE/timeout navigation and menu re-entry do not apply the candidate again.

## Software evidence

The host regression creates a deliberately mismatched context/engine tare
state, confirms a dP edit, and asserts one revision increment plus the exact
local pending revision. The full host suite and firmware builds must pass
before any 0x050D hardware qualification is authorized.

This document does not claim 0x050D has been flashed or hardware-validated.
The 0x050C keypad failure evidence remains preserved under
`Results/stm32_fw050c_keypad_save_validation/`.
