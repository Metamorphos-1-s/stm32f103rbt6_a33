# Stage 5P-A13E: software hard-stop, NOT qualified for ACTIVE

Date: 2026-09-28. Independent branch `stage5pa13e-guarded-active-engineering`;
baseline `7ccfe181d81079e8f1b670a7fedb574d0b79cbfe` (remote A13D-R).
The contract was frozen and committed before development as
`Docs/STAGE5PA13E_FROZEN_ACTIVE_CONTRACT.md` at commit
`775ca36f841c01cbd5867d545f6f80cc0f189633`.

**Decision: SOFTWARE GATE FAIL; A13 GUARDED ACTIVE ENGINEERING BETA NOT READY.**
No COM5 query, SWD/config access, application flash, SAVE, ZERO, TARE,
calibration or physical testing occurred in this stage. There is no current
device readback; the historical 051D/OFF+SHADOW is NOT fresh confirmation.
No 0520/0521 BIN was released or flashed. Do not flash any build of this
branch: the integration code is a default-OFF *unqualified development draft*.
There is no final hardware state claim beyond the absence of device operations.

## Exact blocking evidence

The A13B (88,765 samples) and A9 (107,491 samples) previously opened CSVs
were replayed with the unchanged C/Python A13 implementation: corrected,
offset, states and timing match, 0 mismatches each. Six synthetic replays
(`step`, `cycles`, `pulse`, `motion`, `gap`, `dosing`) also match. Full SHA,
per-sample logs and maximum 10 s correction are in
`Results/stage5pa13e/software/parity_*.json`; none are new holdout.

The A13C-R real hardware trace at
`Results/stage5pa13c_r/static_load_unload_continuous/samples.csv`,
SHA-256 `499D2F14D2E2A04FEE27369209610089AA39D91CD39B1CBABD04A27B8D2CA838`,
was replayed continuously across recorded STATIC/DOSING modes, with resets
only at actual OFF, 10,774 unique active samples. The previous historical
MCU/Python comparison reports 0 primary-signal mismatches, but the **new
strict key-timing parity is FAIL (1 mismatch)**:

- At DOSING_EXIT event sequence **85212**, frozen Python clears its old
  `obvious_seq`, so the new event has no obvious-step sequence.
- Frozen C snapshot still reports the old loading step **80441** as
  `obvious_step_sequence`. C `LeaveDosing` clears `obvious_valid` and updates
  robust/quiet/lock information but does not clear the old public sequence;
  Python `SampleController.feed` clears `event_seq` and `obvious_seq`.
- C/Python corrected mass, offset, state, gate count/reasons, current quiet
  sequence85212, reference lock85661 and first correction86334 agree. This
  localizes the mismatch to stale **diagnostic metadata**, not a demonstrated
  numerical compensation difference. It is still a frozen strict timing-field
  mismatch, so no one may report full A13C-R timing parity or proceed to
  ACTIVE qualification on this result.

Preserved first failing output:
`Results/stage5pa13e/software/parity_a13cr/c_python_segment_0.json`.
The entrypoint also saves an explicit FAIL summary, without suppressing or
changing the existing comparator:
`Results/stage5pa13e/software/parity_a13cr/summary.json`.
It was reproduced by:

```powershell
python -B Tools/stage5pa13e/replay_implementation.py --runner D:/Downloads/a13e_builds/host/stage5pa13c/Release/stage5pa13c_replay_runner.exe --output Results/stage5pa13e/software/parity_a13cr
```

The runner can be rebuilt from `Tests/host/stage5pa13c/CMakeLists.txt`.
The failing exit code is expected and must not be relabeled PASS. A future
separately reviewed solution may alter *only public diagnostic metadata*,
with proof the frozen A13 mathematics and sample outputs remain unchanged;
re-run strict replay and all gates on new artifacts. No patch to A13C or its
Python model was made here. No A13B data were used for tuning.

## Other work, explicitly NOT the complete software gate

The draft adds independent build flag `A33_ENABLE_STAGE5PA13E_ACTIVE`
(default OFF), prospective identities 0520/0109 and 0521/010A (not built as
distributable BINs); an explicit volatile pair command and generation, a
menu-long-FUNCTION volatile apply path, fail-safe OFF logic and a policy
excluding concurrent formal checkweigh. No A13 computation file changed;
no DMA/Modbus/BLE buffer/stack reserve was intentionally reduced. Existing
ordinary051D Release rebuilt byte-identical at 93,588B SHA-256
`DDF57FB131E5EF675D94B08A5A8752EB0FB588BA5230DEA6F34C57D6B2F62DDD`.
The ordinary/A13E/diagnostic A13C math object SHA matches:
`EFCAA070EC912A2746C89E7C27DAEB39829E9FF953DBF3468038E22B63AC38EC`.

Draft real-module Host integration (menu, zero/tare, offset, alarms/faults,
PLC generation and output) and full Debug/Release CTest ran **46/46 PASS**
at their recorded source revision. Initial strict ARM Debug failed on an
unused compatibility function; log preserved; subsequently repaired and four
preliminary strict ARM builds completed. Preliminary map/static results:

| Build (pre-final code) | RAM | Complete static stack | Collision |
|---|---:|---:|---:|
| A13E Debug | 19,432 B | 1,552 B | 520 B |
| A13E Release | 19,384 B | 1,448 B | 672 B |
| A13E + cumulative diagnostic Debug | 19,432 B | 1,552 B | 520 B |
| A13E + cumulative diagnostic Release | 19,384 B | 1,448 B | 672 B |

These bounds include the existing 256B indirect allowance and nested ISR
frames. **Do not promote them to final PASS**: the code was subsequently
revised for transient raw authority in the sample path, then the strict
timing-field replay failed and the phase stopped. No final re-build/map
or target diagnostic measurement of that exact draft was completed. Target
runtime Feed, loop and stack high-water: **NOT RUN**. OFF/ACTIVE hardware,
display/PLC/checkweigh hardware, load/unload, DOSING, power-cycle and
backup/flash/rollback: **NOT RUN**. Exact 0520/0521 BIN size/SHA: **NOT RUN**.

The diagnostic-vs-ordinary timing cannot be inferred from earlier A13D-R
8.393ms. We do not claim product efficacy: A13B remains **INCONCLUSIVE**,
historical A13D remains **FAIL**, A13D-R resource qualification remains PASS
only for its tested build. Long-duration/high-stimulus efficacy, 40Hz/other
filters, cross-sensor, true slow feed and legal metrology remain deferred.

This branch preserves a reviewable opt-in draft and a *negative* test record,
not an approved engineering firmware. No merge, tag or PR is requested.
