#!/usr/bin/env python3
import argparse
import csv
import itertools
import json
import statistics
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools" / "stage5ma_replay"))
from stage5ma_replay import RUNS, calibrated, delta32, load_rows, step_metrics
from low_ram_model import Config, LowRamAdaptive, FAST, SETTLING

DEVELOPMENT = ("filt0_empty", "filt0_load", "filt0_unload", "filt2_empty",
               "filt2_load", "filt2_unload", "slow_fill", "cold_control1")
OPENED = tuple(name for name in RUNS if name not in DEVELOPMENT)


def divround(value, divisor):
    sign = -1 if value < 0 else 1
    return sign * ((abs(value) + divisor // 2) // divisor)


class PrecisionFilter:
    def __init__(self):
        self.history = []
        self.output = None
    def process(self, raw):
        self.history.append(raw)
        self.history = self.history[-3:]
        if self.output is None:
            self.output = raw
        if len(self.history) == 3:
            median = sorted(self.history)[1]
            self.output += divround(median - self.output, 8)
        return self.output


def replay(name, config, process_active=False):
    _, rows = load_rows(RUNS[name][0])
    model = LowRamAdaptive(config)
    precision = PrecisionFilter()
    output = []
    for index, row in enumerate(rows):
        raw = int(row["raw_adc"])
        raw_mass = calibrated(raw)
        precise_mass = calibrated(precision.process(raw))
        result = model.process(raw_mass, precise_mass,
            int(row["mcu_uptime_ms"]), index + 1, process_active)
        output.append({**result, "display_mass_ug": result["candidate_mass_ug"],
            "calibrated_mass_ug": raw_mass,
            "timestamp_ms": int(row["mcu_uptime_ms"])})
    return output


def static_metrics(values):
    used = values[len(values) // 5:]
    mass = [row["candidate_mass_ug"] for row in used]
    return {"stddev_g": statistics.pstdev(mass) / 1e6,
        "motion_false_positive_ratio": sum(row["state"] in (FAST, SETTLING)
            for row in used) / len(used),
        "stable_ratio": sum(row["stable_candidate"] for row in used) / len(used),
        "maximum_switch_jump_g": max(abs(b["candidate_mass_ug"] -
            a["candidate_mass_ug"]) for a, b in zip(values, values[1:])) / 1e6}


def slow_metrics(values):
    start = values[0]["timestamp_ms"]
    window = [row for row in values if 10000 <=
        delta32(start, row["timestamp_ms"]) <= 70000]
    return {"false_stable_ratio": sum(row["stable_candidate"] for row in window) /
        len(window), "records": len(window)}


def metrics(config, opened=False):
    static_name = "filt3_empty" if opened else "filt2_empty"
    load_name = "filt3_load" if opened else "filt0_load"
    unload_name = "filt3_unload" if opened else "filt0_unload"
    slow_name = "faster_fill" if opened else "slow_fill"
    return {"static": static_metrics(replay(static_name, config)),
        "load": step_metrics(replay(load_name, config), "LOAD_STEP"),
        "unload": step_metrics(replay(unload_name, config), "UNLOAD_STEP"),
        "slow": slow_metrics(replay(slow_name, config)),
        "dosing": slow_metrics(replay(slow_name, config, True))}


def passed(value):
    return value["static"]["stddev_g"] <= .0068 and \
        value["static"]["motion_false_positive_ratio"] <= .01 and \
        (value["load"]["time_10_90_s"] or 99) <= .60 and \
        (value["unload"]["time_10_90_s"] or 99) <= .60 and \
        (value["load"]["stable_time_s"] or 99) <= 3.0 and \
        value["slow"]["false_stable_ratio"] <= .10 and \
        value["dosing"]["false_stable_ratio"] == 0 and \
        value["load"]["overshoot_g"] <= .05 and \
        value["unload"]["overshoot_g"] <= .05


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    output = Path(args.output); output.mkdir(parents=True, exist_ok=True)
    candidates = []
    for innovation, motion, settle, quiet, stable in itertools.product(
            (30000, 50000, 80000), (5000, 10000, 15000),
            (5000, 10000, 20000), (2, 3, 5), (3, 5, 8)):
        config = Config(innovation_threshold_ug=innovation,
            motion_threshold_ug=motion, settle_threshold_ug=settle,
            quiet_samples=quiet, stable_samples=stable)
        value = metrics(config)
        score = ((value["static"]["stddev_g"] / .0068) +
            ((value["load"]["time_10_90_s"] or 9) / .6) +
            ((value["unload"]["time_10_90_s"] or 9) / .6) +
            ((value["load"]["stable_time_s"] or 9) / 3) +
            value["slow"]["false_stable_ratio"] / .1 +
            value["static"]["motion_false_positive_ratio"] / .01)
        candidates.append({"config": config.__dict__, "metrics": value,
                           "passed": passed(value), "score": score})
    candidates.sort(key=lambda item: (not item["passed"], item["score"]))
    selected = candidates[0]
    selected["opened_regression"] = metrics(Config(**selected["config"]), True)
    selected["opened_regression_passed"] = passed(selected["opened_regression"])
    (output / "development_search.json").write_text(json.dumps({
        "schema_version": 1, "candidate_count": len(candidates),
        "development_runs": DEVELOPMENT, "opened_regression_runs": OPENED,
        "top_10": candidates[:10]}, indent=2) + "\n", encoding="utf-8")
    (output / "selected_development_candidate.json").write_text(
        json.dumps(selected, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(selected, indent=2))
    return 0 if selected["passed"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
