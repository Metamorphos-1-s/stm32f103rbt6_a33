# Stage 5L Measurement Observability Audit

Stage 5L starts at frozen Stage 5K commit `61ab01330c19ec2f01f7af0f3723ce055ce931a8`. Production algorithms and public Map `0x0104` remain unchanged.

## Actual processing chain

```text
CS1237 IRQ/sample buffer (raw int32, driver timestamp)
  -> MeasurementBridge_Process / RawMeasurement_Accept
  -> MetrologyManager_AcceptRawSample
  -> WeightEngine_ProcessRawSample
     -> WeightFilter_Process(raw -> filtered_raw)
     -> CalibrationModel_ConvertMass(filtered_raw, calibration, zero_offset_raw)
     -> uncompensated_gross_mass_ug
     -> RuntimeDriftCompensator (subtract drift offset)
     -> gross_mass_ug
     -> Zero/Tare (subtract tare for net_mass_ug)
     -> StabilityDetector(net mass)
     -> UnitConverter (division/rounding compatibility values)
  -> DisplayConditioner(authoritative gross/net mass -> held panel mass)
  -> UnitConverter_MassToDisplay(panel mass -> final display count)
  -> checkweigh/alarm from weight snapshot and configured source
```

This order follows `measurement_bridge.c`, `metrology_manager.c`, `weight_engine.c`, `display_conditioner.c`, and `modbus_register_model.c`. Calibration is applied after filtering, not directly to each unfiltered raw sample. Runtime drift is subtracted before tare/net derivation. Stability is evaluated on net mass before the current sample's drift-compensator update; display conditioning follows the completed weight snapshot.

## Existing observability

| Signal | Source | Public access | Assessment |
|---|---|---|---|
| CS1237 raw 24-bit code | `CS1237_Sample.raw` / snapshot `raw_value` | `0x001C..0x001D` | Available |
| sample time / sequence | weight snapshot | `0x0022..0x0023`, `0x0020..0x0021` | Available; timestamp is MCU sample time and serves as measurement uptime |
| buffered / overrun | CS1237 driver | `0x002C`, `0x002D..0x002E` | Available |
| filtered raw | weight filter | `0x001E..0x001F` | Available |
| calibrated unfiltered mass | not retained | none | Missing; can only be estimated offline when zero offset is known |
| uncompensated gross | calibration of filtered raw, before runtime drift | `0x0208..0x020B` | Available |
| compensated gross / net / tare | weight engine | `0x0014..0x001B`, `0x0010..0x0013` | Available |
| pre-display-quantization mass | authoritative net/gross and conditioner mass | net/gross above; `0x01E2..0x01E5` | Available |
| final display | unit conversion after conditioner | `0x0000..0x0001` | Available |
| zero compensation raw offset | `WeightEngine.zero_tare.zero_offset_raw` | none | Missing |
| runtime drift offset | runtime drift snapshot | `0x0204..0x0207` | Available |
| filter mode / level | active profile | `0x0122/0x0123` or `0x0130/0x0131` | Available |
| stable / spread | status flags and StabilityDetector | `0x0004..0x0005`, `0x0024..0x0027` | Available; range, not variance |
| rate / gain / driver state | active profile and CS1237 | `0x0029..0x002B` | Available |
| battery voltage | no Map source | none | Missing |
| fault mask | FaultManager | `0x0039..0x003A` high-word first | Available |

## Collection decision

The existing registers are sufficient for the first 10 Hz baselines, creep, return-to-zero, repeated steps, and display/filter lag analysis. Stage 5L therefore begins without a diagnostic firmware build. Each primary FC03 read captures `0x0000..0x003F` as one register-model snapshot. Display-condition and runtime-drift blocks are sampled less frequently and joined by the closest sample sequence/time.

Limitations must remain explicit: `raw_calibrated_mass_ug` is an offline estimate only when `zero_offset_raw` is known; otherwise it is blank. Battery voltage is blank. Polling gaps describe host observation coverage and are not automatically classified as CS1237 overruns; the driver overrun counter is the authoritative device-side loss indicator.

