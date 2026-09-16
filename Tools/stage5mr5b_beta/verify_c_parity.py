#!/usr/bin/env python3
import argparse
import csv
import hashlib
import json
import subprocess
from pathlib import Path

from evaluate_reference_lock import ROOT, RUNS
from reference_lock_model import Mode, ReferenceLock, csv_seconds

FIELDS = (
    "mode", "state", "uncompensated_gross_ug", "corrected_gross_ug",
    "offset_ug", "reference_ug", "current_window_ug", "reference_error_ug",
    "correction_rate_milli_ug_per_s", "holdoff_remaining", "reference_fill",
    "observation_fill", "automatic_rebase_count", "last_rebase_reason",
    "limited", "evaluation_count",
)


def python_row(model, second, mass, mode):
    model.set_mode(mode)
    row = model.process_second(second, mass)
    row["correction_rate_milli_ug_per_s"] = round(row.pop("correction_rate_ug_per_s") * 1000)
    return {field: int(row[field]) for field in FIELDS}


def run_case(runner, name, samples):
    request = "".join(f"{second},{mass},{int(mode)},1,0,0,0\n"
                      for second, mass, mode in samples)
    process = subprocess.run([str(runner)], input=request, text=True,
                             capture_output=True, check=True)
    c_rows = list(csv.DictReader(process.stdout.splitlines()))
    model = ReferenceLock()
    mismatches = []
    for index, ((second, mass, mode), c_row) in enumerate(zip(samples, c_rows)):
        expected = python_row(model, second, mass, mode)
        actual = {field: int(c_row[field]) for field in FIELDS}
        if actual != expected and len(mismatches) < 20:
            mismatches.append({"index": index, "second": second,
                "expected": expected, "actual": actual})
    if len(c_rows) != len(samples):
        mismatches.append({"row_count": len(c_rows), "expected_count": len(samples)})
    return {"case": name, "samples": len(samples),
            "mismatch_count": len(mismatches), "first_mismatches": mismatches}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--runner", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    runner = Path(args.runner).resolve()
    cases = []
    for name, run_id, start_utc in RUNS:
        source = csv_seconds(ROOT / "Results" / "stage5mr4" / run_id / "samples.csv", start_utc)
        cases.append(run_case(runner, name,
            [(second, mass, Mode.STATIC_COMPENSATION) for second, mass in source]))
    cycle = csv_seconds(ROOT / "Results/stage5mr4/20260916T_r4_load_unload_cycles_5/samples.csv")
    cases.append(run_case(runner, "cycles_static", [(s,m,Mode.STATIC_COMPENSATION) for s,m in cycle]))
    cases.append(run_case(runner, "cycles_dosing", [(s,m,Mode.DOSING_NO_COMPENSATION) for s,m in cycle]))
    modes=[]
    mass=0
    for second in range(2500):
        mode = Mode.OFF if second < 30 else Mode.DOSING_NO_COMPENSATION if second < 700 or 1500 <= second < 1700 else Mode.STATIC_COMPENSATION
        if 300 <= second < 600 and second % 5 == 0: mass += 1000
        modes.append((second,mass,mode))
    cases.append(run_case(runner,"mode_switches",modes))
    runner_bytes = runner.read_bytes()
    try: runner_name = runner.relative_to(ROOT).as_posix()
    except ValueError: runner_name = runner.name
    report={"schema_version":1,"runner":{"path":runner_name,
        "length":len(runner_bytes),"sha256":hashlib.sha256(runner_bytes).hexdigest().upper()},"cases":cases,
        "real_sample_count":sum(case["samples"] for case in cases[:-1]),
        "total_sample_count":sum(case["samples"] for case in cases),
        "total_mismatch_count":sum(case["mismatch_count"] for case in cases)}
    report["passed"] = report["total_mismatch_count"] == 0
    output=Path(args.output); output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(report,indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__": raise SystemExit(main())
