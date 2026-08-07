# Stage 5A closeout: persistent-dirty semantics

## Scope

This note records the configuration-dirty audit performed after the HF2-R1
hardware regression. It is intentionally a semantic audit; no firmware logic
was changed.

## Findings

- Schema V2 is a 344-byte payload. It stores `RuntimeState.current_tare_ug`
  as the persistent tare mass. The V1 compatibility payload also stores the
  legacy tare value and active flag.
- `DeviceConfig.system.tare_power_loss_retention` gates tare restoration.
  `MetrologyManager_Init()` restores tare only when that setting and the
  persisted `RuntimeState.tare_active` are both true.
- `MetrologyManager_Tare()` and `MetrologyManager_ClearTare()` call
  `MetrologyManager_SyncTare()`. That updates the runtime tare fields through
  `SystemContext_SetTareStateMass()`, which increments the config revision and
  sets `runtime.config_dirty` when the value or active flag changes.
- `SystemContext_MarkRevisionSaved()` clears `config_dirty` only for the
  revision actually committed to flash. Therefore a SAVE followed by TARE or
  CLEAR TARE is expected to leave the runtime dirty until a second SAVE.
- ZERO and RESET ZERO modify `WeightEngine`'s `ZeroTareState.zero_offset_raw`
  only. That offset is not represented in `RuntimeState` or the current
  persistent schema, so these operations do not mark persistent configuration
  dirty and do not survive a power cycle. This is the current product
  behavior, not an accidental omission in the dirty flag path.
- Calibration/profile/unit/config edits remain persistent configuration
  changes and mark the runtime dirty through the existing context APIs.

## Hardware result interpretation

The final HF2-R1 hardware snapshot had `config_dirty=true` because the test
sequence performed SAVE and then later TARE/CLEAR TARE/runtime actions without
a second SAVE. The flag is therefore consistent with the implementation and
the observed flash contents.

## Stage 5C naming

BLE-facing diagnostics should expose this state as `persistent_dirty` (or an
equivalent explicit name) while preserving `RuntimeState.config_dirty` in the
firmware ABI until a deliberate schema/API change is approved. BLE transport
must not clear or set this state directly; configuration commands should use
the existing context/persistence APIs.

