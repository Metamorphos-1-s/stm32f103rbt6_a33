# Stage 5P Unified-R10C battery fix execution

Date: 2026-10-12 Asia/Shanghai
Branch: `stage5p-unified-r10c-battery-audit`
Commit at start: `2eddd0c08a76dc79aa357428d864221f5513ad0c`

## Pre-reset state

COM5 read at `2026-10-12T14:36:22.687463Z` reported `0x0524 / 0x010C`,
10 Hz/filt1/strength3, valid calibration, R5 ACTIVE+STATIC, Checkweigh OFF,
revision/saved `20/20`, dirty/fault/overrun `0/0/0`, and volatile offset
`-151563 ug`. This is the pre-SWD runtime evidence.

## SWD backup and current V3 decode

The read-only SWD dump used HotPlug and was performed before any configuration
write. It contains the complete 126,976-byte application region and 4,096-byte
V3 region:

| Artifact | SHA-256 |
| --- | --- |
| `swd/application_region.bin` | `2FFDE2F34986566D0923229EB99C7F50A2D4B1EAEBE9FF307C03A395A7EDB99B` |
| `swd/config_region.bin` | `86A3CFC29E0567FD78A6597E5A221AC72D2CF274E8C8D9888EC77011D3D54785` |

Both V3 slots are valid, committed, CRC-clean and schema 3. Slot A is
sequence 19; slot B is the active sequence 20. The active sequence 20 battery
fields are:

| Field | Value |
| --- | ---: |
| Divider | 47000 / 10000 ohm |
| Calibration gain/offset | 0 ppm / 0 mV |
| Low warning / critical / recovery | 0 / 0 / 0 mV |
| Alarm enable | false |

Sequence 19 contains the same tuple. This is a **current sequence 20
readback**, not a historical inference. Slot-level CRC, commit and sequence
details are in `parsed_slots.json`; the field decode is in
`current_battery_decode.json`.

The post-SWD COM5 probe at `2026-10-12T14:38:19.808861Z` still reported
`0x0524`, revision/saved `20/20`, dirty/fault/overrun `0/0/0`, R5
ACTIVE+STATIC and offset `-152903 ug`. No offset reset was observed in this
HotPlug read; the application uptime remained monotonic. The accepted SWD
operation therefore produced no claim of a power-cycle or offset clearing.

## Root cause

`BatteryAdc_ApproxPercent()` intentionally returns 0% at corrected voltage
`<= 12800 mV`, hence `0P`. The warning path is separately gated by
`low_voltage_alarm_enable` and requires 60 consecutive 500-ms samples at or
below `low_warning_mv`. With the current sequence 20 values (alarm disabled,
threshold zero), no warning can be confirmed. `Lo-bAt` is only a transient
NET/GROSS-page overlay; the battery percentage page is not required to show
it.

The 12.7-V student-supply physical behavior was not re-run in this pass. The
configuration evidence is sufficient to explain the reported symptom, but
does not substitute for the later authorized three-level visual test.

## Safe correction boundary

The existing 0x0524 public configuration/mailbox path has no individual
battery-field editor. It can apply/save a complete `DeviceConfig`, but the
host contract cannot read or set these battery members without a new firmware
interface. Directly editing the V3 bytes is explicitly prohibited.

Accordingly, this run stops before configuration modification. The minimum
safe implementation is a new, separately identified firmware/host transaction
that:

1. reads the full current configuration and preserves every non-battery field;
2. requires an explicit operator confirmation for the five target values
   (`+400`, `13600`, `13200`, `13900`, enabled);
3. applies only the battery struct in RAM, validates it, and performs one
   explicit SAVE;
4. verifies the new active slot, CRC/commit, revision and field-by-field
   preservation, then performs the authorized power-cycle check.

No new firmware was built or flashed and no configuration was written in this
run. Implementing that interface requires a new firmware identity and a
separate software/ARM/RAM gate; it must not reuse the frozen `0x0524` SHA.

## Qualification status

| Item | Result |
| --- | --- |
| Current sequence 20 battery fields | PASS: direct SWD readback |
| Root-cause attribution | PASS: legacy battery tuple explains symptom |
| Normal/12.7 V/recovery physical visual test | NOT RUN |
| Battery configuration correction/SAVE | NOT RUN: no safe individual-field interface |
| Power-cycle after correction | NOT RUN |
| R5/500 g/40 Hz/long drift/resource qualifications | NOT RUN by scope |

The device remains on the existing `0x0524` image and configuration, with
R5 ACTIVE+STATIC and the volatile offset observed above. No claim is made
that the low-voltage UI has been physically corrected until the explicit
transaction is implemented and authorized.
