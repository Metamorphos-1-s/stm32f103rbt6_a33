#!/usr/bin/env python3
"""Finite, auditable D1-C parameter search and regression scoring."""

import argparse
import csv
import hashlib
import itertools
import json
import sys
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from directional_model import DirectionalConfig, DirectionalDisplay, quantize_count


RUNS = {
    "r5d_12h": "Results/stage5mr5d/20260917T182739Z_r5d_12h_500g/active_12h/samples.csv",
    "r5d_unload": "Results/stage5mr5d/20260917T182739Z_r5d_12h_500g/active_dosing_unload/samples.csv",
    "r5e_shadow": "Results/stage5mr5e/20260918T_r5e_local_control/shadow_to_tracking/samples.csv",
    "d1_cycle1": "Results/stage5mr5e_d1/20260919T_display_d1/cycle1/samples.csv",
    "d1_cycle2": "Results/stage5mr5e_d1/20260919T_display_d1/cycle2/samples.csv",
    "d1_cycle3": "Results/stage5mr5e_d1/20260919T_display_d1/cycle3/samples.csv",
}
D1B = "Results/stage5mr5e_d1b/20260920T_low_ram_display_candidate/slow_display_gate/samples.csv"


def read_rows(relative):
    with (ROOT / relative).open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def sha256(relative):
    return hashlib.sha256((ROOT / relative).read_bytes()).hexdigest().upper()


def context(row):
    mass = int(row["net_mass_ug"])
    decimals = int(row.get("display_decimals", 2))
    desired = int(row.get("desired_display_count",
        quantize_count(mass, 1000000, 10 ** decimals, 1)))
    stable = bool(int(row.get("stable",
        1 if int(row.get("status_flags", 0)) & 16 else 0)))
    active = int(row.get("application", 0)) == 1
    source = (int(row.get("display_page", 0)) << 2) | int(row.get("unit", 0))
    return desired, stable, active, source


def replay(records, config, force_active=None):
    model = DirectionalDisplay(config)
    trace = []
    for index, row in enumerate(records):
        desired, stable, active, source = context(row)
        if force_active is not None:
            active = force_active
        baseline = int(row["display_count"])
        result = model.process(desired, stable, active,
            baseline_count=baseline, source=source)
        trace.append({"index": index, "uptime_ms": int(row["uptime_ms"]),
            "sample_sequence": int(row.get("sample_sequence", index)),
            "authoritative_display_input_ug": int(row["net_mass_ug"]),
            "baseline_count": baseline, **result})
    return trace


def changes(trace):
    return [trace[index] for index in range(1, len(trace))
        if trace[index]["current_count"] != trace[index - 1]["current_count"]]


def series_metrics(trace):
    changed = changes(trace); aba = 0; window = deque(); max_aba = 0
    max_jump = 0; max_lag = 0; stale_1 = 0; stale_2_5s = 0
    lag2_start = None; counts = [row["current_count"] for row in trace]
    transition_violations = 0
    for index, row in enumerate(trace):
        lag = abs(row["desired_count"] - row["current_count"])
        max_lag = max(max_lag, lag)
        qualified = bool(row["stable"] and row["active"])
        if qualified and lag > 1:
            stale_1 += 1
        if qualified and lag > 2:
            if lag2_start is None:
                lag2_start = row["uptime_ms"]
            if row["uptime_ms"] - lag2_start > 5000:
                stale_2_5s += 1
        else:
            lag2_start = None
        if index:
            jump = row["current_count"] - trace[index - 1]["current_count"]
            pre_delta = row["desired_count"] - trace[index - 1]["current_count"]
            max_jump = max(max_jump, abs(jump))
            if jump:
                valid_step = abs(jump) <= 1 and \
                    ((jump > 0) == (pre_delta > 0))
                valid_release = abs(pre_delta) > 8 and \
                    row["current_count"] == row["desired_count"]
                if not (valid_step or valid_release):
                    transition_violations += 1
        if index >= 2 and trace[index - 2]["current_count"] == \
                row["current_count"] and trace[index - 1]["current_count"] != \
                row["current_count"]:
            aba += 1; window.append(row["uptime_ms"])
        while window and row["uptime_ms"] - window[0] > 10000:
            window.popleft()
        max_aba = max(max_aba, len(window))
    duration_min = max(1 / 60,
        (trace[-1]["uptime_ms"] - trace[0]["uptime_ms"]) / 60000)
    return {"records": len(trace), "updates": len(changed),
        "changes_per_minute": len(changed) / duration_min,
        "aba_count": aba, "max_aba_10s": max_aba,
        "peak_to_peak_divisions": max(counts) - min(counts),
        "maximum_jump_divisions": max_jump,
        "transition_violations": transition_violations,
        "maximum_lag_divisions": max_lag,
        "stale_over_1d_records": stale_1,
        "stale_over_2d_over_5s_records": stale_2_5s,
        "final_error_divisions": abs(trace[-1]["desired_count"] -
            trace[-1]["current_count"])}


def d1b_metrics(config, records):
    trace = replay(records, config, force_active=True)
    changed = changes(trace); initial = trace[0]["current_count"]
    correct = [row for row in changed if row["current_count"] < initial]
    wrong = 0; max_jump = 0
    for index in range(1, len(trace)):
        jump = trace[index]["current_count"] - trace[index - 1]["current_count"]
        pre_update_delta = trace[index]["desired_count"] - \
            trace[index - 1]["current_count"]
        max_jump = max(max_jump, abs(jump))
        if jump and (jump > 0) != (pre_update_delta > 0):
            wrong += 1
    first_ms = None if not correct else correct[0]["uptime_ms"] - trace[0]["uptime_ms"]
    result = {"records": len(trace), "updates": len(changed),
        "first_correct_update_ms": first_ms,
        "final_error_divisions": abs(trace[-1]["desired_count"] -
            trace[-1]["current_count"]), "wrong_direction_updates": wrong,
        "maximum_jump_divisions": max_jump,
        "final_display_count": trace[-1]["current_count"],
        "final_desired_count": trace[-1]["desired_count"]}
    result["passed"] = len(changed) > 0 and first_ms is not None and \
        first_ms <= 5000 and result["final_error_divisions"] <= 1 and \
        wrong == 0 and max_jump <= 1
    return result, trace


def synthetic_trace(config, desired_values, interval_ms=100, baseline=0):
    model = DirectionalDisplay(config); trace = []
    for index, desired in enumerate(desired_values):
        result = model.process(desired, True, True, baseline_count=baseline)
        trace.append({"index": index, "uptime_ms": index * interval_ms,
            "sample_sequence": index, "authoritative_display_input_ug": 0,
            "baseline_count": baseline, **result})
    return trace


def slow_metrics(config, direction):
    desired = [0 if index < 100 else direction *
        ((index - 100) * 22 // 600) for index in range(700)]
    trace = synthetic_trace(config, desired)
    changed = changes(trace)
    first_expected = next(index for index, value in enumerate(desired) if value)
    first_correct = next((row["index"] for row in changed
        if (row["current_count"] * direction) > 0), None)
    metric = series_metrics(trace)
    metric["first_correct_update_ms"] = None if first_correct is None else \
        (first_correct - first_expected) * 100
    metric["passed"] = metric["first_correct_update_ms"] is not None and \
        metric["first_correct_update_ms"] <= 5000 and \
        metric["stale_over_1d_records"] == 0 and \
        metric["stale_over_2d_over_5s_records"] == 0 and \
        metric["maximum_jump_divisions"] <= 1 and \
        metric["final_error_divisions"] <= 1
    return metric


def boundary_metrics(config):
    desired = [0 if index % 2 == 0 else 1 for index in range(600)]
    trace = synthetic_trace(config, desired)
    metric = series_metrics(trace)
    metric["passed"] = metric["max_aba_10s"] <= 2 and \
        metric["peak_to_peak_divisions"] <= 1 and \
        trace[-1]["current_count"] == trace[0]["current_count"]
    return metric


def reversal_metrics(config):
    desired = [0] * 20 + [4] * 80 + [-4] * 100
    trace = synthetic_trace(config, desired)
    reversal_index = 100; old_updates = 0; response = None
    for index in range(reversal_index + 1, len(trace)):
        jump = trace[index]["current_count"] - trace[index - 1]["current_count"]
        if jump > 0:
            old_updates += jump
        if jump < 0 and response is None:
            response = (index - reversal_index) * 100
    metric = series_metrics(trace)
    metric.update({"wrong_direction_extra_divisions": old_updates,
        "new_direction_response_ms": response})
    metric["passed"] = old_updates <= 1 and response is not None and \
        response <= 5000 and metric["maximum_jump_divisions"] <= 1
    return metric


def step_metrics(config):
    desired = [0] * 20 + [50000] * 40 + [0] * 40
    trace = synthetic_trace(config, desired)
    load = next(index for index in range(20, len(trace))
        if trace[index]["current_count"] == 50000) - 20
    unload = next(index for index in range(60, len(trace))
        if trace[index]["current_count"] == 0) - 60
    result = {"load_delay_samples": load, "unload_delay_samples": unload,
        "step_loss_counts": (50000 - trace[59]["current_count"]) +
            trace[-1]["current_count"],
        "maximum_jump_counts": max(abs(trace[index]["current_count"] -
            trace[index - 1]["current_count"]) for index in range(1, len(trace)))}
    result["passed"] = load <= 1 and unload <= 1 and \
        result["step_loss_counts"] == 0
    return result


def evaluate(config, d1b, runs):
    d1b_result, _ = d1b_metrics(config, d1b)
    run_results = {name: series_metrics(replay(records, config))
        for name, records in runs.items()}
    frozen = {name: all(row["current_count"] == int(records[index]["display_count"])
        for index, row in enumerate(replay(records, config)))
        for name, records in runs.items() if all(int(row["application"]) == 0
            for row in records)}
    result = {"config": config.__dict__, "d1b_failure": d1b_result,
        "synthetic_up": slow_metrics(config, 1),
        "synthetic_down": slow_metrics(config, -1),
        "boundary_noise": boundary_metrics(config),
        "direction_reversal": reversal_metrics(config),
        "large_step": step_metrics(config), "runs": run_results,
        "frozen_passthrough": frozen}
    result["passed"] = d1b_result["passed"] and \
        result["synthetic_up"]["passed"] and \
        result["synthetic_down"]["passed"] and \
        result["boundary_noise"]["passed"] and \
        result["direction_reversal"]["passed"] and \
        result["large_step"]["passed"] and all(frozen.values()) and \
        all(metric["transition_violations"] == 0 and
            metric["max_aba_10s"] <= 2 for metric in run_results.values())
    return result


def rank_key(item):
    c = item["config"]
    return (not item["passed"],
        item["d1b_failure"]["final_error_divisions"],
        item["boundary_noise"]["max_aba_10s"],
        item["large_step"]["load_delay_samples"] +
            item["large_step"]["unload_delay_samples"],
        c["threshold"], c["increment"], c["reverse_cancel"],
        c["zero_leak"], c["retain_remainder"])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(); args.output.mkdir(parents=True, exist_ok=True)
    d1b = read_rows(D1B)
    runs = {name: read_rows(path) for name, path in RUNS.items()}
    candidates = []
    for values in itertools.product((1, 2, 3), (1, 2), (1, 2, 3),
            (5, 8, 10, 12, 15, 20), (False, True)):
        candidates.append(evaluate(DirectionalConfig(*values), d1b, runs))
    candidates.sort(key=rank_key)
    selected = next((item for item in candidates if item["passed"]), None)
    inventory = {D1B: sha256(D1B), **{path: sha256(path)
        for path in RUNS.values()}}
    summary = {"schema_version": 1, "search_space": 216,
        "candidate_passes": sum(item["passed"] for item in candidates),
        "selected": selected, "ranking": candidates,
        "dataset_sha256": inventory,
        "result": "PASS" if selected else "NO_ACCEPTABLE_CANDIDATE"}
    (args.output / "candidate_results.json").write_text(
        json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    if selected:
        config = DirectionalConfig(**selected["config"])
        _, trace = d1b_metrics(config, d1b)
        with (args.output / "d1b_147_sample_replay.csv").open(
                "w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=trace[0].keys(),
                lineterminator="\n")
            writer.writeheader(); writer.writerows(trace)
    print(json.dumps({"result": summary["result"],
        "candidate_passes": summary["candidate_passes"],
        "selected": None if selected is None else selected["config"],
        "d1b": None if selected is None else selected["d1b_failure"],
        "boundary": None if selected is None else selected["boundary_noise"],
        "reversal": None if selected is None else selected["direction_reversal"],
        "step": None if selected is None else selected["large_step"]}, indent=2))
    return 0 if selected else 2


if __name__ == "__main__":
    raise SystemExit(main())
