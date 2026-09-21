#!/usr/bin/env python3
"""Analyze Stage 5N-A3 target invalid-injection and recovery evidence."""

import argparse
import csv
import hashlib
import json
from pathlib import Path


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def rows(path):
    with Path(path).open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def formal_nonzero(row):
    return any(int(row[key]) for key in ("formal_state", "formal_alarm_active",
        "formal_green", "formal_yellow", "formal_red",
        "formal_internal_buzzer", "formal_external_buzzer"))


def capture_summary(directory):
    directory = Path(directory)
    data = rows(directory / "samples.csv")
    invalid = [row for row in data if row["static_reason"] == "2" or
               row["dynamic_reason"] == "2"]
    return {
        "records": len(data),
        "read_errors": json.loads((directory / "summary.json").read_text(
            encoding="utf-8"))["read_errors"],
        "invalid_observation_records": len(invalid),
        "first_invalid_sequence": None if not invalid else
            int(invalid[0]["sample_sequence"]),
        "last_invalid_sequence": None if not invalid else
            int(invalid[-1]["sample_sequence"]),
        "formal_output_nonzero_records": sum(formal_nonzero(row) for row in data),
        "fault_max": max(int(row["fault"]) for row in data),
        "overrun_max": max(max(int(row["overrun"]), int(row["overrun_count"]))
                           for row in data),
        "dirty_max": max(int(row["dirty"]) for row in data),
        "save_max": max(int(row["save_count"]) for row in data),
        "revision_values": sorted(set(int(row["revision"]) for row in data)),
        "saved_revision_values": sorted(set(int(row["saved_revision"])
                                             for row in data)),
        "r5_offset_values": sorted(set(int(row["offset_ug"]) for row in data)),
        "r5_reference_values": sorted(set(int(row["reference_ug"]) for row in data)),
        "r5_rebase_values": sorted(set(int(row["automatic_rebase_count"])
                                        for row in data)),
        "sequence_advanced": int(data[-1]["sample_sequence"]) >
                             int(data[0]["sample_sequence"]),
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    args = parser.parse_args()
    root = args.root
    auto = json.loads((root / "invalid_auto_high" / "swd_command" /
                       "result.json").read_text(encoding="utf-8"))
    start = json.loads((root / "invalid_abort_high" / "swd_start" /
                        "result.json").read_text(encoding="utf-8"))
    abort = json.loads((root / "invalid_abort_high" / "swd_abort" /
                        "result.json").read_text(encoding="utf-8"))
    auto_capture = capture_summary(root / "invalid_auto_high")
    abort_capture = capture_summary(root / "invalid_abort_high")
    pre_config = root / "preflash_config" / "config_region.bin"
    post_config = root / "postrestore_config" / "config_region.bin"
    recovery = root.parent / "frozen_0515_recovery.bin"
    restored = root / "postrestore_0515_application.bin"
    page103 = (root / "postrestore_page103.bin").read_bytes()
    active = start["after"]
    aborted = abort["after"]
    automatic = auto["after"]
    gates = {
        "automatic_command_accepted": automatic["applied_sequence"] ==
                                      auto["sequence"],
        "automatic_timeout_completed": automatic["status"] == 2 and
            automatic["completion_reason"] == 1 and automatic["active"] == 0,
        "automatic_injected_samples": automatic["injected_sample_count"] >= 1,
        "active_snapshot_invalid": active["active"] == 1 and
            active["last_input_valid"] == 0 and active["last_static_class"] == 0 and
            active["last_dynamic_class"] == 0,
        "abort_completed": aborted["status"] == 3 and
            aborted["completion_reason"] == 2 and aborted["active"] == 0,
        "abort_recovered_high": aborted["last_input_valid"] == 1 and
            aborted["last_static_class"] == 4 and aborted["last_dynamic_class"] == 4,
        "main_loop_and_modbus_continued": auto_capture["sequence_advanced"] and
                                         abort_capture["sequence_advanced"],
        "formal_outputs_zero": auto_capture["formal_output_nonzero_records"] == 0 and
                               abort_capture["formal_output_nonzero_records"] == 0,
        "safety_clean": all(value == 0 for value in (
            auto_capture["fault_max"], auto_capture["overrun_max"],
            auto_capture["dirty_max"], auto_capture["save_max"],
            abort_capture["fault_max"], abort_capture["overrun_max"],
            abort_capture["dirty_max"], abort_capture["save_max"])),
        "configuration_unchanged": pre_config.read_bytes() == post_config.read_bytes(),
        "application_restored": recovery.read_bytes() == restored.read_bytes(),
        "diagnostic_tail_erased": all(value == 0xFF for value in page103),
    }
    result = {
        "schema_version": 1,
        "result": "PASS" if all(gates.values()) else "FAIL",
        "scope": "TARGET INVALID_INPUT INJECTION; NOT PHYSICAL SENSOR FAULT",
        "automatic_restore": {
            "requested_duration_ms": auto["duration_ms"],
            "target_final": automatic,
            "modbus_capture": auto_capture,
            "host_disconnected_for_injection_duration": True,
        },
        "abort_restore": {
            "active_snapshot": active,
            "abort_snapshot": aborted,
            "modbus_capture": abort_capture,
        },
        "engineering_modbus_note": "During suppression, dynamic_reason reports INPUT while the frozen diagnostic dynamic_confirmed field retains internal hysteresis state; A3 last_dynamic_class records the actual CheckweighShadow output INVALID.",
        "configuration": {
            "before_sha256": sha256(pre_config),
            "after_sha256": sha256(post_config),
            "byte_identical": pre_config.read_bytes() == post_config.read_bytes(),
            "v3_slots": "A sequence 7 valid; B sequence 8 valid and active",
        },
        "recovery": {
            "expected_sha256": sha256(recovery),
            "readback_sha256": sha256(restored),
            "byte_identical": recovery.read_bytes() == restored.read_bytes(),
            "page103_all_ff": all(value == 0xFF for value in page103),
        },
        "gates": gates,
    }
    (root / "p3_analysis.json").write_text(json.dumps(result, indent=2) + "\n",
                                            encoding="utf-8")
    print(json.dumps({"result": result["result"], "gates": gates}, indent=2))
    return 0 if result["result"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
