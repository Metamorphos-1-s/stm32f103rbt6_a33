"""Decode and classify V3 battery fields without changing flash images.

The V3 payload has no provenance bit for the alarm enable/thresholds.  This
tool therefore reports an exact legacy-zero tuple and emits a proposed,
explicitly reviewed migration without applying it.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

SLOT_SIZE = 2048
HEADER_SIZE = 32
PAYLOAD_SIZE = 281
BATTERY_OFFSET = 238
BATTERY_SIZE = 25


def decode_battery(slot):
    payload = slot[HEADER_SIZE:HEADER_SIZE + PAYLOAD_SIZE]
    if len(payload) != PAYLOAD_SIZE:
        raise ValueError("truncated V3 payload")
    top, bottom, gain, offset, warning, critical, recovery, enabled = struct.unpack_from(
        "<IIiiIIIB", payload, BATTERY_OFFSET
    )
    return {
        "divider_top_ohm": top,
        "divider_bottom_ohm": bottom,
        "calibration_gain_ppm": gain,
        "calibration_offset_mv": offset,
        "low_warning_mv": warning,
        "critical_low_mv": critical,
        "recovery_mv": recovery,
        "low_voltage_alarm_enable": bool(enabled),
    }


def inspect(path):
    data = Path(path).read_bytes()
    if len(data) != 4096:
        raise ValueError("expected complete 4096-byte V3 region")
    slots = []
    for index in range(2):
        slot = data[index * SLOT_SIZE:(index + 1) * SLOT_SIZE]
        sequence = struct.unpack_from("<I", slot, 12)[0]
        battery = decode_battery(slot)
        legacy_zero = (
            battery["calibration_gain_ppm"] == 0 and
            battery["calibration_offset_mv"] == 0 and
            battery["low_warning_mv"] == 0 and
            battery["critical_low_mv"] == 0 and
            battery["recovery_mv"] == 0 and
            not battery["low_voltage_alarm_enable"]
        )
        slots.append({"slot": "AB"[index], "sequence": sequence,
                      "battery": battery,
                      "legacy_zero_tuple": legacy_zero})
    return {"sha256": hashlib.sha256(data).hexdigest().upper(), "slots": slots}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("config_region", type=Path)
    parser.add_argument("--proposed-offset-mv", type=int, default=400)
    parser.add_argument("--proposed-warning-mv", type=int, default=13600)
    parser.add_argument("--proposed-critical-mv", type=int, default=13200)
    parser.add_argument("--proposed-recovery-mv", type=int, default=13900)
    args = parser.parse_args()
    result = inspect(args.config_region)
    result["migration_policy"] = {
        "automatic": False,
        "reason": "V3 has no provenance bit distinguishing legacy defaults from an intentional alarm-off setting",
        "requires_explicit_operator_confirmation": True,
        "proposed_unified_values": {
            "calibration_offset_mv": args.proposed_offset_mv,
            "low_warning_mv": args.proposed_warning_mv,
            "critical_low_mv": args.proposed_critical_mv,
            "recovery_mv": args.proposed_recovery_mv,
            "low_voltage_alarm_enable": True,
        },
    }
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
