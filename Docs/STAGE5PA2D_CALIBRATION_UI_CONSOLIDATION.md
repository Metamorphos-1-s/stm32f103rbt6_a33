# Stage 5P-A2D Calibration and Menu Interaction Candidate

## Scope and status

`SOFTWARE CANDIDATE; ARM RESOURCE GATE AND TARGET HARDWARE CHECK PENDING`.
This branch starts at `d241f750b8f6420bfc5b6138531e2e7df614caf5`.
The build preset `Stage5PA2DCalibration` identifies the *unflashed*
engineering candidate as firmware `0x051C`. With the candidate flag disabled,
the existing build paths retain their behavior. The physical device last
reported firmware `0x0517`, R5 `OFF + SHADOW`, checkweigh `OFF`, and saved
revision `8/8`; no device access was available in this execution environment.

## Operator contract

1. Enter the calibration menu with a clean saved configuration and an empty
   scale. Press FUNCTION once to begin zero acquisition.
2. Enter the known standard mass. Put that standard on the platform **before**
   pressing FUNCTION. That key accepts the entered mass and begins stable span
   acquisition directly; the separate `LOAD` confirmation is removed.
3. A valid span is applied to RAM and an automatic Flash SAVE is started.
   Display `donE` only after the requested revision is saved and the runtime
   and saved revisions match. Exit directly to the live weight page after the
   confirmation interval, with no extra key press or stuck menu page.
4. An unsafe/rejected/failed SAVE displays `ErrSAU`; RAM can remain dirty and
   must never be reported as persisted. A running save with uncertain result
   holds calibration keys until its outcome becomes known; no second write is
   started. If the backend calibration session expires before committing,
   show an error and exit rather than leaving a dead calibration page.
5. An unsaved setting, external dirty revision, or pending local candidate
   blocks a new calibration, because the Flash record includes the entire
   configuration. TARE cancels a calibration before the save starts.
6. In the menu, a long FUNCTION while editing validates the currently visible
   value first, then starts the same transactional save used after short
   FUNCTION confirmation. This also applies to the R5 and checkweigh choice
   pages. An invalid edit remains on its edit page for correction; a stale
   revision returns `bUSY` and starts no SAVE. TARE and the 30-second menu
   timeout still discard uncommitted menu changes.
7. During calibration mass entry, valid local input keys refresh the local
   backend's 120-second inactivity timer. A queued key with an older
   timestamp cannot move the timer backwards. Unattended sessions and remote
   calibration sessions keep their existing timeout semantics.

The automatic save changes persistent calibration by design. It does not
change calibration math, R5 drift parameters, display following, checkweigh
classification, Modbus mapping, or the V3 persistent layout.

## Software evidence and limits

- Existing and candidate Stage 4A calibration integration tests: PASS with
  `-Wall -Wextra -Werror` on GCC host. The candidate covers automatic save,
  delayed confirmation, a rejected write, a failed write, a late successful
  write, a backend timeout, and activity extending mass editing beyond two
  minutes.
- Existing and candidate A2C menu tests: PASS with strict GCC host flags.
  Candidate tests cover unconfirmed numeric, R5, and checkweigh values saved
  by long FUNCTION; rejected input remaining editable; and stale revision
  protection.
- Existing and candidate fake Flash persistence tests: PASS. They verify an
  applied calibration reaches a loadable V3 record only through the authorized
  local session; a simulated interrupted write preserves the earlier record.
- The five changed product C units compile on GCC host with candidate flags,
  `-Os -Wall -Wextra -Werror`; CMakePresets JSON and `git diff --check` pass.
- ARM Debug/Release links, exact BIN identity and size, map RAM, ARM call graph
  stack bound, and physical behavior are **NOT RUN** here: no ARM compiler or
  connected serial/SWD programmer is present. Host `-fstack-usage` cannot
  replace the STM32 stack margin requirement. Do not flash until the existing
  conservative stack collision gate of at least 512 bytes is met.

## Single focused target session

These checks are directly tied to the changed behavior; there is no reason
to repeat a 12-hour drift test, the eight filter/rate combinations, alarm
response matrices, or physical fault injection for this UI change.

1. On the Windows bench, check out the exact branch/commit and build the
   `Stage5PA2DCalibration` preset. Run candidate and existing Host CTest,
   Debug/Release ARM builds and the 512-byte collision-margin check. Record
   the exact BIN SHA and verify identity `0x051C`. Stop before flashing if an
   ARM warning, memory, or regression gate fails.
2. Take a read-only Modbus status snapshot and back up the two 2-KiB V3
   configuration slots before programming. Program and Verify only the
   application pages. Check the immediate post-flash configuration bytes are
   unchanged; preserve the original `0x0517` application and configuration
   backup for rollback.
3. Run one deliberate 500 g local zero/span calibration. Check the absence of
   `LoAd` and `rAnonL`, the `CAL SP` acquisition, `SAUE` and `donE`, and
   automatic return to live weight. `APPLY` may last only a 10 ms tick, so
   use a logger to check that transient state if needed. Check exactly one SAVE,
   one revision increment, matching revision/saved revision, no dirty/fault,
   and a reasonable loaded/unloaded span. The deliberate SAVE will change
   the configuration-slot SHA; compare its contents and sequence with the
   expected new calibration rather than claiming the SHA stayed unchanged.
4. Physically power cycle once and read back the calibration span and
   revision from the V3 record. Reweigh the same 500 g standard and unload.
   This checks actual persistence; a RAM-only readback before reset is not
   sufficient.
5. With the scale idle, edit one harmless normal menu value and long-press
   FUNCTION **without** first short-confirming it; check the new value is
   saved. Repeat the direct long press for one R5 and one checkweigh choice,
   restoring the original requested modes before leaving the device. For one
   candidate, use TARE before saving and confirm zero SAVE/revision change.
   Host tests cover stale and invalid edits, so do not simulate unsafe power
   faults or destructive failures on the instrument.
6. Read the final firmware, modes, saved revision, fault/overrun/dirty/SAVE
   counters and V3 slot health. Record any interruptions as NOT RUN. Decide
   whether to retain the qualified candidate or restore the exact backed-up
   `0x0517` application/configuration as part of the handoff; do not label an
   unperformed operation PASS.

This target session is **pending**; no physical calibration or device Flash
write was performed in the present workspace.
