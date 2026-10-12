# Stage 5P Unified-R10C battery display/alarm root-cause audit

Date: 2026-10-12 Asia/Shanghai

## Current read-only state

Baseline HEAD is `50830efe2f9adfb96205c7a4ae844f24edbacc9e` on
`stage5p-unified-r10c-panel-observation`. The worktree retains the existing
user `.pyc` modifications. A fresh COM5 read at
`2026-10-12T14:18:08.639650Z` reported firmware `0x0524`, Map `0x010C`,
10 Hz/profile 0, filt1/strength3, valid calibration, revision/saved `20/20`,
dirty/fault/overrun `0/0/0`, R5 ACTIVE+STATIC, Checkweigh OFF and volatile
offset `-144028 ug`. The public probe has no battery configuration fields,
so these values are current-device evidence only for the exposed fields.

## Historical V3 evidence

The archived complete V3 region from the R9 session is
`Results/stage5p_unified/hardware/stage5r9_20261011/config_before.bin`,
SHA-256 `856BD8F5C14760561FC4BFEC4274FE0C5480C19BC4422046617C2450F8439733`.
Its active sequence 19 slot (A; slot B is sequence 18) decodes to:

| Field | Sequence 19 value |
| --- | ---: |
| Divider | 47000 / 10000 ohm |
| Gain | 0 ppm |
| Calibration offset | 0 mV |
| Low warning / critical / recovery | 0 / 0 / 0 mV |
| Low-voltage alarm enable | false |

This exactly explains a corrected value at or below 12.8 V displaying `0P`
and no low-voltage warning: the percentage and warning are separate paths,
and the warning path is disabled. It does **not** prove that sequence 20 has
the same battery bytes. The R9/R10 tracked evidence contains no complete
sequence-20 V3 image or public Modbus battery-field readback; sequence 20 is
therefore **not confirmed** here.

The field offsets and classification are implemented in
`Tools/stage5_unified/audit_battery_config.py`; it is read-only and prints a
proposed value set without modifying the image. Running it against the
archived region identifies both slots as the legacy-zero battery tuple.

## Root cause and minimal-safe fix

The firmware defaults set `+400 mV`, warning `13.6 V`, critical `13.2 V`,
recovery `13.9 V`, and alarm enabled only when the V3 configuration is absent
or factory defaults are explicitly selected. Existing valid V3 configuration
has precedence, so application flashing does not replace these fields. The
observed behavior is therefore consistent with an old saved configuration,
not a defect in `BatteryAdc_ApproxPercent()`.

An automatic “all-zero means old default” migration is unsafe: V3 stores no
provenance bit, so the same tuple could be an intentional user choice to keep
the alarm disabled. This audit deliberately does **not** implement or invoke
an unconditional migration, does not change firmware, and does not write
configuration. The safe correction is an explicit operator-approved battery
configuration transaction that changes only the five battery fields above,
preserving calibration, communication, unit, filter, R5 and checkweigh. The
transaction must be followed by the existing explicit SAVE and power-cycle
verification; cancellation leaves the old alarm-off configuration untouched.

If a product requirement later mandates automatic migration, a new V3 schema
or explicit provenance marker is required first. A heuristic based only on
zero values must not silently overwrite user settings.

## Behavior and verification status

| Item | Result |
| --- | --- |
| Historical legacy-zero fields | CONFIRMED for sequence 19 only |
| Current sequence 20 battery fields | NOT CONFIRMED: no public field/readback or tracked full V3 image |
| `0P` arithmetic at <=12.8 V | CONFIRMED by source review |
| Low warning enable/30 s confirmation | CONFIRMED by source review; requires enable and 60 x 500 ms samples |
| `Lo-bAt` placement | CONFIRMED by source: transiently on NET/GROSS, not battery page |
| 12.7 V physical reproduction | NOT RUN in this read-only stage |
| Explicit migration/SAVE/power-cycle | NOT RUN; requires user authorization and current V3 backup |
| Host/ARM product rebuild | NOT RUN; no product source changed |

No R5 algorithm, parameters, calibration, configuration format, 500 g,
40 Hz, long-duration drift or metrology qualification was touched.
