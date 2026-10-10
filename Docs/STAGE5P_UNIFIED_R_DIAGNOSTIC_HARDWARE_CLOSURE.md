# Stage 5P Unified-R diagnostic configuration and focused hardware closure

Date: 2026-10-11 (Asia/Shanghai)

## Outcome

The Release diagnostic configuration was corrected and the device-side
resource attempt was executed with a fresh backup. The 0x0524 delivery image
was **not** installed. The resource gate is **INCOMPLETE/FAIL for this run**:
the existing profile-switch command returned `OK`, but repeated device
readback stayed on profile 0 at 10 Hz. No 40 Hz target evidence was accepted.
The diagnostic image was then removed and the exact pre-test 0x0520 image and
configuration were restored byte-for-byte.

## Build correction

The previous failure used Debug `-O0`; its historical log remains at
`Results/stage5p_unified/software/target_resource_diag_build.log`.

The corrected fresh Release build used GCC 13.3.1, `Release` (`-O3 -DNDEBUG`),
strict `-Wall -Wextra -Werror -fstack-usage -fcallgraph-info=su`, the existing
linker script and all delivery feature switches. A diagnostic-only switch was
added:

```text
A33_ENABLE_STAGE5P_UNIFIED_RESOURCES=1
Firmware 0x0525 / Map 0x010D
A33_ENABLE_STAGE5PA13DR_RESOURCES=1
```

The switch is off for the delivery image, which was rebuilt independently and
remained exactly 96,620 B with SHA-256
`CBAB6D5E294D36F62231F9A233DF9E915E9F599D4B4826E6DC993A95035FFBF1`.

Diagnostic image:

- 98,348 B, SHA-256
  `799FAE10059692AF56E8413B59BC4E5F128C1DAAE38E52C75C0DE867C37A28F3`
- FLASH: 98,348 / 126,976 B
- RAM: 19,384 B
- Complete static conservative stack: 1,448 B
- Static collision margin: 672 B
- Host cumulative-contract tests: 10/10

The diagnostic map, ELF, CMake cache, compile commands, stack analysis and
build log are retained below `Results/stage5p_unified/software/`.

## Device preflight and backup

Read-only preflight at `2026-10-10T17:21:04Z` found:

- Firmware `0x0520` / Map `0x0109`
- 10 Hz, filt1, strength3, calibrated
- R5 `ACTIVE+DOSING`, offset `+198,571 µg`
- approximately 500 g on the platform
- Checkweigh OFF, dirty/fault/overrun 0, revision/saved 19/19

Fresh SWD backup was made before any write:

- Application: 126,976 B, SHA-256
  `F85404B5D551EB44CB9B585793CA1935D6D06D7DC4608B9F986956B9FAA84A6B`
- V3 configuration: 4,096 B, SHA-256
  `856BD8F5C14760561FC4BFEC4274FE0C5480C19BC4422046617C2450F8439733`
- Slot A valid/committed, sequence 19, CRC `0x580A10E4`
- Slot B valid/committed, sequence 18, CRC `0x3DEDBEB1`

## Diagnostic install and stop

Only the application region was erased and programmed. The configuration
SHA-256 remained unchanged. Boot readback was `0x0525/0x010D`, OFF+SHADOW,
offset 0, dirty/fault/overrun 0, revision/saved 19/19.

The configured second profile was read as 40 Hz before the test. A new
profile-switch request returned `OK`, but repeated reads after the request
continued to report active profile 0 / 10 Hz. Since no target-side 40 Hz
resource trace could be proved, the run stopped without pretending that
admitted or host polling data represented 40 Hz.

## Exact restoration

The diagnostic image was replaced from the fresh backup. Verification showed:

- Restored application SHA equals the pre-test application SHA above.
- Restored configuration SHA equals the pre-test configuration SHA above.
- Final readback: `0x0520/0x0109`, OFF+SHADOW, offset 0.
- Final revision/saved: 19/19; dirty/fault/overrun: 0/0/0.
- Checkweigh OFF; calibration valid.

## Gate table

| Item | Result |
| --- | --- |
| Release diagnostic build and identity separation | PASS |
| Diagnostic static FLASH/RAM/stack gate | PASS |
| Host regression 47/47 Debug and Release | PASS |
| 40 Hz target resource coverage | INCOMPLETE: profile remained 10 Hz |
| Diagnostic 10 Hz resource coverage | NOT RUN as a qualification substitute |
| 0x0524 accurate flash/Verify | NOT RUN |
| 0x0524 10/40 Hz/filter/load functionality | NOT RUN |
| V3 migration/SAVE/physical power cycle | NOT RUN |
| Menu/PLC/battery UI | NOT RUN |
| Exact rollback and configuration preservation | PASS |
| A13B long-duration drift efficacy | INCONCLUSIVE, unchanged |

The command/profile-switch path must be diagnosed or an explicitly controlled
40 Hz RAM configuration path must be provided before repeating target resource
qualification. No 0x0524 delivery claim, long-term efficacy claim, cross-sensor
claim or metrology certification is made.
