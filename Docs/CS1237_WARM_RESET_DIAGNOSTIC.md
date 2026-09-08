# CS1237 warm-reset diagnostic

Date: 2026-09-08

Firmware commit: `6a258ef2491d1ed3d0082acae4355ec7caf492c7`

Release ELF SHA-256:
`DD17C01319A5DCDF861074F23E5B85A32E16F3B413670FB1814C7180EE7E7F93`

## Scope and controls

This investigation compared a complete cold start, an ST-Link software reset,
an NRST hardware reset, and a complete physical power cycle. The scale platform
was left unchanged. All measurement captures used only Modbus FC03 on COM5 at
115200 8N1, Unit ID 1. No SAVE, configuration write, calibration, factory reset,
Option Byte operation, mass erase, or measurement command was issued.

Before programming, both 2 KiB configuration slots at `0x0801F000` were uploaded
as one 4 KiB image. The same region was uploaded after programming, after the D
power cycle, and after manual keypad checks. Every image had SHA-256:

`43024A3382D5A01644700ADD2DD4759418B07DB1F6FF4D1C68D7796C89999AA7`

The Release ELF was programmed once to application sectors 0..79 and verified
by STM32CubeProgrammer 2.19.0. The configuration sectors were not erased.

## Reset comparison

Each formal group contains at least 60 seconds of continuous read-only samples.
CS1237 state 4 is `CS1237_STATE_RUNNING`. Profile/rate/gain values 0/0/3 are the
high-precision profile, 10 Hz and gain 128.

| Group | Reset boundary | Samples / duration | Raw mean / span | Filtered mean / span | Display mass range | Sequence | CS1237 / overrun / fault |
|---|---|---:|---:|---:|---:|---:|---|
| A | Complete cold start | 172 / 60.094 s | -488000.60 / 54 | -488000.09 / 46 | 500158821..500158821 ug | 270..866 | RUNNING / 0 / 0 |
| B | ST-Link software reset, no programming | 172 / 60.157 s | -487991.33 / 70 | -487990.59 / 41 | 500109260..500109260 ug | 319..916 | RUNNING / 0 / 0 |
| C | NRST `-hardRst` | 172 / 60.156 s | -488001.41 / 55 | -488000.82 / 36 | 500107007..500107007 ug | 163..759 | RUNNING / 0 / 0 |
| D | Complete physical power cycle | 173 / 60.344 s | -487999.32 / 82 | -487998.68 / 64 | 500165579..500165579 ug | 184..782 | RUNNING / 0 / 0 |

A preliminary 60.047-second post-programming warm observation also remained
stable: raw span 52, filtered span 45, fixed display mass 500057446 ug,
sequence 2286..2882, RUNNING, zero overrun and zero fault.

Every formal group retained the same active profile, sample rate, gain,
communication readback, revision semantics, runtime-drift state, and startup
auto-zero terminal tuple. Sample sequences advanced monotonically within each
boot. The largest difference between the four fixed displayed masses was
58,572 ug (0.058572 g); no group showed rapid display changes.

Register Map `0x0104` does not expose `CS1237_GetReadErrorCount()`. Consequently,
the internal read-error counter is unavailable in this FC03 evidence and is not
claimed to be zero. The exposed CS1237 state, buffer overrun and fault mask were
stable and clean.

## Keypad verification

The operator confirmed the physical nested interaction after programming:

- STAR short had no effect and showed no success message.
- STAR long entered directly at the first STATUS label `FIr`; there is no
  separate STATUS splash page.
- The entry hold did not advance the item before STAR release.
- LIST remained on `FIr` instead of automatically replacing the label with a
  value.
- FUNCTION entered VIEW/EDIT, parameter-level TARE returned to the same label,
  and list-level TARE exited STATUS.
- Menu edit-level TARE returned to the menu list; a second list-level TARE
  exited the menu.
- The complete manual sequence conformed and issued no SAVE.

## Conclusion

The reported fast weight changes were not reproduced. Software reset and NRST
did not produce behavior distinct from cold start or complete power cycle in
raw, filtered, or displayed values. Available evidence therefore does not
support the classification `CS1237 warm-reset synchronization defect`.

No CS1237 driver, filter, DisplayConditioner, calibration, gain, rate, or
metrology change was made. A power-off delay state must not be added without a
future reproduction that demonstrates a reset-dependent raw-data failure and
confirms that AD_EN low resets the external ADC on the actual board.
