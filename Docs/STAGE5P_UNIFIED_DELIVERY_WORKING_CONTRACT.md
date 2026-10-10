# Stage 5P unified delivery: working contract

Status: **software candidate in progress; not a flashable or qualified release**.
Baseline: `578f7ce9d7923a98b477001645f7f75f6e504126` (A13E-R4).
Candidate branch: `stage5p-unified-product`. New functionality is behind
`A33_ENABLE_STAGE5P_UNIFIED` (OFF in ordinary builds), with prospective
firmware identity `0x0522`. The independent PA14 long-rate experiment is not
part of this branch.

## Frozen user decisions

| Setting | Target |
| --- | --- |
| Factory sampling/filter | 10 Hz, filt1, strength3, gain128 |
| Factory drift request | ACTIVE + STATIC; boot offset starts at zero |
| Factory checkweigh request | OFF |
| Other profiles | 10/40 Hz with each legal filt0–filt3/strength combination; not yet implemented for A13E |
| Weight unit/display | g, d=0.01 g; legal e=1 g remains separate |
| Battery | 4S 18650; corrected millivolts = measured bus millivolts + nominal 400 mV; 12.8–16.8 V maps linearly to 0–100% |
| Charging | No MCU charging status; bus voltage while charging may make the percentage optimistic |
| Low-voltage UI | Warning at 13.6 V after 30 s, recovery at 13.9 V after 10 s; indicator flashes and `Lo-bAt` intermittently replaces the weight for short intervals |
| Power saving | ON by default; after 10 min of official stable state and no key activity, brightness reduces from 3 to 1; sampling and communication continue |

`13.2 V` is a stored critical threshold reserved for future diagnosis; this
candidate does not implement a battery shutdown. Without BMS information the
voltage display is only an approximate indicator, especially while charging.

## Implemented in the isolated flag so far

- Factory configuration requests 10 Hz/filt1/strength3 and ACTIVE+STATIC.
  Existing saved V3 configuration takes precedence; an application-only flash
  never rewrites a user's calibration or other settings.
- Boot keeps the authoritative weight uncorrected until a valid calibrated
  sample with no fault/overload and no active checkweigh is available. It then
  attempts the saved R5 request from zero offset. A manual OFF cancels the
  deferred request. Conflicting saved ACTIVE and checkweigh requests are
  rejected instead of silently applying both.
- Advanced-menu R5/checkweigh short FUNCTION accepts the session candidate
  and returns to the menu list. STAR/HASH continues to navigate. TARE discards
  the session. Long FUNCTION uses the existing atomic candidate SAVE path.
  Changes to either runtime mode by another owner invalidate that menu
  candidate before SAVE. ACTIVE R5 and non-OFF checkweigh cannot be saved
  together; turning one OFF in the same candidate session permits the other.
- PLC/Modbus R5 pair changes update the pending V3 request in one revision.
  A separate explicit SAVE is required for power-cycle persistence. Automatic
  safety stops and startup restoration never change the saved request or
  trigger SAVE. Checkweigh commands already follow the same explicit-SAVE
  rule; the menu uses its atomic candidate SAVE.
- If ACTIVE has stopped for a safety reason, its saved request remains ACTIVE.
  A PLC request for checkweigh ON is rejected until the operator explicitly
  sets R5 OFF (or commits an atomic menu candidate that turns it OFF), so an
  ordinary SAVE cannot store both conflicting requests.
- Saving display-only fields does not rebuild metrology or clear a running R5
  reference/offset. An actual metrology change still rebuilds and re-arms
  the requested R5 state from a fresh reference.
- Battery page shows `P`-marked integer percentage rather than voltage. The
  advanced status battery item uses the same `P` suffix. The underlying
  corrected millivolts remain available for diagnostics. Low
  voltage is confirmed using the existing battery ADC state storage.
- Display-only idle brightness reduction restores upon a key event or loss of
  official stable state. It does not alter the sample rate, outputs, or Flash.
- The existing g/kg/lb unit selector remains available, subject to the
  existing compliance-mode validator. No new unit path was introduced.

## Evidence and remaining gates

- Host Debug/Release: 47/47 each (including the new default/startup/menu test).
- Existing guarded ACTIVE Host test passes unchanged in behavior. Its obsolete
  assertion of a removed display snapshot field was removed to build it.
- GCC `-Wall -Wextra -Werror` focused syntax check: PASS.
- ARM target build, RAM/stack map, product binary hash and target timing:
  **NOT RUN** in this environment; no ARM toolchain is installed.
- No device was accessed, flashed, calibrated or saved in this branch.
- A13E still admits **only 10 Hz/filt1/strength3**. The requested multi-rate,
  multi-filter compensation is **NOT IMPLEMENTED**. A candidate SAVE that
  requests ACTIVE in another profile is rejected until that admission path
  exists; do not enable this flag on the device as a unified delivery yet.
- The standalone PLC ConfigEdit path's R5 re-arm behavior after a metrology
  rebuild is not yet closed. The atomic local menu SAVE path is wired to
  re-arm; do not claim profile switching with ACTIVE R5 from PLC until the
  independent path is tested and made coherent.
- The battery warning/dimming UI and the new SAVE order require one focused
  on-board validation after ARM resource gates pass. No new 12-hour drift
  qualification is being asserted here.

## Next implementation slice

1. Design a bounded 40 Hz admission path for the 10 Hz A13C controller:
   preserve wall-clock holdoff, reference and observation lengths and the
   offset limit per 10 s; detect invalid/near-rail/fault immediately even on
   samples not admitted to the controller. Preserve raw sample sequence
   diagnostics and do not hide a missing sample. Profile changes keep the
   existing offset where permitted, freeze correction, and rebuild reference.
2. Test all supported filter/strength combinations with actual noise and
   loading records, including 40 Hz under the normal five-block Modbus load.
   Admission and step detection must not manufacture a compensation rate four
   times faster than 10 Hz or consume legitimate DOSING steps.
3. Decide whether the separate PA14 10 ug long-rate candidate should replace
   the frozen 5 ug rate. It needs its own parity and focused on-board result;
   this branch has not silently adopted it.
4. Build an exact 0x0522 candidate with ARM Debug/Release/strict flags and
   verify conservative stack collision margin at least 512 B. Check persistent
   V3 encode/decode and rollback, then perform a single focused on-board
   menu/PLC/power-cycle/battery/sampling integration sweep.
5. For an existing device, back up V3 and perform a one-time, explicit
   candidate SAVE of 10 Hz/filt1/strength3, ACTIVE+STATIC and checkweigh OFF.
   Preserve actual calibration, Modbus address, communication and units;
   expect the V3 slot sequence to advance exactly once and verify its CRC and
   commit after power cycle. Changing default constants
   alone does not migrate the currently saved OFF request.
