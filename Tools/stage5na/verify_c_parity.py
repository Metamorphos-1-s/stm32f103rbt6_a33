#!/usr/bin/env python3
"""Sample-by-sample Python/C parity for the frozen Stage 5N-A candidate."""

import argparse
import csv
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from checkweigh_model import CheckweighShadow, ShadowConfig
from evaluate_candidates import RUNS, read_rows, real_samples


CONFIG = ShadowConfig(3, 20000, 1, 0)
FIELDS = ("static_input_ug", "dynamic_input_ug", "stable",
    "process_active", "valid", "static_immediate", "static_class",
    "static_last_valid", "static_stable_count", "static_reason",
    "dynamic_immediate", "dynamic_candidate", "dynamic_confirmed",
    "dynamic_confirm_count", "dynamic_reason", "reset_reason", "event",
    "event_count")


def run_case(runner, name, records):
    model = CheckweighShadow(CONFIG); expected = []; lines = []
    for record in records:
        result = model.process(sequence=record["sequence"],
            timestamp_ms=record["timestamp"],
            static_weight_ug=record["static"],
            dynamic_weight_ug=record["dynamic"],
            low_limit_ug=record["low"], high_limit_ug=record["high"],
            stable=record["stable"], process_active=record["process_active"],
            valid=record["valid"], fault=record["fault"],
            overload=record["overload"], calibration=record["calibration"],
            reset_reason=record["reset_reason"])
        expected.append({field: int(result[field]) for field in FIELDS})
        lines.append("{},{},{},{},{},{},{},{},{},{},{},{},{}".format(
            record["sequence"], record["timestamp"], record["static"],
            record["dynamic"], record["low"], record["high"],
            int(record["stable"]), int(record["process_active"]),
            int(record["valid"]), int(record["fault"]),
            int(record["overload"]), int(record["calibration"]),
            record["reset_reason"]))
    process = subprocess.run([str(runner)], input="\n".join(lines) + "\n",
        text=True, capture_output=True, check=True)
    actual = list(csv.DictReader(process.stdout.splitlines()))
    mismatches = []
    for index, (c_row, py_row) in enumerate(zip(actual, expected)):
        c_value = {field: int(c_row[field]) for field in FIELDS}
        if c_value != py_row and len(mismatches) < 20:
            mismatches.append({"index": index, "python": py_row, "c": c_value})
    if len(actual) != len(expected):
        mismatches.append({"rows": len(actual), "expected": len(expected)})
    return {"case": name, "samples": len(records),
        "mismatches": len(mismatches), "first_mismatches": mismatches}


def real_records(name, relative):
    samples = real_samples(read_rows(relative))
    masses = [sample["dynamic"] for sample in samples]
    if name in ("slow_fill", "faster_fill"):
        low = int(min(masses) + (max(masses) - min(masses)) / 3)
        high = int(min(masses) + 2 * (max(masses) - min(masses)) / 3)
    else:
        low, high = 100000000, 400000000
    return [{**sample, "low": low, "high": high,
        "process_active": False, "fault": False, "calibration": False,
        "reset_reason": 0} for sample in samples]


def synthetic_records():
    values = [0] * 20 + [150000000] * 20 + [500000000] * 20 + \
        [150000000] * 20 + [0] * 20
    records = []
    for index, mass in enumerate(values, 1):
        records.append({"sequence": index, "timestamp": index * 100,
            "static": mass, "dynamic": mass, "low": 100000000,
            "high": 400000000, "stable": index % 7 != 0,
            "process_active": 30 <= index < 40, "valid": True,
            "fault": index == 45, "overload": index == 65,
            "calibration": index == 75,
            "reset_reason": 4 if index in (25, 55, 85) else 0})
    records.extend((
        {"sequence": 500, "timestamp": 50000, "static": 0, "dynamic": 0,
         "low": 2, "high": 1, "stable": True, "process_active": False,
         "valid": True, "fault": False, "overload": False,
         "calibration": False, "reset_reason": 0},
        {"sequence": 502, "timestamp": 50100, "static": -1,
         "dynamic": -1, "low": -1, "high": -1, "stable": True,
         "process_active": False, "valid": True, "fault": False,
         "overload": False, "calibration": False, "reset_reason": 0},
        {"sequence": 503, "timestamp": 50000, "static": 1,
         "dynamic": 1, "low": -1, "high": 1, "stable": True,
         "process_active": False, "valid": True, "fault": False,
         "overload": False, "calibration": False, "reset_reason": 0}))
    return records


def main():
    parser = argparse.ArgumentParser(); parser.add_argument("--runner", type=Path,
        required=True); parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(); cases = []
    for name, relative in RUNS.items():
        cases.append(run_case(args.runner, name, real_records(name, relative)))
    cases.append(run_case(args.runner, "synthetic_resets_boundaries",
        synthetic_records()))
    value = {"schema_version": 1,
        "state_bytes": int(subprocess.check_output(
            [str(args.runner), "--sizeof"], text=True)),
        "config": CONFIG.__dict__, "cases": cases,
        "samples": sum(case["samples"] for case in cases),
        "mismatches": sum(case["mismatches"] for case in cases)}
    args.output.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(value, indent=2))
    return 0 if value["mismatches"] == 0 else 2


if __name__ == "__main__":
    raise SystemExit(main())
