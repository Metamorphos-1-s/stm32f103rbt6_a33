#!/usr/bin/env python3
import argparse
import json
import statistics
from pathlib import Path

from reference_lock_model import Mode, ReferenceLock, csv_seconds, ols_g_per_h

ROOT = Path(__file__).resolve().parents[2]
RUNS = (
    ("cold_empty_1", "20260915T_r4_cold_empty_1", None),
    ("cold_empty_2", "20260915T_r4_cold_empty_2", None),
    ("hot_empty", "20260915T_r4_hot_empty", None),
    ("loaded_500g_1", "20260916T_r4_500g_constant_1", None),
    ("loaded_500g_2", "20260916T_r4_500g_constant_2", None),
    ("loaded_1kg", "20260916T_r4_1kg_constant", "2026-09-16T06:00:11.791Z"),
)


def improvement(before, after):
    return 1.0 - abs(after) / abs(before)


def endpoint(rows):
    groups = {}
    for second, value in rows:
        groups.setdefault(second // 60, []).append(value)
    blocks = [statistics.median(values) for _, values in sorted(groups.items()) if len(values) >= 48]
    return (blocks[-1] - blocks[0]) / 1e6


def replay_run(name, run_id, start_utc):
    source = csv_seconds(ROOT / "Results" / "stage5mr4" / run_id / "samples.csv", start_utc)
    model = ReferenceLock()
    model.set_mode(Mode.STATIC_COMPENSATION)
    output = []
    for second, mass in source:
        snap = model.process_second(second, mass)
        output.append((second, mass, snap["corrected_gross_ug"], snap["offset_ug"]))
    raw = [(row[0], row[1]) for row in output]
    corrected = [(row[0], row[2]) for row in output]
    max10 = max((abs(output[index][3] - output[index - 10][3]) for index in range(10, len(output))), default=0)
    raw_slope = ols_g_per_h(raw)
    corrected_slope = ols_g_per_h(corrected)
    raw_endpoint = endpoint(raw)
    corrected_endpoint = endpoint(corrected)
    return {
        "name": name, "run_id": run_id,
        "raw_ols_g_per_h": raw_slope,
        "corrected_ols_g_per_h": corrected_slope,
        "ols_improvement_fraction": improvement(raw_slope, corrected_slope),
        "raw_endpoint_g": raw_endpoint,
        "corrected_endpoint_g": corrected_endpoint,
        "endpoint_improvement_fraction": improvement(raw_endpoint, corrected_endpoint),
        "final_offset_g": model.offset_ug / 1e6,
        "maximum_10s_offset_change_g": max10 / 1e6,
        "automatic_rebase_count": model.automatic_rebase_count,
        "evaluation_count": model.evaluation_count,
    }


def synthetic_12h():
    model = ReferenceLock()
    model.set_mode(Mode.STATIC_COMPENSATION)
    output = []
    duration = 12 * 3600
    for second in range(duration + 1):
        mass = 500_000_000 + round(220_000 * second / duration)
        snap = model.process_second(second, mass)
        output.append((second, mass, snap))
    model.set_mode(Mode.DOSING_NO_COMPENSATION)
    unload = model.process_second(duration + 1, 220_000)
    max10 = max(abs(output[index][2]["offset_ug"] - output[index - 10][2]["offset_ug"]) for index in range(10, len(output)))
    return {
        "classification": "SYNTHETIC_NOT_HARDWARE_VALIDATION",
        "final_offset_g": model.offset_ug / 1e6,
        "loaded_residual_g": output[-1][2]["corrected_gross_ug"] / 1e6 - 500,
        "unloaded_residual_g": unload["corrected_gross_ug"] / 1e6,
        "loaded_display_g_at_e_0_05": round(output[-1][2]["corrected_gross_ug"] / 50_000) * 0.05,
        "unloaded_display_g_at_e_0_05": round(unload["corrected_gross_ug"] / 50_000) * 0.05,
        "maximum_10s_offset_change_g": max10 / 1e6,
    }


def protection():
    cycle = csv_seconds(ROOT / "Results/stage5mr4/20260916T_r4_load_unload_cycles_5/samples.csv")
    dosing = ReferenceLock(); dosing.offset_ug = 200_000; dosing.set_mode(Mode.DOSING_NO_COMPENSATION)
    start = dosing.offset_ug
    corrected = []
    for second, mass in cycle:
        corrected.append(dosing.process_second(second, mass)["corrected_gross_ug"])
    static = ReferenceLock(); static.set_mode(Mode.STATIC_COMPENSATION)
    for second, mass in cycle:
        static.process_second(second, mass)
    slow_cases = []
    for increment in (100, 1000, 5000, 20000):
        for pause in (2, 5, 10):
            model = ReferenceLock(); model.offset_ug = 200_000; model.set_mode(Mode.DOSING_NO_COMPENSATION)
            initial = model.offset_ug
            mass = 0
            for second in range(601):
                if second and second % pause == 0:
                    mass += increment
                final = model.process_second(second, mass)
            slow_cases.append({"increment_g": increment/1e6, "pause_s": pause,
                "input_gain_g": mass/1e6,
                "output_gain_g": (final["corrected_gross_ug"] - (-initial))/1e6,
                "offset_change_g": (model.offset_ug-initial)/1e6})
    return {
        "dosing_offset_change_g": (dosing.offset_ug-start)/1e6,
        "dosing_dynamic_range_loss_g": ((max(cycle, key=lambda x:x[1])[1]-min(cycle,key=lambda x:x[1])[1])-(max(corrected)-min(corrected)))/1e6,
        "existing_0_20g_offset_load_difference_g": (max(corrected)-min(corrected))/1e6,
        "automatic_rebase_count": static.automatic_rebase_count,
        "physical_edge_count": 10,
        "slow_cases": slow_cases,
        "slow_case_failures": [case for case in slow_cases if case["offset_change_g"] != 0 or abs(case["output_gain_g"]-case["input_gain_g"]) > 1e-12],
    }


def main():
    parser=argparse.ArgumentParser(); parser.add_argument("--output", required=True); args=parser.parse_args()
    runs=[replay_run(*item) for item in RUNS]
    report={"algorithm":"R5_REFERENCE_LOCK_FROZEN","run_count":len(runs),"runs":runs,
        "median_ols_improvement_fraction":statistics.median(r["ols_improvement_fraction"] for r in runs),
        "median_endpoint_improvement_fraction":statistics.median(r["endpoint_improvement_fraction"] for r in runs),
        "minimum_endpoint_improvement_fraction":min(r["endpoint_improvement_fraction"] for r in runs),
        "formal_reverse_amplification_count":sum(abs(r["corrected_ols_g_per_h"]) > abs(r["raw_ols_g_per_h"])+0.002 for r in runs),
        "maximum_10s_offset_change_g":max(r["maximum_10s_offset_change_g"] for r in runs),
        "synthetic_12h":synthetic_12h(),"protection":protection()}
    output=Path(args.output); output.mkdir(parents=True,exist_ok=False)
    (output/"offline_report.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(report,indent=2)); return 0


if __name__=="__main__": raise SystemExit(main())
