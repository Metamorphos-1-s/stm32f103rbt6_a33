# Stage 5A-DSP-A-HF2-R1 Hardware Regression

Firmware baseline: `5eecbb1d799827fbcc3a82eac7a2be6ae2ecee37`.

PC tooling is maintained on branch `stage5a-dsp-a-hf2-r1-hwreg`. The exact
tool commit and generated result directory must be copied into this document
only after an actual hardware run.

## Current status

- Tool map: `0x0102`.
- Persistent schema: 2.
- Runtime drift default: disabled.
- Firmware was previously programmed and byte-verified over SWD.
- Current serial enumeration contains no USB-RS485 adapter.
- Probe, `0x0203`, weighing, panel, drift and RS485 regression: **NOT TESTED**.

No MCU firmware source, Schema V2, Flash layout or CubeMX-generated source is
changed by the hardware tooling work.

## Required setup

Connect USB-RS485 A to board A, B to B, and GND to board GND. Do not connect
the adapter 5 V output. ST-Link may remain connected, with all grounds common.
Do not connect RS232 and RS485 to the shared channel at the same time. Change
physical interface wiring only with board power off.

Run the read-only gate first:

```powershell
python Tools\stage5b_hw\stage5b_hw.py hf2-r1-regression --port auto --interface rs485 --non-interactive-readonly
```

The automatic HF2-R1 path stops if map version is not `0x0102`, if `0x0203`
is nonzero, or if the `0x021D/0x021E` boundary is wrong. Ordinary `read` remains
available for diagnosing an unknown firmware map.

## Result record

Fill this section only from generated evidence and operator observations:

| Item | Result | Evidence |
|---|---|---|
| Probe / identity | NOT TESTED | no USB-RS485 port |
| `0x0203` and runtime block | NOT TESTED | no USB-RS485 port |
| Empty / 500 g / +1 g | NOT TESTED | pending |
| TARE quick load | NOT TESTED | pending |
| ZERO quick load | NOT TESTED | pending |
| TARE-active ZERO rejection | NOT TESTED | pending manual observation |
| OL migration / menu / SAVE | NOT TESTED | pending |
| Drift ARMING / TRACKING | NOT TESTED | pending |
| 1 g drift protection | NOT TESTED | pending |
| TARE-active drift re-arm | NOT TESTED | pending |
| RS485 smoke / 10-minute soak | NOT TESTED | pending |

This remains **PROVISIONAL RUNTIME DRIFT COMPENSATION**, **DEFAULT DISABLED**,
and **NOT METROLOGICALLY VALIDATED**. Host tests are not hardware evidence.
