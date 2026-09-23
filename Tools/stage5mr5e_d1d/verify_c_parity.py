#!/usr/bin/env python3
"""Compare the unified Python and C display conditioners sample by sample."""

import argparse
import csv
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(ROOT / "Tools" / "stage5mr5e_d1c"))
from unified_display_model import UnifiedDisplay, UnifiedInput, count_to_mass
from evaluate_directional_candidates import D1B, RUNS, read_rows


FIELDS = ("desired", "displayed", "delta", "state", "anchor",
    "direction", "evidence", "source", "release_reason", "locked",
    "stable", "valid", "large_step", "sample_sequence", "operator_zero")


def source_for(row):
    unit = int(row.get("unit", 1))
    decimals = int(row.get("display_decimals", 2))
    division = int(row.get("division_digit", 1))
    division_code = {1: 0, 2: 1, 5: 2}.get(division, 3)
    gross = int(row.get("weight_view", 0)) == 1
    application = int(row.get("application", 0)) == 1
    return (0x100 if gross else 0) | ((unit & 3) << 6) | \
        ((decimals & 7) << 3) | (division_code << 1) | int(application)


def real_case(relative):
    records = []
    for index, row in enumerate(read_rows(relative)):
        records.append(UnifiedInput(
            mass_ug=int(row.get("corrected_gross_ug", row["net_mass_ug"]))
                if int(row.get("weight_view", 0)) == 1 else
                int(row["net_mass_ug"]),
            sequence=int(row.get("sample_sequence", index)),
            now_ms=int(row.get("uptime_ms", index * 100)),
            source=source_for(row), unit=int(row.get("unit", 1)),
            decimals=int(row.get("display_decimals", 2)),
            division=int(row.get("division_digit", 1)),
            stable=bool(int(row.get("stable",
                1 if int(row.get("status_flags", 0)) & 16 else 0)))))
    return records


def synthetic_case():
    records = []
    sequence = 0
    now = 0
    for unit, decimals, division in ((1, 2, 1), (1, 1, 2),
            (0, 4, 5), (2, 3, 1)):
        source = (unit << 6) | (decimals << 3) | \
            ({1: 0, 2: 1, 5: 2}[division] << 1)
        for target in ([0] * 12 + [division * 2] * 15 +
                [-division * 2] * 12 + [division * 8] * 7 +
                [division * 9] * 3):
            sequence = (sequence + 1) & 0xFFFFFFFF
            now += 100
            records.append(UnifiedInput(count_to_mass(target, unit, decimals),
                sequence, now, source, unit, decimals, division, True))
            if target == division * 2:
                records.append(UnifiedInput(count_to_mass(target, unit,
                    decimals), sequence, now + 20, source, unit, decimals,
                    division, True))
    records.extend([
        UnifiedInput(0, 0xFFFFFFFF, now + 100, 0x220, 1, 2, 1, True),
        UnifiedInput(20000, 0, now + 200, 0x220, 1, 2, 1, True),
        UnifiedInput(0, 1, now + 300, 0x221, 1, 2, 1, True),
        UnifiedInput(0, 2, now + 400, 0x221, 1, 6, 1, True),
        UnifiedInput(0, 3, now + 500, 0x222, 1, 2, 1, True,
            operator_zero=True),
        UnifiedInput(0, 4, now + 600, 0x222, 1, 2, 1, True),
        UnifiedInput(0, 5, now + 700, 0x222, 1, 2, 1, False),
        UnifiedInput(0, 6, now + 3800, 0x222, 1, 2, 1, False),
        UnifiedInput(0, 7, now + 3900, 0x222, 1, 2, 1, True, reset=True),
        UnifiedInput(0, 8, now + 4000, 0x222, 1, 2, 1, True, valid=False),
    ])
    for index in range(1000):
        sequence = (sequence + (0 if index % 5 else 1)) & 0xFFFFFFFF
        now += 20
        target = (0, 1, 0, -1, 0, 2, 2, 2, -2, -2)[index % 10]
        records.append(UnifiedInput(count_to_mass(target, 1, 2), sequence,
            now, 0x240, 1, 2, 1, True))
    return records


def run_case(runner, name, records):
    model = UnifiedDisplay()
    expected = []
    lines = []
    for item in records:
        expected.append(model.process(item))
        lines.append("{},{},{},{},{},{},{},{},{},{},{}".format(
            item.mass_ug, item.sequence, item.now_ms, item.source, item.unit,
            item.decimals, item.division, int(item.stable), int(item.valid),
            int(item.reset), int(item.operator_zero)))
    process = subprocess.run([str(runner)], input="\n".join(lines) + "\n",
        text=True, capture_output=True, check=True)
    actual = list(csv.DictReader(process.stdout.splitlines()))
    mismatch_count = 0
    first = []
    for index, (actual_row, expected_row) in enumerate(zip(actual, expected)):
        converted = {field: int(actual_row[field]) for field in FIELDS}
        if converted != expected_row:
            mismatch_count += 1
            if len(first) < 20:
                first.append({"index": index, "python": expected_row,
                    "c": converted})
    mismatch_count += abs(len(actual) - len(expected))
    return {"case": name, "samples": len(expected),
        "mismatches": mismatch_count, "first_mismatches": first}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--runner", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    cases = [run_case(args.runner, name, real_case(relative))
        for name, relative in {**RUNS,
            "d1b_opened_regression": D1B}.items()]
    cases.append(run_case(args.runner, "synthetic_boundaries", synthetic_case()))
    value = {"schema_version": 1,
        "classification": "DEVELOPMENT / OPENED REGRESSION",
        "state_bytes": int(subprocess.check_output(
            [str(args.runner), "--sizeof"], text=True)),
        "cases": cases,
        "samples": sum(case["samples"] for case in cases),
        "mismatches": sum(case["mismatches"] for case in cases)}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(value, indent=2) + "\n",
        encoding="utf-8")
    print(json.dumps(value, indent=2))
    return 0 if value["mismatches"] == 0 else 2


if __name__ == "__main__":
    raise SystemExit(main())
