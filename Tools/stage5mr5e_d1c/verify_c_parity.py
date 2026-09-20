#!/usr/bin/env python3
"""Compare frozen D1-C Python and fixed-point C state sample by sample."""

import argparse
import csv
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from directional_model import DirectionalConfig, DirectionalDisplay, quantize_count
from evaluate_directional_candidates import D1B, RUNS, read_rows


CONFIG = DirectionalConfig(1, 1, 1, 5, False, 8)
FIELDS = ("desired_count", "current_count", "delta_count", "direction",
    "evidence", "anchor_count", "locked", "stable", "active",
    "large_step", "release_reason", "source", "unit", "decimals",
    "application", "mode")


def real_case(relative):
    records = []
    for index, row in enumerate(read_rows(relative)):
        decimals = int(row.get("display_decimals", 2))
        desired = int(row.get("desired_display_count",
            quantize_count(int(row["net_mass_ug"]), 1000000,
                10 ** decimals, 1)))
        stable = int(row.get("stable",
            1 if int(row.get("status_flags", 0)) & 16 else 0))
        application = int(row.get("application", 0))
        records.append({"desired": desired,
            "baseline": int(row["display_count"]), "source": 0,
            "sequence": int(row.get("sample_sequence", index)),
            "stable": stable, "active": int(application == 1), "valid": 1,
            "unit": int(row.get("unit", 0)), "decimals": decimals,
            "application": application, "mode": int(row.get("mode", 0))})
    return records


def synthetic_cases():
    boundary = [0 if index % 2 == 0 else 1 for index in range(600)]
    reversal = [0] * 20 + [4] * 80 + [-4] * 100
    limits = [(1 << 31) - 1, -(1 << 31), (1 << 31) - 1]
    def pack(values):
        return [{"desired": value, "baseline": values[0], "source": 0,
            "sequence": index,
            "stable": 1, "active": 1, "valid": 1, "unit": 0,
            "decimals": 2, "application": 1, "mode": 2}
            for index, value in enumerate(values)]
    duplicates = pack([0] * 5 + [2] * 30)
    for index, item in enumerate(duplicates):
        item["sequence"] = index // 5
    return {"boundary_noise": pack(boundary),
        "direction_reversal": pack(reversal), "int32_limits": pack(limits),
        "scheduler_duplicates": duplicates}


def run_case(runner, name, records):
    model = DirectionalDisplay(CONFIG); expected = []; lines = []
    previous_sequence = None; previous_result = None
    for item in records:
        if previous_sequence == item["sequence"]:
            result = previous_result
        else:
            result = model.process(item["desired"], bool(item["stable"]),
                bool(item["active"]), bool(item["valid"]), item["baseline"],
                item["source"])
            previous_sequence = item["sequence"]
            previous_result = result
        expected.append({**{key: int(result[key]) for key in FIELDS[:12]},
            **{key: int(item[key]) for key in FIELDS[12:]}})
        lines.append("{desired},{baseline},{sequence},{source},{stable},{active},{valid},"
            "{unit},{decimals},{application},{mode}".format(**item))
    process = subprocess.run([str(runner)], input="\n".join(lines) + "\n",
        text=True, capture_output=True, check=True)
    actual = list(csv.DictReader(process.stdout.splitlines()))
    mismatches = []
    for index, (c_row, py_row) in enumerate(zip(actual, expected)):
        c_value = {field: int(c_row[field]) for field in FIELDS}
        if c_value != py_row and len(mismatches) < 20:
            mismatches.append({"index": index, "python": py_row,
                "c": c_value})
    if len(actual) != len(expected):
        mismatches.append({"row_count": len(actual),
            "expected_count": len(expected)})
    return {"case": name, "samples": len(expected),
        "mismatches": len(mismatches), "first_mismatches": mismatches}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--runner", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(); cases = []
    for name, relative in {**RUNS, "d1b_opened_regression": D1B}.items():
        cases.append(run_case(args.runner, name, real_case(relative)))
    for name, records in synthetic_cases().items():
        cases.append(run_case(args.runner, name, records))
    state_bytes = int(subprocess.check_output(
        [str(args.runner), "--sizeof"], text=True))
    value = {"schema_version": 1, "parameters": CONFIG.__dict__,
        "state_bytes": state_bytes, "cases": cases,
        "samples": sum(item["samples"] for item in cases),
        "mismatches": sum(item["mismatches"] for item in cases)}
    args.output.write_text(json.dumps(value, indent=2) + "\n",
        encoding="utf-8")
    print(json.dumps(value, indent=2))
    return 0 if value["mismatches"] == 0 else 2


if __name__ == "__main__":
    raise SystemExit(main())
