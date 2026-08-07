# Stage 5A-DSP-A-HF2-R1 Hardware Regression

Firmware baseline: `5eecbb1d799827fbcc3a82eac7a2be6ae2ecee37`.

PC tooling branch: `stage5a-dsp-a-hf2-r1-hwreg`.

PC tooling commit used for the repeated read-only gate:
`88afab50b541119892723007f1706e1242c6dc97`.

## Current status

- Tool map: `0x0102`.
- Persistent schema: 2.
- Runtime drift default: disabled.
- Firmware was previously programmed and byte-verified over SWD.
- Test PC: Windows 10 build 26200, Python 3.8.10, pyserial 3.5.
- Adapter: CH340, VID:PID `1A86:7523`, COM5, RS485.
- Communication: 115200 baud, 8N1, slave address 1.
- Raw result directory: `Results/hardware/hf2_r1_20260807_144451/` (Git ignored).
- Probe, `0x0203`, full runtime block and 20-read FC03 gate: **PASS**.
- Weighing, panel, drift and 10-minute RS485 regression: **NOT TESTED**.

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
| Probe / identity | PASS | map `0x0102`, firmware `0x050A`, Schema 2, high-word-first |
| `0x0203` and runtime block | PASS | reserved=0, `0200-021D` read, `021D` accepted, `021E` exception 02 |
| Empty | PASS | 23 samples/12 s, stable and LOCKED, no overload |
| 500 g | PASS | 55 stable and LOCKED samples, gross `500.073433 g` |
| 500 g + 1 g | PASS | gross delta `1.024671 g`, anchor delta `1.019023 g`, DEVIATION |
| TARE quick load | PASS | zero anchor, DEVIATION release, final net `1.028061 g` |
| ZERO quick load | PASS + MANUAL PASS | gross `1.034839 g`, DEVIATION release; ZERO key/panel confirmed by operator |
| TARE-active ZERO rejection | PASS + MANUAL PASS | tare remained active; panel displayed `tArE` |
| OL migration | SKIPPED | old 10 kg configuration no longer present and OL is not mapped |
| OL menu | MANUAL PASS | operator confirmed normal editable OL menu |
| SAVE + power cycle | PASS | dirty cleared, revision `11/11`, post-cycle probe passed, OL manually confirmed 3 kg |
| Drift ARMING | PASS | first TRACKING at `299.172 s`, offset stayed 0 during ARMING |
| Drift TRACKING | PASS | 95 s, one 60 s window reset, max observed step 500 ug |
| 1 g drift protection | PASS with limitation | gross delta `1.048396 g`, comp delta equal, offset `-1000 ug` retained; direct FROZEN frame not sampled, ARMING observed |
| TARE-active drift re-arm | PASS | TRACKING after `250.203 s`, tare remained active, offset `-1000 ug` retained |
| CLEAR TARE | PASS | tare inactive, state ARMING, offset `-1000 ug` retained |
| RS485 smoke | PASS | 100 consecutive read-only FC03 requests |
| RS485 10-minute soak | PASS | 8034 requests, 0 timeout, 0 CRC, FIFO overrun `0 -> 0` |

The repeated status snapshot reported flags `0x0000003F`, stable=true,
tare=false, overload=false, CS1237 state 4, CAP=3 kg, runtime drift DISABLED,
and runtime offset 0. The first gate exposed and the second gate verified a PC
tool correction for the legacy status registers: `0004` carries the low 16
bits and `0005` the high 16 bits independently of configured word order.

The soak CSV recorded 600 seconds, sequence growth 6114, response latency
7.80-11.34 ms (mean 8.38 ms), stable=true, display LOCKED, and Runtime Drift
DISABLED with offset 0. The final post-test snapshot is still map `0x0102`,
Schema 2, tare inactive, stable, and Runtime Drift disabled with offset 0.
`config_dirty=true` at the final snapshot because TARE/CLEAR-TARE operations
after the earlier SAVE changed volatile runtime configuration; no second SAVE
was performed.

## Gate decision

`CORE HARDWARE REGRESSION TESTED`: **YES**.

`PROVISIONAL RUNTIME DRIFT BASIC HARDWARE TESTED`: **YES, WITH LIMITATION**.
The direct transient FROZEN frame was not sampled; the physical load step was
observed in both uncompensated and compensated gross, offset was retained, and
the state was observed re-entering ARMING. Exact <=500 ms TARE/ZERO action
timing and direct ZERO key-mask capture remain unverified.

`HF2-R1 HARDWARE REGRESSION COMPLETE`: **YES** for this read-only/low-risk
scope. No dangerous FAULT injection, temperature study, long-term creep
validation, or metrological validation was performed.

Main merge recommendation: **CONDITIONALLY YES** for the HF2-R1 ordinary
weighing and menu fixes, with the Runtime Drift limitations above reviewed.
Runtime Drift remains **DEFAULT DISABLED** and **NOT METROLOGICALLY VALIDATED**.

This remains **PROVISIONAL RUNTIME DRIFT COMPENSATION**, **DEFAULT DISABLED**,
and **NOT METROLOGICALLY VALIDATED**. Host tests are not hardware evidence.
