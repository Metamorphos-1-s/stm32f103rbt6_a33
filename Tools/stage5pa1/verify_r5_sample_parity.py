#!/usr/bin/env python3
import argparse
import csv
import io
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools" / "stage5mr5b_beta"))
from reference_lock_model import Mode, ReferenceLock


def cases():
    rows = []
    sequence = 0
    timestamp = 0
    for interval, count, mode in ((100, 18000, Mode.STATIC_COMPENSATION),
                                  (25, 72000, Mode.STATIC_COMPENSATION),
                                  (25, 4000, Mode.DOSING_NO_COMPENSATION)):
        for index in range(count):
            sequence = (sequence + 1) & 0xFFFFFFFF
            timestamp = (timestamp + interval) & 0xFFFFFFFF
            mass = 500000000 + (timestamp // 1000) * 100
            if mode == Mode.DOSING_NO_COMPENSATION and index % 500 < 20:
                mass = 0
            rows.append((sequence, timestamp, mass, int(mode), 1, 0, 0, 0))
    for interval in (25, 26, 38, 34, 99, 101) * 200:
        sequence = (sequence + 1) & 0xFFFFFFFF
        timestamp = (timestamp + interval) & 0xFFFFFFFF
        rows.append((sequence, timestamp, 500250000,
                     int(Mode.STATIC_COMPENSATION), 1, 0, 0, 0))
    return rows


def python_rows(source):
    model = ReferenceLock()
    result = []
    for sequence, stamp, mass, mode, valid, fault, overload, rail in source:
        model.set_mode(mode)
        snap = model.process_sample(sequence, stamp, mass, valid=bool(valid),
            fault=bool(fault), overload=bool(overload), near_rail=bool(rail))
        result.append({
            "sequence": sequence, "timestamp_ms": stamp,
            "mode": snap["mode"], "state": snap["state"],
            "uncompensated_gross_ug": snap["uncompensated_gross_ug"],
            "corrected_gross_ug": snap["corrected_gross_ug"],
            "offset_ug": snap["offset_ug"], "reference_ug": snap["reference_ug"],
            "current_window_ug": snap["current_window_ug"],
            "reference_error_ug": snap["reference_error_ug"],
            "correction_rate_milli_ug_per_s":
                round(snap["correction_rate_ug_per_s"] * 1000),
            "holdoff_remaining": snap["holdoff_remaining"],
            "reference_fill": snap["reference_fill"],
            "observation_fill": snap["observation_fill"],
            "automatic_rebase_count": snap["automatic_rebase_count"],
            "last_rebase_reason": snap["last_rebase_reason"],
            "limited": snap["limited"],
            "evaluation_count": snap["evaluation_count"]})
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = cases()
    payload = "".join(",".join(str(value) for value in row) + "\n"
                      for row in source)
    completed = subprocess.run([str(args.runner)], input=payload, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    actual = [{key: int(value) for key, value in row.items()}
              for row in csv.DictReader(io.StringIO(completed.stdout))]
    expected = python_rows(source)
    first = None
    mismatches = 0
    for index, (want, got) in enumerate(zip(expected, actual)):
        if want != got:
            mismatches += 1
            if first is None:
                first = {"index": index, "expected": want, "actual": got}
    summary = {"samples_total": len(source), "real_samples": 0,
        "synthetic_samples": len(source), "mismatch_count": mismatches,
        "first_mismatch": first,
        "scenarios": ["10 Hz 30 min", "40 Hz 30 min",
            "40 Hz DOSING steps", "26-40 Hz and 99/101 ms jitter"]}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(summary, indent=2) + "\n",
                           encoding="utf-8")
    print(json.dumps(summary, indent=2))
    return 0 if mismatches == 0 and len(actual) == len(expected) else 2


if __name__ == "__main__":
    raise SystemExit(main())
