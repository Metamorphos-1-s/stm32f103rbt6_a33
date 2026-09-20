#!/usr/bin/env python3
"""Analyze a D1-C hardware holdout without modifying raw capture files."""

import argparse
import csv
import json
from collections import deque
from pathlib import Path


def analyze(path):
    with path.open(encoding="utf-8", newline="") as stream:
        rows = list(csv.DictReader(stream))
    if len(rows) < 2:
        raise ValueError("at least two samples required")
    integer = lambda row, key: int(row[key])
    first_sequence = integer(rows[0], "sample_sequence")
    final_sequence = integer(rows[-1], "sample_sequence")
    expected = ((final_sequence - first_sequence) & 0xFFFFFFFF) + 1
    desired_values = [integer(row, "desired_count") for row in rows]
    offset_values = [integer(row, "offset_ug") for row in rows]
    update_count = wrong_updates = transition_violations = 0
    maximum_jump = maximum_lag = maximum_sequence_gap = 0
    maximum_device_gap_ms = 0; aba = 0; max_aba_10s = 0; aba_times = deque()
    stale_over_1d = 0; lag2_start = None; maximum_lag2_duration_ms = 0
    previous_direction = 0; direction_reversals = 0
    for index, row in enumerate(rows):
        desired = integer(row, "desired_count")
        display = integer(row, "display_count")
        lag = abs(desired - display); maximum_lag = max(maximum_lag, lag)
        qualified = integer(row, "stable") and integer(row, "application") == 1
        if qualified and lag > 1:
            stale_over_1d += 1
        if qualified and lag > 2:
            if lag2_start is None:
                lag2_start = integer(row, "uptime_ms")
            maximum_lag2_duration_ms = max(maximum_lag2_duration_ms,
                integer(row, "uptime_ms") - lag2_start)
        else:
            lag2_start = None
        direction = integer(row, "direction")
        if direction and previous_direction and direction != previous_direction:
            direction_reversals += 1
        if direction:
            previous_direction = direction
        if index == 0:
            continue
        previous = rows[index - 1]
        sequence_gap = (integer(row, "sample_sequence") -
            integer(previous, "sample_sequence")) & 0xFFFFFFFF
        device_gap = (integer(row, "uptime_ms") -
            integer(previous, "uptime_ms")) & 0xFFFFFFFF
        maximum_sequence_gap = max(maximum_sequence_gap, sequence_gap)
        maximum_device_gap_ms = max(maximum_device_gap_ms, device_gap)
        jump = display - integer(previous, "display_count")
        pre_delta = desired - integer(previous, "display_count")
        maximum_jump = max(maximum_jump, abs(jump))
        if jump:
            update_count += 1
            valid_small = abs(jump) <= 1 and ((jump > 0) == (pre_delta > 0))
            valid_release = abs(pre_delta) > 8 and display == desired
            if not (valid_small or valid_release):
                transition_violations += 1
            if pre_delta and ((jump > 0) != (pre_delta > 0)):
                wrong_updates += 1
        if index >= 2 and integer(rows[index - 2], "display_count") == display and \
                integer(previous, "display_count") != display:
            aba += 1; aba_times.append(integer(row, "uptime_ms"))
        while aba_times and integer(row, "uptime_ms") - aba_times[0] > 10000:
            aba_times.popleft()
        max_aba_10s = max(max_aba_10s, len(aba_times))
    safety = {
        "fault_max": max(integer(row, "fault_mask") for row in rows),
        "overrun_max": max(integer(row, "overrun_count") for row in rows),
        "dirty_max": max(integer(row, "dirty") for row in rows),
        "save_max": max(integer(row, "save_request_count_low") for row in rows),
        "revision_values": sorted(set(integer(row, "revision") for row in rows)),
        "saved_revision_values": sorted(set(integer(row, "saved_revision") for row in rows)),
        "rebase_max": max(integer(row, "automatic_rebase_count") for row in rows),
    }
    trigger = max(abs(value) for value in offset_values) >= 20000 and \
        max(desired_values) - min(desired_values) >= 2
    safety_pass = all(safety[key] == 0 for key in
        ("fault_max", "overrun_max", "dirty_max", "save_max", "rebase_max")) and \
        safety["revision_values"] == [8] and safety["saved_revision_values"] == [8]
    display_pass = wrong_updates == 0 and transition_violations == 0 and \
        maximum_jump <= 1 and max_aba_10s <= 2 and maximum_lag2_duration_ms <= 5000
    return {"schema_version": 1,
        "classification": "NEW HARDWARE HOLDOUT; STOPPED FOR LOW NATURAL OFFSET",
        "records": len(rows), "expected_records": expected,
        "coverage": len(rows) / expected, "first_sequence": first_sequence,
        "final_sequence": final_sequence,
        "duration_ms": integer(rows[-1], "uptime_ms") - integer(rows[0], "uptime_ms"),
        "maximum_sequence_gap": maximum_sequence_gap,
        "maximum_device_gap_ms": maximum_device_gap_ms,
        "desired_min": min(desired_values), "desired_max": max(desired_values),
        "desired_span_divisions": max(desired_values) - min(desired_values),
        "maximum_abs_offset_ug": max(abs(value) for value in offset_values),
        "final_offset_ug": offset_values[-1], "trigger_reached": trigger,
        "display": {"updates": update_count, "wrong_direction_updates": wrong_updates,
            "transition_violations": transition_violations,
            "maximum_jump_divisions": maximum_jump,
            "maximum_lag_divisions": maximum_lag,
            "stale_over_1d_records": stale_over_1d,
            "maximum_over_2d_duration_ms": maximum_lag2_duration_ms,
            "aba_count": aba, "max_aba_10s": max_aba_10s,
            "direction_reversals": direction_reversals,
            "initial_desired": desired_values[0],
            "initial_display": integer(rows[0], "display_count"),
            "final_desired": desired_values[-1],
            "final_display": integer(rows[-1], "display_count")},
        "safety": safety, "safety_pass": safety_pass,
        "display_gate_pass_on_observed_low_offset_data": display_pass,
        "qualification": "NONZERO-OFFSET HARDWARE QUALIFICATION PENDING"
            if safety_pass and display_pass and not trigger else "REVIEW REQUIRED"}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(); result = analyze(args.input)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2)); return 0


if __name__ == "__main__":
    raise SystemExit(main())
