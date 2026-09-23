#!/usr/bin/env python3
"""Evaluate the frozen Stage 5P-A EARLY_TRACKING candidate on opened data."""

import argparse
import csv
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DATASETS = {
    "stage5l_creep": "Results/stage5l_characterization/20260913T064617Z_creep_500g_30m/samples.csv",
    "r4_placement_2": "Results/stage5mr4/20260916T_r4_500g_placement_2/samples.csv",
    "r4_cycles": "Results/stage5mr4/20260916T_r4_load_unload_cycles_5/samples.csv",
    "filt0_load": "Results/stage5l_characterization/20260913_filter_compare/filt0_load_step/samples.csv",
    "filt1_load": "Results/stage5l_characterization/20260913_filter_compare/filt1_load_step/samples.csv",
    "filt2_load": "Results/stage5l_characterization/20260913_filter_compare/filt2_load_step_retry/samples.csv",
    "filt3_load": "Results/stage5l_characterization/20260913_filter_compare/filt3_load_step/samples.csv",
    "r5d_12h": "Results/stage5mr5d/20260917T182739Z_r5d_12h_500g/active_12h/samples.csv",
    "r5d_unload": "Results/stage5mr5d/20260917T182739Z_r5d_12h_500g/active_dosing_unload/samples.csv",
    "dosing_slow_fill": "Results/stage5na2/20260921T095640Z_shadow_hardware_closure/slow_fill_and_pauses/samples.csv"
}
CHECKPOINTS = (60, 120, 180, 300, 600, 900, 1800)


def median(values):
    values = sorted(values)
    return values[len(values) // 2]


def read(path):
    with path.open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def field(row, names):
    for name in names:
        if row.get(name, "") != "":
            return int(row[name])
    raise KeyError(names)


def replay(rows, enabled):
    if not rows:
        return None
    t0 = field(rows[0], ("uptime_ms", "mcu_uptime_ms"))
    points = []
    for row in rows:
        elapsed = (field(row, ("uptime_ms", "mcu_uptime_ms")) - t0) / 1000
        mass = field(row, ("uncompensated_gross_ug", "gross_mass_ug",
            "gross_ug", "net_mass_ug", "net_ug", "filtered_mass_ug",
            "raw_calibrated_mass_ug"))
        stable = bool(int(row.get("stable",
            1 if int(row.get("status_flags", 0)) & 16 else 0)))
        points.append((elapsed, mass, stable))
    reference_values = [mass for elapsed, mass, stable in points
        if 15 <= elapsed <= 45 and stable]
    if not reference_values:
        return {"eligible": False}
    reference = median(reference_values)
    offset = 0
    evidence = 0
    evaluations = []
    for second in range(65, 901, 10):
        window = [mass for elapsed, mass, stable in points
            if second - 30 <= elapsed <= second and stable]
        if not enabled or not window:
            evaluations.append((second, offset, 0))
            continue
        error = median(window) - offset - reference
        direction = 1 if error > 10000 else -1 if error < -10000 else 0
        if direction == 0:
            evidence = 0
        elif evidence == 0 or (evidence > 0) == (direction > 0):
            evidence += direction
        else:
            evidence += direction
        if abs(evidence) >= 2:
            step = min(1667, max(-1667, error))
            offset = min(100000, max(-100000, offset + step))
            evidence = 0
        evaluations.append((second, offset, error))
    metrics = {}
    for second in CHECKPOINTS:
        candidates = [(elapsed, mass) for elapsed, mass, stable in points
            if elapsed <= second and stable]
        if candidates:
            mass = candidates[-1][1]
            applied = max((item for item in evaluations if item[0] <= second),
                default=(0, 0, 0))[1]
            metrics[str(second)] = {"original_error_ug": mass - reference,
                "corrected_error_ug": mass - applied - reference,
                "offset_ug": applied}
    reverse = any(abs(value["corrected_error_ug"]) >
        abs(value["original_error_ug"]) + 10000 for value in metrics.values())
    improved = sum(abs(value["corrected_error_ug"]) <
        abs(value["original_error_ug"]) for key, value in metrics.items()
        if int(key) in (300, 600, 900))
    return {"eligible": True, "reference_ug": reference,
        "metrics": metrics, "final_offset_ug": offset,
        "reverse_amplification": reverse,
        "improved_5_10_15_count": improved,
        "max_10s_correction_ug": max((abs(evaluations[index][1] -
            evaluations[index - 1][1]) for index in range(1,
            len(evaluations))), default=0)}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    results = {}
    for name, relative in DATASETS.items():
        path = ROOT / relative
        if path.is_file():
            results[name] = {"path": relative,
                "classification": "DEVELOPMENT / OPENED REGRESSION",
                "candidate": replay(read(path), name != "dosing_slow_fill")}
    qualified = [item["candidate"] for item in results.values()
        if item["candidate"] and item["candidate"].get("eligible") and
        item["path"] != DATASETS["dosing_slow_fill"]]
    passed = (bool(qualified) and
        all((not item["reverse_amplification"]) and
            (item["max_10s_correction_ug"] <= 1667)
            for item in qualified) and
        all(item["improved_5_10_15_count"] >= 2 for item in qualified))
    value = {"schema_version": 1,
        "classification": "DEVELOPMENT / OPENED REGRESSION",
        "parameters": {"holdoff_s": 15, "early_reference_s": [15, 45],
            "evaluation_s": 10, "window_s": 30, "confirmations": 2,
            "earliest_adjustment_s": 65, "end_s": 900,
            "max_step_ug": 1667, "max_early_offset_ug": 100000,
            "deadband_ug": 10000},
        "datasets": results, "passed": passed,
        "decision": "INCLUDE" if passed else "REJECT"}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(value, indent=2) + "\n",
        encoding="utf-8")
    print(json.dumps({"dataset_count": len(results), "passed": passed,
        "decision": value["decision"]}, indent=2))


if __name__ == "__main__":
    main()
