# Stage 5A-UIR: Local UI Metrology Adaptation

## Baseline and scope

- Actual baseline: `337af5af12c31982de3f27d4c553b69e40bda1e0`.
- Baseline worktree was clean.
- This change adapts local display, menu editing, calibration, zero/tare and
  configuration validation to the canonical signed 64-bit `MassValueUg` model.
- No `.ioc`, CubeMX `Core/`, HAL, USART2 DMA, TIM4, RS485 DE, Modbus address,
  linker layout or configuration-slot address was changed.

## Root causes

The local UI still treated V1 display counts as authoritative physical values.
In particular, calibration copied an `int32_t` span count into micrograms,
capacity/division/zero/overload edits targeted legacy fields, and runtime
validation required both old and new representations. The old advanced-menu
detector also consumed ordinary STAR/HASH navigation.

The corrected ownership is:

| Function | Authoritative field | Legacy projection |
| --- | --- | --- |
| Capacity | `metrology.capacity_ug` | `metrology.capacity` |
| Active unit | `metrology.active_unit` | `metrology.unit` |
| Unit format | `unit_display[unit]` | `division`, `decimal_places` |
| Zero range | `metrology.zero_range_ug` | `metrology.zero_range` |
| Overload | `metrology.overload_threshold_ug` | `metrology.overload_threshold` |
| Profile | `profiles[active_profile]` | gain/rate/filter/stability fields |
| Calibration mass | `calibration.span_mass_ug` | `span_weight` |
| Runtime tare | `runtime.current_tare_ug` | `current_tare` |

## Implementation

`CalibrationSession` now stores `span_mass_ug`, a display-only signed 64-bit
count, the locked input unit, decimals and 1/2/5 division. Calibration starts
from the existing legal span mass or `capacity_ug`, quantizes it through
`MassToDisplay` and converts it back through `CountToMass`. STAR/HASH changes
1e, ZERO cycles 1e/10e/100e/1000e, and every candidate is checked for positive
mass, capacity, six-digit representation and exact round trip. The commit chain
is capture zero, `SET_SPAN_MASS(value64)`, capture span, commit RAM. The old
`SET_SPAN_WEIGHT` command is retained by number but returns `NOT_IMPLEMENTED`.

The menu uses typed config commands for 64-bit mass, unit display and profile
fields. Capacity, zero range and GENERAL overload are edited as display counts
then converted to micrograms. Division cycles 1/2/5 and decimals cycle 0..5 for
only the active unit. Filter and stability update the active profile. Detailed
sample-rate and gain entries are explicitly read-only because the current edit
transaction cannot safely wait for asynchronous ADC reconfiguration; whole
PROFILE switching remains available through `WeighingProfileManager`.

At the UNIT home item, HASH navigates immediately. An initial STAR is buffered
for at most 1000 ms between keys and 4000 ms total. A complete
STAR-HASH-STAR-HASH sequence enters the advanced menu; mismatch or timeout
replays every buffered key through ordinary navigation.

The display reads `gross_mass_ug`, `net_mass_ug` or `tare_mass_ug`, converts it
once using the active `UnitDisplayConfig`, and then formats the decimal point.
Compatibility `int32_t` snapshot values are derived from the same conversion.
Unit switching rebuilds the current snapshot immediately without changing
calibration, zero or tare mass. Zero and tare continue through the mass-domain
engine and use `zero_range_ug` and `tare_mass_ug`.

## Validation and compatibility

`MetrologyConfig_ValidateCanonical()` validates only physical configuration,
unit formats, profiles, metadata and compliance rules.
`MetrologyLegacyV1_Validate()` is used only by V1 encode/decode/migration.
Canonical V2 configuration remains valid when every legacy field is zero or
invalid.

`metrology_legacy_projection` is a checked one-way adapter. It runs before V2
encoding and successful configuration application. It never changes canonical
fields and rejects overflow instead of truncating.

Schema inspection found all required values already present in V2, including
64-bit capacity, ranges, profiles, calibration span and runtime tare. Schema V2
therefore remains exactly 344 bytes. V1 remains 164 bytes. A/B slots remain at
`0x0801F000` and `0x0801F800`; CRC32, commit-last and PVD gating are unchanged.

## Legacy reference audit

- V1 codec reads/writes legacy metrology, stability, span and tare fields only
  in `persistent_codec.c`.
- `metrology_legacy_projection.c` is the only new producer of legacy fields.
- `COMMAND_GET_CONFIG` and the existing Modbus compatibility view expose
  projected display counts; neither is an input to local metrology.
- `CalibrationModel_Build`, `WeightEngine_Init`, legacy zero/tare and
  `WeightValue` display helpers remain compatibility APIs. New runtime paths use
  `BuildMass`, `InitMass`, `MassSnapshot` and mass-domain zero/tare functions.
- Default configuration initializes both representations for V1 export.
- No local menu or calibration path reads a legacy value as authority.

## Verification

MSVC C11 `/W4 /WX` clean-first build passed. All five host tests passed:
Stage 4A UI/metrology, Stage 4B codec/store, Stage 4B-R persistence fault
injection, Stage 5A mass/register model and Stage 5B RTU/DMA/RS485 logic.
Coverage includes exact kg/g/lb conversion, 1/2/5 quantization, canonical
validation with invalid legacy fields, V1 migration, V2 round trip and 64-bit
tare, positive/negative calibration direction, RAM-only calibration commit,
menu navigation/replay, zero/tare, CRC/A-B/PVD and Stage5B regressions.

ARM clean-first results relative to the recorded baseline are:

| Build | FLASH | Delta | RAM | Delta |
| --- | ---: | ---: | ---: | ---: |
| Debug | 99,748 B | +3,892 B | 10,648 B | +48 B |
| Release | 54,352 B | +2,408 B | 10,664 B | +40 B |
| BoardDiagnostics | 97,732 B | +3,812 B | 10,640 B | +48 B |

Debug load image ends at `0x080185A4`, below application limit `0x0801F000`
with 27,228 bytes remaining. CONFIG_A/B usage is 0 bytes in all builds. No
dynamic allocation, floating point, FreeRTOS, `HAL_Delay`, Domain HAL access or
UI direct HAL/CS1237 access was added.

## Manual board test

NOT TESTED ON HARDWARE.

The required board run is: verify noCAL startup; enter the ordinary menu by
FUNCTION long press; test HASH/STAR navigation and the advanced sequence; edit
capacity, independent kg/g/lb format, zero range and GENERAL overload; perform
zero and positive/negative-direction span calibration with a known mass; verify
RAM indication before SAVE; test zero/reset-zero, tare/clear-tare and unit
switching; SAVE and power-cycle; confirm calibration/config/tare retention; then
read matching mass and status over existing Modbus without transport errors.

Software acceptance is complete and the code is suitable as the Stage 5C
development baseline. Hardware metrology, panel usability and real-scale
calibration remain required before a product release.

## Stage 5A-UIR hotfix follow-up

Real panel testing after this adaptation found a missing k/K glyph, temporary
message state replacing the menu base page, legacy projection incorrectly
using the six-digit display limit, and full WeightEngine reinitialization during
a display-unit change. These findings and their regression evidence are covered
by [STAGE5A_UIR_HOTFIX.md](STAGE5A_UIR_HOTFIX.md).

The follow-up centralizes kg/g/lb labels, makes Unit an explicit
candidate/confirm/cancel editor, adds an independent message overlay, uses an
unbounded checked converter for legacy 32-bit fields, and applies display-unit
changes without resetting the measurement chain. The original hardware section
above remains historical evidence for this adaptation; the hotfix panel run is
tracked separately and must not be inferred from host tests.

## Changed tree

```text
App/                    calibration, config, metrology, profile, result mapping
Domain/calibration/     canonical calibration model
Domain/measurement/     canonical validator, legacy projection, snapshot view
Protocol/command_service/ typed commands and calibration guards
Services/config_edit/   typed transaction setters
Services/config_store/  V1/V2 validation and one-way projection
UI/display_controller*  canonical numeric display
UI/display_model/       centralized result labels
UI/menu_controller/     canonical editor and replaying sequence state machine
Tests/host/              canonical UI, codec, navigation and Stage5B regression
```
