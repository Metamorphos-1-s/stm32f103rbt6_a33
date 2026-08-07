# Stage 5A-DSP-A-HF1 Board Fixes

## Scope and baseline

- Baseline: `eed02b753ed02a24e80a264a90e66399aad94abb` from `origin/stage5a-dsp-a`.
- Branch: `stage5a-dsp-a-hf1`.
- Product hardware: STM32F103RBT6, CS1237, six-digit display, and a 3 kg rated load cell.
- Scope: board-profile correction, display-lock release, operator zero anchoring, ZERO feedback, and third-key diagnosis.
- Out of scope: merging `main`, BLE/Stage 5C, automatic Flash save, standard-weight attraction, and register-map/schema changes.

## Root causes and fixes

The board uses a 3 kg load cell. New and factory defaults now use 3 kg for both sensor rated capacity and configured scale capacity/overload threshold. These remain distinct configuration fields so a future user CAP setting is not mistaken for sensor identity.

The previous display-release formula was `max(8 * division, 2 * stability_exit)`. The saved development profile used a 4 g exit threshold, making release 8 g. A real 1 g change therefore updated WeightEngine but did not release the held panel display. HF1 uses only `8 * division`; at the default 0.01 g division the release threshold is 0.08 g. Three consecutive samples must exceed it, so an isolated 0.2 g spike and +/-0.03 g jitter remain held while sustained 1 g and 500 g changes release.

Successful ZERO and successful TARE while viewing net weight now request a display-only zero anchor. WeightEngine applies either operation and recomputes its snapshot synchronously, so there is no pending operation state. The first following valid sample captures a separate authoritative release reference once and then freezes it; normal post-operation settling is therefore not compared with the visual zero. Initial settling grace is 1000 ms, and continuous instability has a bounded 3000 ms timeout because the active 8-sample window cannot rebuild within 1000 ms at 10 Hz. Three consecutive over-threshold deviations from the frozen reference can still release immediately. Restore-zero, clear-tare, gross-view tare, configuration changes, calibration, overload, and faults still force tracking.

Authoritative `gross_mass_ug`, `net_mass_ug`, and `tare_mass_ug` remain continuous. Only panel/current-display values use the anchor. Modbus `0000-0001` follows the conditioned panel value, while `0010-001B` remains authoritative in both word orders. Calibration, ZERO range decisions, tare, overload, and future true-weight transport do not consume conditioned mass.

ZERO now has a usable 60 g General-mode default. Failures distinguish unstable, out of range, tare active (` tArE `), zero disabled (`ZrOFF `), missing calibration/sample, and internal failure. A failed command never creates an operator anchor. No configuration is saved automatically.

## Restricted startup normalization

Schema V2 Flash configuration normally overrides compiled defaults. HF1 therefore recognizes only the exact former high-precision development tuple: 10 Hz, gain 128, `MEDIAN3_IIR`, strength 3, window 8, enter 2 g, exit 4 g, hold 500 ms. It changes stability to enter 0.05 g, exit 0.10 g, and hold 1000 ms. If and only if that tuple is in General mode with zero range 0, it also sets zero range to 60 g.

Capacity, overload, unit/decimal/division, calibration, and communications are preserved. The existing dirty/migration-pending flag records the in-RAM change; persistence still requires an explicit operator SAVE.

## Third key

Software mapping is confirmed: raw key bit `0x04` maps to `KEY_ID_ZERO`; a short press dispatches ZERO and a long press dispatches restore-zero, with no short event after the long event. The display feedback paths are also covered by host tests. Physical TM1628 bit identity, wiring, and contact operation remain a board test and must not be inferred from software tests.

## Verification status

Host tests cover the restricted legacy normalization, default 3 kg profile, threshold formula and saturation, noise/spike rejection, 1 g/500 g release, operator grace, TARE/ZERO success and failure paths, raw/logical third-key mapping, short/long dispatch, Modbus panel-versus-authoritative separation, word order, and read-only behavior.

All six host suites pass. Coverage includes preserving a stable lock across an unchanged menu visit and releasing it after a real configuration change. ARM Debug, Release, and BoardDiagnostics clean builds pass. Relative to baseline `eed02b7`, Debug is +956 B Flash/+24 B RAM, Release is +540 B Flash/+16 B RAM, and BoardDiagnostics is +964 B Flash/+24 B RAM. Final linker use is 105,516/10,832 B, 57,172/10,832 B, and 103,308/10,824 B respectively. Application Flash still ends at `0x0801F000`; configuration slots remain `0x0801F000-0x0801FFFF`. Static scans found no added allocation or floating-point use and no generated/CubeMX source changes.

Release was programmed and byte-verified on 2026-08-05 using STM32CubeProgrammer 2.19.0 over SWD. Target voltage was 3.29 V; only application sectors 0-55 were erased, the two configuration slots were preserved, and software reset completed. Read-only FC03 on COM5 at address 1, 115200 N1 returned register map `0x0101` and firmware `0x050A`.

The saved board configuration has CAP 10 kg and a filter-mode field that does not match the exact former development tuple. HF1 therefore correctly preserved CAP, calibration, communications, and the unmatched saved profile without applying the restricted normalization. The display-release fix is independent of that profile, so the active 0.01 g division still yields a 0.08 g release threshold.

Physical third-key polling later captured three repeatable press/release cycles: pressed raw mask `0x0004`, released `0x0000`. The final press lasted about 1.89 seconds. After it, display telemetry reported tracking with forced release reason 5, consistent with the ZERO-key long-press restore-zero path. The physical bit identity and short/long software mapping are therefore confirmed. The Modbus command mailbox remains zero for local-key actions by design.

The final anchor/menu firmware was then programmed and verified with the same application-only erase boundary. A Modbus ZERO command passed 55 consecutive observations over 12 seconds: panel count 0, conditioned mass 0, LOCKED throughout, release reason 0, while authoritative net mass continued between approximately -0.035 g and +0.047 g. TARE passed another 55 observations with the same zero/LOCKED result while authoritative net mass varied approximately -0.031 g to +0.061 g; CLEAR TARE then succeeded. An unchanged physical menu visit passed 152 observations with the same lock and anchor retained from entry through exit.

The 500 g plus approximately 1 g load-step test also passed. The initial 500 g load settled and locked at a 500.050838 g display anchor. Adding the small object without pressing the scale released the old anchor with `DISPLAY_RELEASE_DEVIATION`; the panel reached 501.00 g about 0.13 seconds after release and relocked about 0.91 seconds after release at a 501.143294 g anchor. The measured object increment was approximately 1.09 g, and no standard-weight attraction was applied. Removing the small object returned to a locked value near 500.12 g, although that removal transition occurred before telemetry capture began. Removing the remaining 500 g also returned automatically to a locked empty-load display of 0.03 g with a 0.025984 g anchor and `DISPLAY_RELEASE_DEVIATION`; authoritative mass continued between approximately -0.010 g and +0.046 g during the 12-second observation. The removal itself preceded telemetry capture, so its exact release latency was not measured.

These results validate the reported ZERO, TARE, and unchanged-menu jump fixes on the target. They do not prove metrological accuracy or replace the remaining load-step tests.

## Board acceptance checklist

1. Program and verify Release over SWD without mass erase; preserve `0x0801F000-0x0801FFFF`.
2. Confirm startup configuration at address 1, 115200 N1, including 3 kg CAP and the normalized high-precision stability tuple.
3. At empty load, press TARE and ZERO separately; confirm immediate `0.00` display and continuing authoritative telemetry.
4. Apply and remove 1 g; confirm release after sustained change and return to zero after the corresponding operator action.
5. Apply and remove 500 g; confirm prompt release in both directions without changing calibration.
6. Press each physical key separately while observing raw key register `0x0037`, logical events, and display response. Record the actual raw mask for the third key.
7. Confirm no automatic Flash save occurred; save only with explicit authorization.

## Unresolved items

Physical third-key identity, ZERO/TARE anchor retention, unchanged-menu return, the 500 g plus approximately 1 g load step, and automatic empty-load recovery after removing 500 g have passed. The board is calibrated with a 500 g span and saved with CAP 3 kg, G/dP 2/division 1, and zero range 60 g. The saved OL remained 10 kg; in G/dP 2 this is outside the six-digit editable range and correctly reports `UnItHI`, so it must be reduced while using a representable unit such as kg. Exact 500 g removal latency and the smaller 0.02/0.03/0.05/0.10 g disturbance matrix remain unmeasured. No main-branch merge or BLE work is part of this hotfix.
