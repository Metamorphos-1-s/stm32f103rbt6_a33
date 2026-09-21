#!/usr/bin/env python3
"""Analyze Stage 5N-A2 physical SHADOW qualification evidence."""

import argparse
import csv
import json
import statistics
from pathlib import Path

NAMES = {0: "INVALID", 1: "PENDING", 2: "LOW", 3: "OK", 4: "HIGH"}
VALID_CLASSES = {2, 3, 4}
HYSTERESIS_UG = 20000


def read_rows(path):
    with Path(path).open(encoding="utf-8", newline="") as stream:
        rows = list(csv.DictReader(stream))
    integer_fields = set(rows[0]) - {"utc", "firmware", "map", "signature"}
    for row in rows:
        for field in integer_fields:
            if row[field] != "":
                row[field] = int(row[field])
    return rows


def read_events(path):
    with Path(path).open(encoding="utf-8") as stream:
        return [json.loads(line) for line in stream if line.strip()]


def percentile(values, percentile_value):
    if not values:
        return 0.0
    ordered = sorted(values)
    index = round((len(ordered) - 1) * percentile_value)
    return ordered[index]


def formal_nonzero(row):
    return any(row[key] for key in ("formal_state", "formal_alarm_active",
        "formal_green", "formal_yellow", "formal_red",
        "formal_internal_buzzer", "formal_external_buzzer"))


def outside_hysteresis_contract(row):
    if row["dynamic_reason"] == 11:
        return False
    state = row["dynamic_confirmed"]
    weight = row["dynamic_input_ug"]
    low = row["low_limit_ug"]
    high = row["high_limit_ug"]
    if state == 2:
        return weight >= low + HYSTERESIS_UG
    if state == 3:
        return weight < low - HYSTERESIS_UG or weight > high + HYSTERESIS_UG
    if state == 4:
        return weight <= high - HYSTERESIS_UG
    return row["valid"] and not row["fault"] and not row["official_overload"]


def transition_rows(rows):
    result = []
    previous = rows[0]
    for row in rows[1:]:
        before = previous["dynamic_confirmed"]
        after = row["dynamic_confirmed"]
        if before != after and before in VALID_CLASSES and after in VALID_CLASSES:
            sequence_delta = (row["sample_sequence"] - previous["sample_sequence"]) & 0xFFFFFFFF
            timestamp_delta = (row["timestamp_ms"] - previous["timestamp_ms"]) & 0xFFFFFFFF
            result.append({"utc": row["utc"], "from": NAMES[before],
                "to": NAMES[after], "sample_sequence": row["sample_sequence"],
                "observed_sequence_delta": sequence_delta,
                "observed_timestamp_delta_ms": timestamp_delta,
                "dynamic_input_ug": row["dynamic_input_ug"],
                "direct_skip": abs(after - before) == 2,
                "candidate_same_sample": row["dynamic_candidate"] == after,
                "confirm_count": row["dynamic_confirm_count"]})
        previous = row
    return result


def analyze_capture(directory):
    directory = Path(directory)
    rows = read_rows(directory / "samples.csv")
    summary = json.loads((directory / "summary.json").read_text(encoding="utf-8"))
    events = read_events(directory / "events.jsonl")
    host_gaps = [(rows[index]["host_monotonic_ns"] -
                  rows[index - 1]["host_monotonic_ns"]) / 1e6
                 for index in range(1, len(rows))]
    transitions = transition_rows(rows)
    static_contract_errors = [row["sample_sequence"] for row in rows
        if ((not row["official_stable"] and row["static_class"] != 1) or
            (row["official_stable"] and row["static_stable_count"] >= 3 and
             row["static_class"] != row["static_immediate"]))]
    return {
        "directory": directory.name,
        "records": len(rows),
        "duration_s": summary["duration_s"],
        "read_errors": summary["read_errors"],
        "unobserved_sample_sequences": summary["unobserved_sample_sequences"],
        "maximum_host_gap_ms": max(host_gaps) if host_gaps else 0,
        "p99_host_gap_ms": percentile(host_gaps, 0.99),
        "transitions": transitions,
        "observable_transition_count": len(transitions),
        "direct_skip_count": sum(item["direct_skip"] for item in transitions),
        "candidate_same_sample_count": sum(item["candidate_same_sample"]
                                             for item in transitions),
        "dynamic_outside_hysteresis_contract": sum(
            outside_hysteresis_contract(row) for row in rows),
        "static_contract_error_count": len(static_contract_errors),
        "static_contract_error_sequences": static_contract_errors[:20],
        "unstable_records": sum(not row["official_stable"] for row in rows),
        "unstable_static_pending_records": sum(
            not row["official_stable"] and row["static_class"] == 1 for row in rows),
        "formal_output_nonzero_records": sum(formal_nonzero(row) for row in rows),
        "fault_nonzero_records": sum(bool(row["fault"] or row["fault_mask"])
                                      for row in rows),
        "overrun_max": max(max(row["overrun"], row["overrun_count"]) for row in rows),
        "dirty_max": max(row["dirty"] for row in rows),
        "save_max": max(max(row["save_count"], row["save_request_count_low"])
                         for row in rows),
        "revision_values": sorted(set(row["revision"] for row in rows)),
        "saved_revision_values": sorted(set(row["saved_revision"] for row in rows)),
        "r5_offset_values": sorted(set(row["offset_ug"] for row in rows)),
        "r5_reference_values": sorted(set(row["reference_ug"] for row in rows)),
        "r5_rebase_values": sorted(set(row["automatic_rebase_count"] for row in rows)),
        "event_labels": [event["event"] for event in events],
    }


def nearest_before(rows, host_ns):
    return min((row for row in rows if row["host_monotonic_ns"] <= host_ns),
               key=lambda row: host_ns - row["host_monotonic_ns"], default=None)


def disturbance_analysis(directory):
    directory = Path(directory)
    rows = read_rows(directory / "samples.csv")
    events = read_events(directory / "events.jsonl")
    results = []
    for event in events:
        if not event["event"].endswith("DISTURBANCE_RECOVERED"):
            continue
        end_ns = event["host_monotonic_ns"]
        window = [row for row in rows
                  if end_ns - 30000000000 <= row["host_monotonic_ns"] <= end_ns]
        unstable = [row for row in window if not row["official_stable"]]
        recovered = nearest_before(rows, end_ns)
        results.append({"event": event["event"],
            "window_records": len(window), "unstable_records": len(unstable),
            "unstable_all_static_pending": all(row["static_class"] == 1
                                                for row in unstable),
            "recovered_official_stable": bool(recovered["official_stable"]),
            "recovered_static_class": NAMES[recovered["static_class"]],
            "formal_output_nonzero_records": sum(formal_nonzero(row) for row in window),
            "fault_or_overrun_records": sum(bool(row["fault"] or row["fault_mask"] or
                row["overrun"] or row["overrun_count"]) for row in window)})
    return results


def reset_capture(directory):
    directory = Path(directory)
    rows = read_rows(directory / "samples.csv")
    event = json.loads((directory / "event.json").read_text(encoding="utf-8"))
    after_sequence = event["after_sequence"]
    post = [row for row in rows if row["sample_sequence"] > after_sequence]
    reset = next((row for row in post if row["static_reason"] == 11 and
                  row["dynamic_reason"] == 11), None)
    after_reset = post if reset is None else [row for row in post
        if row["sample_sequence"] > reset["sample_sequence"]]
    reacquired = next((row for row in after_reset
        if row["static_class"] in VALID_CLASSES and
        row["dynamic_confirmed"] in VALID_CLASSES), None)
    return {"action": event["action"], "command_result": event["response"]["result_name"],
        "records": len(rows), "first_post_action": None if not post else {
            "sequence": post[0]["sample_sequence"],
            "static": NAMES[post[0]["static_class"]],
            "dynamic": NAMES[post[0]["dynamic_confirmed"]],
            "static_reason": post[0]["static_reason"],
            "dynamic_reason": post[0]["dynamic_reason"]},
        "complete_reset_sample": None if reset is None else {
            "sequence": reset["sample_sequence"], "static": NAMES[reset["static_class"]],
            "dynamic": NAMES[reset["dynamic_confirmed"]]},
        "reacquired": None if reacquired is None else {
            "sequence": reacquired["sample_sequence"],
            "static": NAMES[reacquired["static_class"]],
            "dynamic": NAMES[reacquired["dynamic_confirmed"]]},
        "formal_output_nonzero_records": sum(formal_nonzero(row) for row in rows),
        "revision_values": sorted(set(row["revision"] for row in rows)),
        "saved_revision_values": sorted(set(row["saved_revision"] for row in rows)),
        "dirty_max": max(row["dirty"] for row in rows),
        "save_max": max(row["save_count"] for row in rows),
        "fault_max": max(row["fault"] for row in rows),
        "overrun_max": max(row["overrun"] for row in rows)}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    args = parser.parse_args()
    root = args.root
    captures = {name: analyze_capture(root / name) for name in (
        "slow_fill_and_pauses", "mechanical_disturbance", "physical_tare_clear")}
    resets = [reset_capture(root / name) for name in (
        "formal_tare_reset_capture", "formal_clear_tare_reset_capture")]
    all_capture_values = list(captures.values())
    result = {
        "schema_version": 1,
        "captures": captures,
        "disturbances": disturbance_analysis(root / "mechanical_disturbance"),
        "reset_captures": resets,
        "aggregate": {
            "records": sum(item["records"] for item in all_capture_values) +
                       sum(item["records"] for item in resets),
            "read_errors": sum(item["read_errors"] for item in all_capture_values),
            "observable_transitions": sum(item["observable_transition_count"]
                                           for item in all_capture_values),
            "dynamic_outside_hysteresis_contract": sum(
                item["dynamic_outside_hysteresis_contract"]
                for item in all_capture_values),
            "static_contract_errors": sum(item["static_contract_error_count"]
                                           for item in all_capture_values),
            "formal_output_nonzero_records": sum(
                item["formal_output_nonzero_records"] for item in all_capture_values) +
                sum(item["formal_output_nonzero_records"] for item in resets),
            "maximum_host_gap_ms": max(item["maximum_host_gap_ms"]
                                       for item in all_capture_values),
        },
        "physical_scenarios": {
            "slow_fill_and_pauses": "PARTIAL PASS; segmented loading and pauses covered; extreme monotonic ramp excluded because user reported intermediate unloads",
            "mechanical_disturbance": "EXECUTED",
            "physical_tare_clear": "EXECUTED; exact reset contract supplemented by formal CommandService captures",
            "physical_fault_injection": "NOT RUN; no safe reversible injection interface in frozen 0x0515",
        },
        "qualification": "INCOMPLETE",
    }
    (root / "hardware_analysis.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result["aggregate"], indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
