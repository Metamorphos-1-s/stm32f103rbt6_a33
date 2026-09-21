#!/usr/bin/env python3
"""Analyze the Stage 5N-A3 monotonic slow-fill hardware holdout."""

import argparse
import csv
import json
import statistics
from pathlib import Path

VALID_CLASSES = {2, 3, 4}
CLASS_NAMES = {0: "INVALID", 1: "PENDING", 2: "LOW", 3: "OK", 4: "HIGH"}
HYSTERESIS_UG = 20000


def integer_rows(path):
    with Path(path).open(encoding="utf-8", newline="") as stream:
        rows = list(csv.DictReader(stream))
    text_fields = {"utc", "firmware", "map", "signature"}
    for row in rows:
        for key in row.keys() - text_fields:
            if row[key] != "":
                row[key] = int(row[key])
    return rows


def load_events(path):
    with Path(path).open(encoding="utf-8") as stream:
        return [json.loads(line) for line in stream if line.strip()]


def formal_nonzero(row):
    return any(row[key] for key in ("formal_state", "formal_alarm_active",
        "formal_green", "formal_yellow", "formal_red",
        "formal_internal_buzzer", "formal_external_buzzer"))


def dynamic_contract_error(row):
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
    return True


def analyze(run):
    run = Path(run)
    rows = integer_rows(run / "samples.csv")
    events = load_events(run / "events.jsonl")
    summary = json.loads((run / "summary.json").read_text(encoding="utf-8"))
    baseline_end = next(event["host_monotonic_ns"] for event in events
                        if event["event"] == "CONTAINER_LOW_STABLE")
    baseline = [row["static_input_ug"] for row in rows
                if row["host_monotonic_ns"] <= baseline_end]
    baseline_range = max(baseline) - min(baseline)

    origin = rows[0]["host_monotonic_ns"]
    buckets = {}
    for row in rows:
        index = (row["host_monotonic_ns"] - origin) // 5000000000
        buckets.setdefault(index, []).append(row["static_input_ug"])
    medians = [(index, int(statistics.median(values)))
               for index, values in sorted(buckets.items()) if len(values) >= 10]
    median_deltas = [current[1] - previous[1]
                     for previous, current in zip(medians, medians[1:])]
    peak = medians[0][1]
    maximum_drawdown = 0
    for _, value in medians:
        peak = max(peak, value)
        maximum_drawdown = max(maximum_drawdown, peak - value)

    transitions = []
    for previous, current in zip(rows, rows[1:]):
        before = previous["dynamic_confirmed"]
        after = current["dynamic_confirmed"]
        if before != after and before in VALID_CLASSES and after in VALID_CLASSES:
            transitions.append({
                "utc": current["utc"],
                "from": CLASS_NAMES[before],
                "to": CLASS_NAMES[after],
                "sample_sequence": current["sample_sequence"],
                "dynamic_input_ug": current["dynamic_input_ug"],
                "observed_sequence_delta": (current["sample_sequence"] -
                    previous["sample_sequence"]) & 0xFFFFFFFF,
                "observed_timestamp_delta_ms": (current["timestamp_ms"] -
                    previous["timestamp_ms"]) & 0xFFFFFFFF,
                "candidate_same_sample": current["dynamic_candidate"] == after,
            })

    static_errors = sum(
        ((not row["official_stable"]) and row["static_class"] != 1) or
        (row["official_stable"] and row["static_stable_count"] >= 3 and
         row["static_class"] != row["static_immediate"])
        for row in rows)
    dynamic_errors = sum(dynamic_contract_error(row) for row in rows)
    host_gaps_ms = [(current["host_monotonic_ns"] - previous["host_monotonic_ns"])
                    / 1000000.0 for previous, current in zip(rows, rows[1:])]
    expected_transitions = [("LOW", "OK"), ("OK", "HIGH")]
    observed_transitions = [(item["from"], item["to"]) for item in transitions]
    monotonic_valid = (maximum_drawdown <= baseline_range and
                       not any(delta < -100000 for delta in median_deltas))
    result = {
        "schema_version": 1,
        "result": "PASS" if (monotonic_valid and
            observed_transitions == expected_transitions and
            all(item["candidate_same_sample"] for item in transitions) and
            max(item["observed_timestamp_delta_ms"] for item in transitions) <= 600 and
            static_errors == 0 and dynamic_errors == 0 and
            sum(formal_nonzero(row) for row in rows) == 0 and
            summary["read_errors"] == 0) else "FAIL",
        "records": len(rows),
        "duration_s": summary["duration_s"],
        "start_weight_ug": rows[0]["static_input_ug"],
        "end_weight_ug": rows[-1]["static_input_ug"],
        "gain_ug": rows[-1]["static_input_ug"] - rows[0]["static_input_ug"],
        "average_gain_g_per_min": ((rows[-1]["static_input_ug"] -
            rows[0]["static_input_ug"]) / 1000000.0) /
            (summary["duration_s"] / 60.0),
        "user_operation_contract": "water added only; container not moved; no unload",
        "baseline_records": len(baseline),
        "baseline_range_ug": baseline_range,
        "five_second_median_bucket_count": len(medians),
        "minimum_five_second_median_delta_ug": min(median_deltas),
        "maximum_five_second_median_drawdown_ug": maximum_drawdown,
        "reverse_windows_over_100mg": sum(delta < -100000 for delta in median_deltas),
        "monotonic_within_frozen_noise_envelope": monotonic_valid,
        "transitions": transitions,
        "transition_order": [list(item) for item in observed_transitions],
        "maximum_observed_transition_bound_ms": max(
            item["observed_timestamp_delta_ms"] for item in transitions),
        "static_contract_errors": static_errors,
        "dynamic_hysteresis_contract_errors": dynamic_errors,
        "unstable_records": sum(not row["official_stable"] for row in rows),
        "unstable_static_pending_records": sum(
            not row["official_stable"] and row["static_class"] == 1 for row in rows),
        "formal_output_nonzero_records": sum(formal_nonzero(row) for row in rows),
        "fault_max": max(max(row["fault"], row["fault_mask"]) for row in rows),
        "overrun_max": max(max(row["overrun"], row["overrun_count"]) for row in rows),
        "dirty_max": max(row["dirty"] for row in rows),
        "save_max": max(max(row["save_count"], row["save_request_count_low"])
                         for row in rows),
        "revision_values": sorted(set(row["revision"] for row in rows)),
        "saved_revision_values": sorted(set(row["saved_revision"] for row in rows)),
        "r5_offset_values": sorted(set(row["offset_ug"] for row in rows)),
        "r5_reference_values": sorted(set(row["reference_ug"] for row in rows)),
        "r5_rebase_values": sorted(set(row["automatic_rebase_count"] for row in rows)),
        "read_errors": summary["read_errors"],
        "maximum_host_gap_ms": max(host_gaps_ms),
        "events": events,
    }
    (run / "p1_analysis.json").write_text(json.dumps(result, indent=2) + "\n",
                                           encoding="utf-8")
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    args = parser.parse_args()
    result = analyze(args.input)
    print(json.dumps(result, indent=2))
    return 0 if result["result"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
