#!/usr/bin/env python3
"""Consolidate Stage 5N-A hardware SHADOW evidence."""

import argparse
import csv
import json
from pathlib import Path


FORMAL = ("formal_state", "formal_alarm_active", "formal_green",
    "formal_yellow", "formal_red", "formal_internal_buzzer",
    "formal_external_buzzer")


def rows(path):
    with path.open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def integer(row, key):
    return int(row[key])


def first_crossing(data, predicate, start=1):
    return next((index for index in range(start, len(data))
        if predicate(integer(data[index - 1], "dynamic_input_ug"),
                     integer(data[index], "dynamic_input_ug"))), None)


def first_class(data, target, start):
    return next((index for index in range(start, len(data))
        if integer(data[index], "dynamic_confirmed") == target), None)


def transition(data, reference_index, target):
    if reference_index is None:
        return {"present": False, "delay_ms": None}
    confirmed = first_class(data, target, reference_index)
    return {"present": confirmed is not None,
        "reference_sequence": integer(data[reference_index], "sample_sequence"),
        "confirmed_sequence": None if confirmed is None else
            integer(data[confirmed], "sample_sequence"),
        "delay_ms": None if confirmed is None else
            integer(data[confirmed], "timestamp_ms") -
            integer(data[reference_index], "timestamp_ms")}


def safety(data):
    return {"formal_nonzero_records": sum(any(integer(row, key) != 0
        for key in FORMAL) for row in data),
        "fault_max": max(integer(row, "fault") for row in data),
        "overrun_max": max(integer(row, "overrun") for row in data),
        "dirty_max": max(integer(row, "dirty") for row in data),
        "save_max": max(integer(row, "save_count") for row in data),
        "revision_values": sorted(set(integer(row, "revision") for row in data)),
        "saved_revision_values": sorted(set(integer(row, "saved_revision") for row in data))}


def coverage(data):
    expected = integer(data[-1], "sample_sequence") - \
        integer(data[0], "sample_sequence") + 1
    return {"records": len(data), "expected": expected,
        "coverage": len(data) / expected,
        "maximum_sequence_gap": max(integer(data[index], "sample_sequence") -
            integer(data[index - 1], "sample_sequence")
            for index in range(1, len(data)))}


def cycle(root, index, kind):
    path = root / ("cycle%d_%s" % (index, kind)) / "samples.csv"
    data = rows(path); low = 100000000; high = 400000000
    if kind == "load":
        ok_ref = first_crossing(data, lambda before, now:
            before < low <= now)
        high_ref = first_crossing(data, lambda before, now:
            before <= high < now)
        if ok_ref == high_ref and ok_ref is not None:
            transitions = {"low_to_high_direct": transition(data, high_ref, 4)}
        else:
            transitions = {"low_to_ok": transition(data, ok_ref, 3),
                "ok_to_high": transition(data, high_ref, 4)}
    else:
        ok_ref = first_crossing(data, lambda before, now:
            before > high >= now)
        low_ref = first_crossing(data, lambda before, now:
            before >= low > now)
        if ok_ref == low_ref and ok_ref is not None:
            transitions = {"high_to_low_direct": transition(data, low_ref, 2)}
        else:
            transitions = {"high_to_ok": transition(data, ok_ref, 3),
                "ok_to_low": transition(data, low_ref, 2)}
    return {"path": path.parent.name, "coverage": coverage(data),
        "transitions": transitions, "safety": safety(data),
        "final_static": integer(data[-1], "static_class"),
        "final_dynamic": integer(data[-1], "dynamic_confirmed")}


def main():
    parser = argparse.ArgumentParser(); parser.add_argument("--input", type=Path,
        required=True); parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(); cycle_results = []
    for index in range(1, 6):
        cycle_results.extend((cycle(args.input, index, "load"),
            cycle(args.input, index, "unload" if index > 1 else
                "unload_valid")))
    delays = [transition["delay_ms"] for run in cycle_results
        for transition in run["transitions"].values()
        if transition["delay_ms"] is not None]
    all_safety = [run["safety"] for run in cycle_results]
    dosing = rows(args.input / "dosing_pause_30s" / "samples.csv")
    zero = rows(args.input / "zero_empty" / "samples.csv")
    equal = rows(args.input / "equal_limits_5s" / "samples.csv")
    invalid = rows(args.input / "invalid_limits_5s" / "samples.csv")
    result = {"schema_version": 1,
        "classification": "NEW HARDWARE HOLDOUT; PARTIAL PHYSICAL COVERAGE",
        "cycles": cycle_results, "cycle_count": 5,
        "maximum_dynamic_delay_ms": max(delays),
        "median_dynamic_delay_ms": sorted(delays)[len(delays) // 2],
        "transition_count": len(delays),
        "missed_transitions": sum(not transition["present"]
            for run in cycle_results for transition in run["transitions"].values()),
        "formal_nonzero_records": sum(item["formal_nonzero_records"]
            for item in all_safety),
        "cycle_safety_pass": all(item["fault_max"] == 0 and
            item["overrun_max"] == 0 and item["dirty_max"] == 0 and
            item["save_max"] == 0 and item["revision_values"] == [8] and
            item["saved_revision_values"] == [8] for item in all_safety),
        "dosing": {"records": len(dosing),
            "static_valid": sum(integer(row, "static_class") in (2, 3, 4)
                for row in dosing),
            "static_pending": sum(integer(row, "static_class") == 1
                for row in dosing),
            "process_active": sum(integer(row, "process_active") == 1
                for row in dosing), "safety": safety(dosing)},
        "zero": {"records": len(zero),
            "reset_pending_records": sum(integer(row, "static_reason") == 11
                for row in zero), "safety": safety(zero)},
        "equal_limits": {"records": len(equal),
            "valid_records": sum(integer(row, "static_class") in (2, 3, 4)
                for row in equal), "safety": safety(equal)},
        "invalid_limits": {"records": len(invalid),
            "valid_records": sum(integer(row, "static_class") in (2, 3, 4) or
                integer(row, "dynamic_confirmed") in (2, 3, 4)
                for row in invalid), "safety": safety(invalid)},
        "not_run": ["slow physical fill with pauses",
            "mechanical disturbance", "physical TARE", "physical CLEAR TARE",
            "fault/invalid physical injection"],
        "host_covered": ["TARE reset", "CLEAR TARE reset", "fault",
            "overload", "calibration", "sequence gap", "timestamp anomaly"],
        "qualification": "SHADOW HARDWARE QUALIFICATION INCOMPLETE",
        "active_output_authorized": False}
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2)); return 0


if __name__ == "__main__": raise SystemExit(main())
