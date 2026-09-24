#!/usr/bin/env python3
"""Conditional offline comparison; no candidate is selected or qualified."""

import argparse
import json
import statistics
import subprocess
from collections import deque
from pathlib import Path

from analyze_opened_creep import ROOT, RUNS, read_pinned, second_series, replay_r5

CHECKPOINTS = (300, 600, 900, 1800)
TWELVE_HOUR = (
    "Results/stage5mr5d/20260917T182739Z_r5d_12h_500g/"
    "active_12h/samples.csv")
TWELVE_HOUR_SUMMARY = (
    "Results/stage5mr5d/20260917T182739Z_r5d_12h_500g/"
    "qualification_summary.json")


def median_window(series, start, end):
    values = [value for second, value in series if start <= second < end]
    return statistics.median(values) if values else None


class UnifiedRobustReference:
    """Fixed illustrative controller, not firmware logic or a fitted model."""

    def __init__(self):
        self.offset = 0
        self.reference = None
        self.initial = []
        self.window = deque(maxlen=60)
        self.previous_mass = None
        self.previous_second = None
        self.holdoff_until = 0
        self.rebuilds = 0
        self.rate_hz = 10
        self.filter_mode = 3

    def reset(self):
        self.__init__()

    def profile_change(self, second, mass, rate_hz=None, filter_mode=None):
        if rate_hz is not None:
            if rate_hz not in (10, 40):
                raise ValueError("unqualified rate")
            self.rate_hz = rate_hz
        if filter_mode is not None:
            if filter_mode not in range(4):
                raise ValueError("invalid filter mode")
            self.filter_mode = filter_mode
        self.window.clear()
        self.reference = mass - self.offset
        self.holdoff_until = second + 15
        self.rebuilds += 1

    def process(self, second, mass, mode="STATIC"):
        elapsed = 0 if self.previous_second is None else second - self.previous_second
        if elapsed < 0:
            raise ValueError("time moved backwards")
        if mode == "DOSING":
            self.previous_mass = mass
            self.previous_second = second
            return mass - self.offset, self.offset
        if self.previous_mass is not None and abs(mass - self.previous_mass) > 20_000_000:
            self.profile_change(second, mass)
        self.previous_mass = mass
        self.previous_second = second
        if self.reference is None and 15 <= second < 45:
            self.initial.append(mass)
        if self.reference is None and second >= 45 and self.initial:
            self.reference = statistics.median(self.initial)
        self.window.append(mass)
        if self.reference is None or second < self.holdoff_until or len(self.window) < 60:
            return mass - self.offset, self.offset
        center = statistics.median(self.window)
        if statistics.median(abs(value - center) for value in self.window) > 10_000:
            return mass - self.offset, self.offset
        target = max(-500_000, min(500_000, center - self.reference))
        step = min(50 * elapsed, abs(target - self.offset))
        self.offset += step if target > self.offset else -step
        return mass - self.offset, self.offset


def replay_unified(series):
    model = UnifiedRobustReference()
    corrected = []
    offsets = []
    for second, mass in series:
        value, offset = model.process(second, mass)
        corrected.append((second, value))
        offsets.append((second, offset))
    return corrected, offsets, model.rebuilds


def checkpoint_table(series, frozen, unified):
    anchor = median_window(series, 15, 45)
    frozen_values = [(s, value) for s, value, _ in frozen]
    results = {}
    for minute in (5, 10, 15, 30):
        start = minute * 60
        raw = median_window(series, start, start + 60)
        r5 = median_window(frozen_values, start, start + 60)
        robust = median_window(unified, start, start + 60)
        results[str(minute)] = {
            "raw_relative_ug": None if raw is None else round(raw - anchor),
            "frozen_r5_relative_ug": None if r5 is None else round(r5 - anchor),
            "unified_robust_relative_ug": None if robust is None else round(robust - anchor),
            "event_anchored_relative_ug": None,
            "event_anchored_status": "NOT_SCORABLE_NO_VERIFIED_LOAD_EVENT",
        }
    return results


def pinned_blob_id(relative):
    return subprocess.check_output(["git", "-C", str(ROOT), "rev-parse",
        f"d6312dd9898ba8f01a43cd83a76fa41546d88275:{relative}"],
        text=True).strip()


def analyze_opened(name, data_root):
    rows, series, _ = second_series(read_pinned(RUNS[name], data_root))
    frozen, rebases = replay_r5(series)
    unified, offsets, unified_rebuilds = replay_unified(series)
    return {
        "source": RUNS[name], "samples": rows,
        "time_origin": "RECORDER_START_NOT_LOAD_EVENT",
        "comparison": checkpoint_table(series, frozen, unified),
        "frozen_r5_rebuilds": rebases,
        "unified_rebuilds": unified_rebuilds,
        "unified_final_offset_ug": round(offsets[-1][1]),
    }


def analyze_twelve_hour(data_root):
    _, series, _ = second_series(read_pinned(TWELVE_HOUR, data_root))
    corrected, offsets, rebuilds = replay_unified(series)
    actual = json.loads(read_pinned(TWELVE_HOUR_SUMMARY, data_root))
    start = median_window(series, 0, 300)
    end = median_window(series, 42900, 43200)
    corrected_start = median_window(corrected, 0, 300)
    corrected_end = median_window(corrected, 42900, 43200)
    offset_by_second = dict(offsets)
    maximum_offset_10s_change = max(
        (abs(offset - offset_by_second[second - 10])
         for second, offset in offsets if second - 10 in offset_by_second),
        default=None)
    empty_baseline = actual["empty_baseline_last_5m"]["corrected_gross_ug"]
    unload = actual["unload"]["unloaded_last_5m"]
    return {
        "source": TWELVE_HOUR,
        "status": "CONDITIONAL_REPLAY_STARTS_AFTER_LOAD_AND_REFERENCE_BUILD",
        "event_anchored_status": "NOT_SCORABLE_NO_VERIFIED_LOAD_EVENT",
        "recorded_frozen_r5_uncompensated_drift_ug": round(
            actual["active_12h"]["last_5m"]["uncompensated_gross_ug"] -
            actual["active_12h"]["first_5m"]["uncompensated_gross_ug"]),
        "recorded_frozen_r5_corrected_drift_ug": round(
            actual["active_12h"]["last_5m"]["corrected_gross_ug"] -
            actual["active_12h"]["first_5m"]["corrected_gross_ug"]),
        "unified_raw_drift_ug": round(end - start),
        "unified_corrected_drift_ug": round(corrected_end - corrected_start),
        "unified_final_offset_ug": round(offsets[-1][1]),
        "unified_maximum_offset_10s_change_ug": round(maximum_offset_10s_change),
        "unified_rebuilds": rebuilds,
        "actual_dosing_offset_strictly_frozen": actual["unload"]["offset_strictly_frozen"],
        "actual_automatic_rebuilds": actual["active_12h"]["invariant_values"]["automatic_rebase_count"],
        "unified_dosing_step_loss_ug_by_frozen_offset": 0,
        "recorded_r5_unloaded_zero_residual_ug": round(
            unload["corrected_gross_ug"] - empty_baseline),
        "unified_frozen_offset_unloaded_zero_residual_ug": round(
            unload["uncompensated_gross_ug"] - offsets[-1][1] - empty_baseline),
        "unified_dosing_freeze": "SYNTHETIC_CONTRACT_ONLY",
        "loaded_event_time": "NOT RECORDED; active_12h starts with 500 g already placed",
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--data-root", type=Path)
    args = parser.parse_args()
    report = {
        "schema_version": 1,
        "classification": "OPENED_DEVELOPMENT_CONDITIONAL_COMPARISON_MODEL_SELECTION_INCOMPLETE",
        "input_commit": "d6312dd9898ba8f01a43cd83a76fa41546d88275",
        "definition": "Changes relative to 15-45 s median, not absolute mass error",
        "unified_fixed_assumptions": {
            "rolling_median_s": 60, "median_absolute_deviation_limit_ug": 10000,
            "offset_rate_limit_ug_per_s": 50, "offset_limit_ug": 500000,
            "step_threshold_ug": 20000000, "post_step_holdoff_s": 15,
            "starting_mode": "STATIC at first recorder record; no actual event inferred",
        },
        "input_git_blobs": {relative: pinned_blob_id(relative)
            for relative in (*RUNS.values(), TWELVE_HOUR, TWELVE_HOUR_SUMMARY)},
        "runs": {name: analyze_opened(name, args.data_root) for name in RUNS},
        "opened_12h": analyze_twelve_hour(args.data_root),
        "decision": "MODEL SELECTION INCOMPLETE",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8")
    print(json.dumps({"runs": len(report["runs"]),
                      "decision": report["decision"]}))


if __name__ == "__main__":
    main()
