#!/usr/bin/env python3
"""Analyze an immutable Stage 5M-R5D capture using bounded memory."""

import argparse
import csv
import json
import math
import statistics
from collections import Counter, deque
from datetime import datetime, timezone
from pathlib import Path


MASS_FIELDS = ("uncompensated_gross_ug", "corrected_gross_ug", "offset_ug",
               "reference_error_ug", "display_count")
STATE_FIELDS = ("application", "mode", "state", "limited", "fault_mask",
                "overrun_count", "dirty", "revision", "saved_revision",
                "save_request_count_low", "automatic_rebase_count")


def parse_utc(value):
    return datetime.fromisoformat(value.replace("Z", "+00:00")).timestamp()


def median(values):
    return statistics.median(values) if values else None


def ols_slope_per_hour(points):
    if len(points) < 2:
        return None
    x0 = points[0][0]
    xs = [(x - x0) / 3600.0 for x, _ in points]
    ys = [y for _, y in points]
    xm = sum(xs) / len(xs)
    ym = sum(ys) / len(ys)
    denominator = sum((x - xm) ** 2 for x in xs)
    return 0.0 if denominator == 0.0 else sum(
        (x - xm) * (y - ym) for x, y in zip(xs, ys)) / denominator


def read_rows(path):
    with Path(path).open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            parsed = {"utc": row["utc"], "timestamp": parse_utc(row["utc"])}
            for key, value in row.items():
                if key != "utc":
                    try:
                        parsed[key] = int(value)
                    except ValueError:
                        parsed[key] = value
            yield parsed


def collect_window(path, start_offset=None, end_offset=None, tail_seconds=None):
    rows = list(read_rows(path))
    if not rows:
        return []
    start = rows[0]["timestamp"]
    end = rows[-1]["timestamp"]
    if tail_seconds is not None:
        return [row for row in rows if row["timestamp"] >= end - tail_seconds]
    low = start + (start_offset or 0)
    high = start + end_offset if end_offset is not None else math.inf
    return [row for row in rows if low <= row["timestamp"] < high]


def summarize_window(rows):
    return {key: median([row[key] for row in rows]) for key in MASS_FIELDS}


def analyze_baseline(path):
    rows = list(read_rows(path))
    values = [row["uncompensated_gross_ug"] for row in rows]
    center = median(values)
    return {
        "records": len(rows),
        "last_5m": summarize_window([row for row in rows if row["timestamp"] >= rows[-1]["timestamp"] - 300.0]),
        "median_ug": center,
        "median_absolute_deviation_ug": median([abs(value - center) for value in values]),
        "peak_to_peak_ug": max(values) - min(values),
        "ols_uncompensated_ug_per_hour": ols_slope_per_hour(
            [(row["timestamp"], row["uncompensated_gross_ug"]) for row in rows]),
    }


def analyze_active(path, duration_s=43200):
    first_time = None
    last_time = None
    first_uptime = None
    last_uptime = None
    previous_time = None
    previous_uptime = None
    maximum_gap = 0.0
    uptime_regressions = 0
    records = 0
    first_window = []
    tail = deque()
    offset_window = deque()
    maximum_offset_10s_change = 0
    maximum_absolute_offset = 0
    state_counts = Counter()
    invariants = {key: set() for key in STATE_FIELDS}
    hourly = [{key: [] for key in MASS_FIELDS} for _ in range(12)]
    ols_points = {key: [] for key in ("uncompensated_gross_ug", "corrected_gross_ug", "offset_ug")}

    for row in read_rows(path):
        timestamp = row["timestamp"]
        if first_time is None:
            first_time = timestamp
            first_uptime = row["uptime_ms"]
        records += 1
        if timestamp < first_time + 300.0:
            first_window.append(row)
        tail.append(row)
        while tail and tail[0]["timestamp"] < timestamp - 300.0:
            tail.popleft()
        offset_window.append((timestamp, row["offset_ug"]))
        while offset_window and offset_window[0][0] < timestamp - 10.0:
            old_time, old_offset = offset_window.popleft()
            if timestamp - old_time <= 10.25:
                maximum_offset_10s_change = max(
                    maximum_offset_10s_change, abs(row["offset_ug"] - old_offset))
        maximum_absolute_offset = max(maximum_absolute_offset, abs(row["offset_ug"]))
        hour = min(11, int((timestamp - first_time) // 3600.0))
        for key in MASS_FIELDS:
            hourly[hour][key].append(row[key])
        for key in ols_points:
            ols_points[key].append((timestamp, row[key]))
        state_counts[(row["application"], row["mode"], row["state"], row["limited"])] += 1
        for key in STATE_FIELDS:
            invariants[key].add(row[key])
        if previous_time is not None:
            maximum_gap = max(maximum_gap, timestamp - previous_time)
            if row["uptime_ms"] < previous_uptime:
                uptime_regressions += 1
        previous_time = timestamp
        previous_uptime = row["uptime_ms"]
        last_time = timestamp
        last_uptime = row["uptime_ms"]

    expected = int(duration_s)
    hourly_rows = []
    for index, bucket in enumerate(hourly):
        hourly_rows.append({
            "hour": index + 1,
            "records": len(bucket["offset_ug"]),
            **{key: median(bucket[key]) for key in MASS_FIELDS},
        })
    return {
        "records": records,
        "expected_records": expected,
        "coverage_percent": records * 100.0 / expected,
        "first_utc": datetime.fromtimestamp(first_time, timezone.utc).isoformat().replace("+00:00", "Z"),
        "last_utc": datetime.fromtimestamp(last_time, timezone.utc).isoformat().replace("+00:00", "Z"),
        "elapsed_seconds_between_samples": last_time - first_time,
        "uptime_elapsed_seconds": (last_uptime - first_uptime) / 1000.0,
        "uptime_regressions": uptime_regressions,
        "maximum_host_sample_gap_seconds": maximum_gap,
        "first_5m": summarize_window(first_window),
        "last_5m": summarize_window(list(tail)),
        "maximum_offset_10s_change_ug": maximum_offset_10s_change,
        "maximum_absolute_offset_ug": maximum_absolute_offset,
        "state_counts": {"%d/%d/%d/%d" % key: value for key, value in sorted(state_counts.items())},
        "state_percent": {"%d/%d/%d/%d" % key: value * 100.0 / records
                          for key, value in sorted(state_counts.items())},
        "invariant_values": {key: sorted(values) for key, values in invariants.items()},
        "ols_ug_per_hour": {key: ols_slope_per_hour(points) for key, points in ols_points.items()},
        "hourly": hourly_rows,
    }


def analyze_unload(path, removal_utc):
    removal = parse_utc(removal_utc)
    rows_2_5 = []
    tail = deque()
    offsets = set()
    loaded_before = []
    all_rows = 0
    invariants = {key: set() for key in STATE_FIELDS}
    for row in read_rows(path):
        all_rows += 1
        timestamp = row["timestamp"]
        if timestamp < removal:
            loaded_before.append(row)
        if removal + 120.0 <= timestamp < removal + 300.0:
            rows_2_5.append(row)
        tail.append(row)
        while tail and tail[0]["timestamp"] < timestamp - 300.0:
            tail.popleft()
        offsets.add(row["offset_ug"])
        for key in STATE_FIELDS:
            invariants[key].add(row[key])
    return {
        "records": all_rows,
        "loaded_before_removal": summarize_window(loaded_before),
        "unloaded_2_5m": summarize_window(rows_2_5),
        "unloaded_last_5m": summarize_window(list(tail)),
        "offset_distinct_values": sorted(offsets),
        "offset_strictly_frozen": len(offsets) == 1,
        "invariant_values": {key: sorted(values) for key, values in invariants.items()},
    }


def write_json(path, value):
    with Path(path).open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(json.dumps(value, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", required=True)
    parser.add_argument("--removal-utc", required=True)
    parser.add_argument("--verification-e-ug", type=int, required=True)
    parser.add_argument("--display-division-ug", type=int, required=True)
    args = parser.parse_args()
    run = Path(args.run)
    active_path = run / "active_12h" / "samples.csv"
    unload_path = run / "active_dosing_unload" / "samples.csv"
    empty_analysis = analyze_baseline(run / "empty_baseline" / "samples.csv")
    empty = empty_analysis["last_5m"]
    active = analyze_active(active_path)
    unload = analyze_unload(unload_path, args.removal_utc)
    active_recorder = json.loads((run / "active_12h" / "summary.json").read_text(encoding="utf-8"))
    unload_recorder = json.loads((run / "active_dosing_unload" / "summary.json").read_text(encoding="utf-8"))

    uncomp_drift = active["last_5m"]["uncompensated_gross_ug"] - active["first_5m"]["uncompensated_gross_ug"]
    corrected_drift = active["last_5m"]["corrected_gross_ug"] - active["first_5m"]["corrected_gross_ug"]
    improvement = None if uncomp_drift == 0 else (1.0 - abs(corrected_drift) / abs(uncomp_drift)) * 100.0
    loaded = unload["loaded_before_removal"]
    unloaded = unload["unloaded_2_5m"]
    uncomp_step = loaded["uncompensated_gross_ug"] - unloaded["uncompensated_gross_ug"]
    corrected_step = loaded["corrected_gross_ug"] - unloaded["corrected_gross_ug"]
    step_loss = abs(corrected_step - uncomp_step)
    unloaded_uncomp_residual = unload["unloaded_last_5m"]["uncompensated_gross_ug"] - empty["uncompensated_gross_ug"]
    unloaded_corrected_residual = unload["unloaded_last_5m"]["corrected_gross_ug"] - empty["uncompensated_gross_ug"]

    inv = active["invariant_values"]
    unload_inv = unload["invariant_values"]
    recorders_ok = (
        active_recorder["status"] == "COMPLETE" and active_recorder["duration_s"] >= 43200.0 and
        active_recorder["records"] == active["records"] and active_recorder["read_errors"] == 0 and
        active_recorder["reconnect_retries"] == 0 and active_recorder["host_poll_gap_count"] == 0 and
        unload_recorder["status"] == "COMPLETE" and unload_recorder["duration_s"] >= 1800.0 and
        unload_recorder["records"] == unload["records"] and unload_recorder["read_errors"] == 0 and
        unload_recorder["reconnect_retries"] == 0 and unload_recorder["host_poll_gap_count"] == 0)
    safe = (
        recorders_ok and
        active["records"] >= 43000 and active["coverage_percent"] >= 99.5 and
        active["uptime_regressions"] == 0 and active["maximum_host_sample_gap_seconds"] <= 30.0 and
        inv["application"] == [1] and inv["mode"] == [2] and inv["state"] == [5] and
        inv["limited"] == [0] and inv["fault_mask"] == [0] and inv["overrun_count"] == [0] and
        inv["dirty"] == [0] and inv["revision"] == [7] and inv["saved_revision"] == [7] and
        inv["save_request_count_low"] == [0] and
        inv["automatic_rebase_count"] == [0] and
        active["maximum_absolute_offset_ug"] <= 500000 and
        active["maximum_offset_10s_change_ug"] <= 1000 and
        unload["offset_strictly_frozen"] and step_loss <= 1000 and
        unload_inv["application"] == [1] and unload_inv["mode"] == [1] and
        unload_inv["state"] == [1] and unload_inv["limited"] == [0] and
        unload_inv["fault_mask"] == [0] and unload_inv["overrun_count"] == [0] and
        unload_inv["dirty"] == [0] and unload_inv["revision"] == [7] and
        unload_inv["saved_revision"] == [7] and unload_inv["save_request_count_low"] == [0] and
        unload_inv["automatic_rebase_count"] == [0]
    )
    sufficient = abs(uncomp_drift) >= args.verification_e_ug
    efficacy = (abs(corrected_drift) <= args.verification_e_ug / 2 and
                abs(corrected_drift) <= abs(uncomp_drift) and improvement is not None and improvement > 0 and
                abs(unloaded_corrected_residual) <= args.verification_e_ug / 2)
    if not safe:
        qualification = "FAIL"
    elif sufficient and efficacy:
        qualification = "PASS"
    elif not sufficient and abs(corrected_drift) <= abs(uncomp_drift):
        qualification = "SAFETY PASS; EFFICACY INCONCLUSIVE DUE TO LOW NATURAL DRIFT"
    else:
        qualification = "FAIL"

    result = {
        "schema_version": 1,
        "qualification": qualification,
        "verification_interval_e_ug": args.verification_e_ug,
        "display_division_ug": args.display_division_ug,
        "empty_baseline": empty_analysis,
        "empty_baseline_last_5m": empty,
        "active_12h": active,
        "unload": unload,
        "recorder_summaries": {"active_12h": active_recorder, "active_dosing_unload": unload_recorder},
        "metrics": {
            "uncompensated_load_drift_ug": uncomp_drift,
            "corrected_load_drift_ug": corrected_drift,
            "improvement_percent": improvement,
            "final_offset_median_ug": active["last_5m"]["offset_ug"],
            "unloaded_uncompensated_zero_residual_ug": unloaded_uncomp_residual,
            "unloaded_corrected_zero_residual_ug": unloaded_corrected_residual,
            "uncompensated_load_unload_step_ug": uncomp_step,
            "corrected_load_unload_step_ug": corrected_step,
            "load_unload_step_loss_ug": step_loss,
        },
        "gates": {"recorders_complete_and_error_free": recorders_ok,
                  "universal_safety": safe, "natural_drift_sufficient_by_e": sufficient,
                  "efficacy_if_drift_sufficient": efficacy},
    }
    write_json(run / "qualification_summary.json", result)
    with (run / "hourly_summary.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=["hour", "records"] + list(MASS_FIELDS), lineterminator="\n")
        writer.writeheader()
        writer.writerows(active["hourly"])
    print(json.dumps(result["metrics"], indent=2))
    print(qualification)


if __name__ == "__main__":
    main()
