#!/usr/bin/env python3
import argparse, csv, json, statistics, sys
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from display_model import CandidateConfig, IncrementalDisplay, quantize_count

RUNS = {
    "r5d_12h": "Results/stage5mr5d/20260917T182739Z_r5d_12h_500g/active_12h/samples.csv",
    "r5d_unload": "Results/stage5mr5d/20260917T182739Z_r5d_12h_500g/active_dosing_unload/samples.csv",
    "r5e_shadow": "Results/stage5mr5e/20260918T_r5e_local_control/shadow_to_tracking/samples.csv",
    "d1_cycle1": "Results/stage5mr5e_d1/20260919T_display_d1/cycle1/samples.csv",
    "d1_cycle2": "Results/stage5mr5e_d1/20260919T_display_d1/cycle2/samples.csv",
    "d1_cycle3": "Results/stage5mr5e_d1/20260919T_display_d1/cycle3/samples.csv",
}


def rows(path):
    with (ROOT / path).open(encoding="utf-8", newline="") as stream:
        yield from csv.DictReader(stream)


def analyze_series(records, config=None):
    model = IncrementalDisplay(config) if config else None
    output = []
    for index, row in enumerate(records):
        mass = int(row["net_mass_ug"])
        desired = quantize_count(mass, 1000000, 100, 1)
        now = int(row["uptime_ms"])
        stable = (int(row["status_flags"]) & 16) != 0
        active = int(row["application"]) == 1
        baseline = int(row["display_count"])
        actual = baseline if model is None or not active else model.process(
            desired, now, stable, active, baseline_count=baseline)["actual_display_count"]
        output.append({"now": now, "desired": desired, "actual": actual,
            "stable": stable, "active": active, "index": index})
    return metrics(output)


def metrics(values):
    changes = 0; aba = 0; maximum_jump = 0; maximum_lag = 0
    stale_over_1d = 0; stale_over_2d_5s = 0; stale_start = None
    windows = deque(); max_aba_10s = 0; previous = None; before = None
    for item in values:
        lag = abs(item["desired"] - item["actual"])
        maximum_lag = max(maximum_lag, lag)
        qualified = item.get("stable", True) and item.get("active", True)
        if qualified and lag > 1:
            stale_over_1d += 1
            if stale_start is None: stale_start = item["now"]
            if lag > 2 and item["now"] - stale_start > 5000:
                stale_over_2d_5s += 1
        else: stale_start = None
        if previous is not None:
            jump = abs(item["actual"] - previous["actual"])
            maximum_jump = max(maximum_jump, jump)
            if jump: changes += 1
            if before is not None and before["actual"] == item["actual"] and \
                    previous["actual"] != item["actual"]:
                aba += 1; windows.append(item["now"])
            while windows and item["now"] - windows[0] > 10000:
                windows.popleft()
            max_aba_10s = max(max_aba_10s, len(windows))
        before, previous = previous, item
    duration_min = max(1 / 60, (values[-1]["now"] - values[0]["now"]) / 60000)
    return {"records": len(values), "changes_per_minute": changes / duration_min,
        "aba_count": aba, "max_aba_10s": max_aba_10s,
        "maximum_jump_divisions": maximum_jump,
        "maximum_lag_divisions": maximum_lag,
        "stale_over_1d_records": stale_over_1d,
        "stale_over_2d_over_5s_records": stale_over_2d_5s,
        "final_mismatch": values[-1].get("stable", True) and
            values[-1].get("active", True) and
            values[-1]["desired"] != values[-1]["actual"]}


def synthetic(config, direction=1, active=True):
    model = IncrementalDisplay(config)
    values = []
    for index in range(900):
        # 30 s constant, then 0.22 g over 60 s at 10 Hz.
        delta = 0 if index < 300 else direction * ((index - 300) * 220000 // 600)
        mass = delta
        desired = quantize_count(mass, 1000000, 100, 1)
        actual = model.process(desired, index * 100, True, active)["actual_display_count"]
        values.append({"now": index * 100, "desired": desired, "actual": actual})
    return metrics(values)


def boundary_noise(config):
    model = IncrementalDisplay(config); values = []
    pattern = (4999, 5001, 4999, 5001, 4999, 5001, 4999, 5001, 4999, 5001)
    for index in range(600):
        mass = pattern[index % len(pattern)]
        desired = quantize_count(mass, 1000000, 100, 1)
        actual = model.process(desired, index * 100, True, True)["actual_display_count"]
        values.append({"now": index * 100, "desired": desired, "actual": actual})
    return metrics(values)


def large_step(config):
    model = IncrementalDisplay(config); values = []
    for index in range(100):
        desired = 0 if index < 20 else 50000 if index < 60 else 0
        actual = model.process(desired, index * 100, index not in (20, 60), True)["actual_display_count"]
        values.append((desired, actual))
    load_delay = next(i for i in range(20, 100) if values[i][1] == 50000) - 20
    unload_delay = next(i for i in range(60, 100) if values[i][1] == 0) - 60
    return {"load_delay_samples": load_delay, "unload_delay_samples": unload_delay,
        "step_loss_counts": (50000 - values[59][1]) + values[-1][1]}


def passes(result):
    slow = (result["synthetic_up"], result["synthetic_down"])
    regressions = result["runs"].values()
    return all(x["stale_over_1d_records"] == 0 and
        x["stale_over_2d_over_5s_records"] == 0 and
        x["maximum_jump_divisions"] <= 1 and not x["final_mismatch"] for x in slow) and \
        result["boundary"]["max_aba_10s"] <= 2 and \
        result["step"]["load_delay_samples"] <= 1 and \
        result["step"]["unload_delay_samples"] <= 1 and \
        result["step"]["step_loss_counts"] == 0 and \
        all(x["stale_over_2d_over_5s_records"] == 0 for x in regressions)


def main():
    parser = argparse.ArgumentParser(); parser.add_argument("--output", required=True)
    args = parser.parse_args(); output = Path(args.output); output.mkdir(parents=True, exist_ok=True)
    candidates = []
    for hysteresis in (1, 2):
        for confirmation in (1000, 2000, 3000, 5000):
            config = CandidateConfig(hysteresis, confirmation)
            result = {"config": config.__dict__,
                "runs": {name: analyze_series(list(rows(path)), config) for name, path in RUNS.items()},
                "synthetic_up": synthetic(config, 1), "synthetic_down": synthetic(config, -1),
                "boundary": boundary_noise(config), "step": large_step(config)}
            result["passed"] = passes(result); candidates.append(result)
    baseline = {name: analyze_series(list(rows(path))) for name, path in RUNS.items()}
    selected = next((item for item in candidates if item["passed"]), None)
    value = {"schema_version": 1, "baseline_8d": baseline,
        "candidates": candidates, "selected": selected,
        "result": "PASS" if selected else "NO_ACCEPTABLE_CANDIDATE"}
    (output / "candidate_results.json").write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"selected": selected["config"] if selected else None,
        "candidate_passes": sum(x["passed"] for x in candidates)}, indent=2))
    return 0 if selected else 2


if __name__ == "__main__": raise SystemExit(main())
