# Stage 5A UIR Hotfix

## Baseline and trigger

The actual local baseline after `git fetch origin` was
`48b0e56a1f0480369894f052ebd728513f62c790` (`Stage 5A-UIR`), equal to
`origin/main` at the start of this work.

This hotfix was triggered by two real panel observations:

- Repeated unit changes showed `g -> lb -> lb -> g` even though the third
  internal unit was kg. The kg prompt contained `k`, which the formatter did
  not support, so formatting failed and TM1628 retained the previous lb frame.
- With Max 1 kg, OL 10 kg, unit g and two decimal places, a valid configuration
  was rejected. Max projects to 100000 counts, but the legacy OL field projects
  to 1000000 counts. Legacy projection incorrectly used the six-digit display
  API for a 32-bit compatibility field.

Unit switching also called the full metrology reconfiguration path, resetting
filter and stability history for a display-only change.

## Display and menu correction

The formatter now accepts both `k` and `K` using the seven-segment
approximation `S_C | S_E | S_F | S_G`. This intentionally resembles a compact
lowercase k/h shape; it is distinct from the existing g and lb prompts. Unit
labels are centralized in `DisplayCodes_GetMassUnitLabel()` and shared by the
menu and calibration controller.

Temporary messages are now an overlay over an independent base page. A message
updates only its text and timer. It does not replace the base page, text, or
numeric edit buffer. Expiry only clears `message_active`; `SetPage()` explicitly
cancels a message, and the fault page remains higher priority. Menu indicators
continue to derive from the base page while a message is visible.

The Unit item now follows an explicit editor:

`FUNCTION` enters edit, `STAR` selects the previous enabled unit, `HASH` selects
the next enabled unit, `FUNCTION` confirms, and `TARE` cancels. Selection does
not alter active configuration. Confirmation applies only a changed candidate,
marks RAM configuration dirty, and does not save Flash. Class III reference
mode skips lb. The advanced-menu sequence is recognized only from the unedited
Unit home item.

## Range and compatibility correction

`UnitConverter_MassToCountUnbounded()` shares the exact integer factor,
rounding, and 1/2/5 quantization path with `MassToDisplay()`, but has no
999999/-99999 panel limit. Legacy metrology, stability, calibration-span, and
runtime-tare projection use this unbounded result and apply only their actual
32-bit destination limits.

The six-digit limit still applies wherever a value must be displayed or edited.
Max, zero range, and overload remain independent physical mass parameters. A
1 kg Max does not lower a 10 kg OL. Thus Max 1 kg / OL 10 kg / g / dp=2 is a
valid configuration, while Max 10 kg / g / dp=2 is rejected because Max itself
would require 1000000 display counts. Entering an edit whose current physical
value cannot be represented reports `UnItHI` and does not begin a transaction.

Schema V2 stays 344 bytes and V1 stays 164 bytes. V2 encoding creates projected
copies for its V1 compatibility prefix while preserving canonical 64-bit fields
in the V2 suffix. A/B addresses, CRC32, commit-last, PVD behavior, and Modbus
register addresses are unchanged.

## Lightweight unit switching

`MetrologyManager_SetDisplayUnit()` validates and projects a candidate before
calling `WeightEngine_UpdateDisplayConfig()`. The engine recomputes only legacy
display counts from the existing physical snapshot. It preserves calibration,
last raw sample, raw-valid state, filter buffer and readiness, stability window
and state, stable flag, zero offset, tare state and mass, gross/net/tare masses,
sample timestamp, sample sequence, publication sequence, and faults. No raw
sample is reprocessed. If context application fails, the engine display config
is rolled back.

## Verification

MSVC C11 `/W4 /WX` host build and all five test targets passed:

- Stage4A UI/metrology: k/K and kg/g/lb formatting, overlay replacement and
  expiry, latest base-page restoration, numeric restoration, menu LEDs, fault
  priority, Unit candidate/cancel/confirm flow, ordinary navigation, and the
  advanced entry sequence.
- Stage4B and Stage4B-R: codec/store, A/B, CRC, fault injection, migration, and
  persistence regressions.
- Stage5A: unbounded projection for OL, zero, auto-zero range, calibration span,
  and runtime tare; 1 kg/10 kg and 1 kg/1.1 kg cases; V2 canonical round trip;
  true Max display overflow rejection; register-model regression.
- Stage5B: Modbus CRC/framing/register model, DMA position, RTU timing, and
  RS485 transmit state regressions.
- Engine state test: `kg -> g -> lb -> kg` retains filter, stability,
  calibration, zero/tare, physical masses, timestamp, and sequence.

Clean ARM builds completed with zero compiler warnings and errors:

| Build | FLASH | Delta vs baseline | RAM | Delta vs baseline |
| --- | ---: | ---: | ---: | ---: |
| Debug | 101,440 B | +1,692 B | 10,672 B | +24 B |
| Release | 55,208 B | +856 B | 10,688 B | +24 B |
| BoardDiagnostics | 99,224 B | +1,492 B | 10,664 B | +24 B |

CONFIG_A and CONFIG_B usage remains 0 bytes. The Release load image ends at
approximately `0x0800D7A8`, below the application boundary `0x0801F000` by
71,768 bytes. No dynamic allocation, floating point, FreeRTOS, `HAL_Delay`,
Domain HAL access, or UI direct HAL access was added.

## Hardware regression

The Release image is intended for the following panel regression: edit and
cycle kg/g/lb; cancel and confirm Unit; observe STABLE and tare preservation;
set Max 1.000 kg and OL 10.000 kg; switch to g and set dp=2; confirm 1000.00 g;
verify OL edit reports the range hint in g and is editable again in kg; SAVE,
power-cycle, and compare Modbus values.

On 2026-07-26 the first connection attempts failed before programming with
`Unable to get core ID / No STM32 target found`. After the board was
power-cycled, STM32CubeProgrammer v2.19.0 connected through ST-Link
`E1007200D0D2139393740544` at 3.29 V using Normal mode, 4 MHz SWD, and software
reset. It programmed 53.91 KiB from the final Release ELF, verified the download
successfully, and reset the MCU. The operation erased application sectors 0..53
only; the configuration region was preserved.

A post-flash read-only FC03 probe on COM5, address 1, 115200 N1 passed. It read
Schema V2, active slot B, storage sequence 4, `config_dirty=0`, active unit g,
sample sequence 134, and `power_safe=1`. The report is under
`Tools/stage5b_hw/reports/20260726_141706_rs485/`.

Programming, verification, reset, firmware startup, and the read-only Modbus
probe are **TESTED ON HARDWARE: PASS**. Panel kg/g/lb appearance, key handling,
STABLE/tare behavior, Max/OL/dp editing, SAVE, and a subsequent power-cycle are
still **NOT TESTED ON HARDWARE** until the operator completes the steps above.

The software acceptance gates for entering Stage 5C are satisfied. Actual
panel, scale, power-cycle, and Modbus regression remains a hardware gate, not a
software-test claim.
