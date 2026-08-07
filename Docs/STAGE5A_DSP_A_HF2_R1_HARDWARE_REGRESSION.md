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
| Empty / 500 g / +1 g | NOT TESTED | pending |
| TARE quick load | NOT TESTED | pending |
| ZERO quick load | NOT TESTED | pending |
| TARE-active ZERO rejection | NOT TESTED | pending manual observation |
| OL migration / menu / SAVE | NOT TESTED | pending |
| Drift ARMING / TRACKING | NOT TESTED | pending |
| 1 g drift protection | NOT TESTED | pending |
| TARE-active drift re-arm | NOT TESTED | pending |
| RS485 smoke | PASS | 20 consecutive read-only FC03 requests |
| RS485 10-minute soak | NOT TESTED | pending |

The repeated status snapshot reported flags `0x0000003F`, stable=true,
tare=false, overload=false, CS1237 state 4, CAP=3 kg, runtime drift DISABLED,
and runtime offset 0. The first gate exposed and the second gate verified a PC
tool correction for the legacy status registers: `0004` carries the low 16
bits and `0005` the high 16 bits independently of configured word order.

This remains **PROVISIONAL RUNTIME DRIFT COMPENSATION**, **DEFAULT DISABLED**,
and **NOT METROLOGICALLY VALIDATED**. Host tests are not hardware evidence.
