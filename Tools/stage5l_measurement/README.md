# Stage 5L Measurement Capture

Read-only Modbus RTU acquisition and offline analysis for the frozen `0x0510` / Map `0x0104` firmware.

```powershell
python Tools/stage5l_measurement/stage5l_capture.py capture-baseline --port COM5 --duration-s 600 --output Results/stage5l_characterization/<run>_empty
python Tools/stage5l_measurement/stage5l_capture.py analyze --input Results/stage5l_characterization/<run>_empty
python Tools/stage5l_measurement/stage5l_capture.py validate-manifest --input Results/stage5l_characterization/<run>_empty
```

Capture aliases are `capture-step`, `capture-creep`, `capture-zero-return`, `capture-slow-ramp`, and `capture-rate-compare`. They differ only in the recorded test kind; physical events are explicit `--start-event` and `--end-event` labels. The tool never implements FC06, FC16, mailbox, SAVE, calibration, or Flash operations.

The optional `Stage5LDiagnostics` firmware preset exposes the RAM symbol
`g_stage5l_measurement_control` for SWD-only filter overrides. It does not add
Modbus registers. `Release` defines the feature off and retains the qualified
ELF hash. A restore command reloads the active persistent profile's filter and
restores the pre-override dirty bit before the production image is reflashed.
