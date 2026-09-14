#!/usr/bin/env python3
"""Stage 5M-R2 offline evaluator for an explicitly mode-gated drift compensator.

This module is not linked into product firmware.  It uses only immutable Stage 5L
CSV evidence and synthetic load sequences.
"""
from __future__ import annotations
import argparse
import csv
import json
import math
import statistics
from dataclasses import asdict, dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

@dataclass(frozen=True)
class Config:
    slow_window_s: int = 300
    edge_s: int = 15
    drift_deadband_ug: int = 8000
    max_drift_g_per_h: float = 0.2
    direction_consistency: float = 0.3
    step_block_s: int = 3
    step_threshold_ug: int = 20000
    step_confirmations: int = 2
    hold_off_s: int = 15
    maximum_update_ug_per_s: int = 50

CONFIG = Config()
STATIC_PATHS = {
    "cold_start_1": "Results/stage5lr_hardware/20260914_control1_10hz_cold_60m/samples.csv",
    "cold_start_2": "Results/stage5lr_hardware/20260914_control2_10hz_cold_60m/samples.csv",
    "loaded_500g": "Results/stage5l_characterization/20260913T064617Z_creep_500g_30m/samples.csv",
}
REAL_LOAD_PATHS = {
    "load_step": "Results/stage5l_characterization/20260913_filter_compare/filt1_load_step/samples.csv",
    "unload_step": "Results/stage5l_characterization/20260913_filter_compare/filt1_unload_step/samples.csv",
    "drip_fast": "Results/stage5l_characterization/20260913_slow_fill/faster_continuous/samples.csv",
    "drip_slow": "Results/stage5l_characterization/20260913_slow_fill/slow_continuous/samples.csv",
}
SOURCE_BLOBS = {
    "cold_start_1": "4a19107cff0d8bae9a81d10a8d54b2e81803e7e4",
    "cold_start_2": "d96f2ce7c95573d84ce552331dc92b09529ba6da",
    "loaded_500g": "2afd2857a4d3f0fe8b9ca4a7d3bdfeff47fab6aa",
    "load_step": "c8ac45bf69820d489c12de0ff6d8150b0fa61ffa",
    "unload_step": "04800e02a3173c7dad95a39ffb6765d6fb69ceb9",
    "drip_fast": "51ed67605052d29a829e7dc335ea0740c45bebdb",
    "drip_slow": "1481669622eee50491f14e3ad1f20266442b485b",
}

def median(values):
    return statistics.median(values)

def load_seconds(relative_path):
    with (ROOT / relative_path).open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    raw = [(int(row["mcu_uptime_ms"]), int(row["uncompensated_gross_ug"])) for row in rows]
    origin = raw[0][0]
    buckets = {}
    for stamp, mass in raw:
        buckets.setdefault((stamp - origin) // 1000, []).append(mass)
    return [(second, median(values)) for second, values in sorted(buckets.items())]

def direction_consistency(block_medians, sign):
    if len(block_medians) < 2 or sign == 0:
        return 0.0
    aligned = sum(
        1
        for left, right in zip(block_medians, block_medians[1:])
        if math.copysign(1, right - left) == sign and right != left
    )
    # Flat blocks remain in the denominator.  This rejects a short burst followed
    # by a long plateau; omitting them caused the earlier false positive.
    return aligned / (len(block_medians) - 1)

class ModeGatedCompensator:
    def __init__(self, config=CONFIG, initial_offset_ug=0):
        self.c = config
        self.offset = float(initial_offset_ug)
        self.target = float(initial_offset_ug)
        self.anchor = None
        self.previous_mode = None
        self.hold_until = -1
        self.step_values = []
        self.slow_values = []
        self.step_count = 0
        self.step_sign = 0
        self.events = 0
        self.updates = 0
        self.releases = 0

    def _mode_change(self, enabled, second):
        if self.previous_mode is None:
            self.previous_mode = enabled
            return
        if enabled == self.previous_mode:
            return
        self.target = self.offset
        self.anchor = None
        self.step_values.clear()
        self.slow_values.clear()
        self.step_count = 0
        self.step_sign = 0
        if enabled:
            self.hold_until = second + self.c.hold_off_s
        self.previous_mode = enabled

    def process(self, second, mass_ug, enabled):
        self._mode_change(enabled, second)
        if not enabled:
            return mass_ug - self.offset

        limit = self.c.maximum_update_ug_per_s
        if self.target > self.offset:
            self.offset = min(self.target, self.offset + limit)
        elif self.target < self.offset:
            self.offset = max(self.target, self.offset - limit)

        self.step_values.append(mass_ug)
        block = self.c.step_block_s
        self.step_values = self.step_values[-2 * block :]
        step = False
        if len(self.step_values) == 2 * block:
            delta = median(self.step_values[block:]) - median(self.step_values[:block])
            sign = 1 if delta > 0 else -1 if delta < 0 else 0
            if abs(delta) >= self.c.step_threshold_ug:
                if sign == self.step_sign:
                    self.step_count += 1
                else:
                    self.step_count, self.step_sign = 1, sign
                step = self.step_count >= self.c.step_confirmations
            else:
                self.step_count = self.step_sign = 0

        if step:
            self.target = self.offset
            self.anchor = None
            self.step_values.clear()
            self.slow_values.clear()
            self.step_count = self.step_sign = 0
            self.hold_until = second + self.c.hold_off_s
            self.events += 1
            return mass_ug - self.offset

        if second < self.hold_until:
            return mass_ug - self.offset

        self.slow_values.append(mass_ug)
        if len(self.slow_values) < self.c.slow_window_s:
            return mass_ug - self.offset

        edge = self.c.edge_s
        first = median(self.slow_values[:edge])
        last = median(self.slow_values[-edge:])
        if self.anchor is None:
            self.anchor = first - self.offset
        delta = last - first
        desired = last - self.anchor
        rate = abs(delta) * 3600.0 / (self.c.slow_window_s * 1_000_000)
        block_medians = [
            median(self.slow_values[index : index + edge])
            for index in range(0, len(self.slow_values) - edge + 1, edge)
        ]
        sign = 1 if delta > 0 else -1 if delta < 0 else 0
        consistency = direction_consistency(block_medians, sign)
        high_rate = rate > self.c.max_drift_g_per_h
        valid_drift = (
            abs(delta) >= self.c.drift_deadband_ug
            and not high_rate
            and consistency >= self.c.direction_consistency
        )
        if valid_drift:
            self.target = desired
            self.updates += 1
        elif high_rate:
            self.anchor = last - self.offset
            self.target = self.offset
            self.events += 1
        elif (self.target or self.offset) and abs(desired) < abs(self.target):
            self.target = desired if (desired == 0 or math.copysign(1, desired) == math.copysign(1, self.target)) else 0
            self.releases += 1
        self.slow_values.clear()
        return mass_ug - self.offset

def replay(samples, initial_offset_ug=0):
    model = ModeGatedCompensator(initial_offset_ug=initial_offset_ug)
    output = []
    for second, mass, enabled in samples:
        corrected = model.process(second, mass, enabled)
        output.append((second, mass, corrected, model.offset, enabled))
    max_ten_second = max(
        (abs(output[index][3] - output[index - 10][3]) for index in range(10, len(output))),
        default=0,
    )
    return model, output, max_ten_second

def ols_g_per_h(points, field):
    xs = [point[0] for point in points]
    ys = [point[field] for point in points]
    xmean, ymean = statistics.fmean(xs), statistics.fmean(ys)
    denominator = sum((value - xmean) ** 2 for value in xs)
    return sum((x - xmean) * (y - ymean) for x, y in zip(xs, ys)) / denominator * 3600 / 1_000_000

def block_metrics(points, field, width=60, minimum_coverage=0.8):
    groups = {}
    for point in points:
        groups.setdefault(point[0] // width, []).append(point[field])
    values = [
        median(group)
        for _, group in sorted(groups.items())
        if len(group) >= math.ceil(width * minimum_coverage)
    ]
    return {
        "blocks": len(values),
        "endpoint_g": (values[-1] - values[0]) / 1_000_000,
        "range_g": (max(values) - min(values)) / 1_000_000,
    }

def static_case(relative_path):
    source = load_seconds(relative_path)
    robust_final = median([mass for _, mass in source[-60:]])
    samples = [(second, mass, True) for second, mass in source]
    last = samples[-1][0]
    samples.extend((last + i, robust_final, True) for i in range(1, 1801))
    model, output, max_ten = replay(samples)
    original = output[: len(source)]
    raw = [(second, mass) for second, mass in source]
    raw_blocks = block_metrics(raw, 1)
    corrected_blocks = block_metrics(original, 2)
    extended_blocks = block_metrics(output, 2)
    return {
        "input_slope_g_per_h": ols_g_per_h(raw, 1),
        "corrected_slope_g_per_h": ols_g_per_h(original, 2),
        "input_endpoint_g": raw_blocks["endpoint_g"],
        "corrected_endpoint_g": corrected_blocks["endpoint_g"],
        "input_range_g": raw_blocks["range_g"],
        "corrected_range_g": corrected_blocks["range_g"],
        "extended_endpoint_g": extended_blocks["endpoint_g"],
        "final_offset_g": model.offset / 1_000_000,
        "final_target_g": model.target / 1_000_000,
        "update_count": model.updates,
        "release_count": model.releases,
        "maximum_10s_update_g": max_ten / 1_000_000,
    }

def real_mode_case(relative_path, initial_offset_ug):
    source = load_seconds(relative_path)
    final_mass = median([mass for _, mass in source[-60:]])
    samples = [(second, mass, False) for second, mass in source]
    last = samples[-1][0]
    samples.extend((last + i, final_mass, True) for i in range(1, 1801))
    model, output, max_ten = replay(samples, initial_offset_ug)
    return {
        "initial_offset_g": initial_offset_ug / 1_000_000,
        "final_offset_g": model.offset / 1_000_000,
        "offset_change_g": (model.offset - initial_offset_ug) / 1_000_000,
        "maximum_10s_update_g": max_ten / 1_000_000,
        "update_count": model.updates,
        "disabled_offset_frozen": all(
            output[index][3] == output[index - 1][3]
            for index in range(1, len(source))
        ),
    }

def synthetic_mode_case(increment_g, pause_s, feed_s, initial_offset_ug):
    mass = 0
    samples = []
    for second in range(feed_s + 1801):
        enabled = second > feed_s
        if not enabled and second and second % pause_s == 0:
            mass += round(increment_g * 1_000_000)
        samples.append((second, mass, enabled))
    model, output, max_ten = replay(samples, initial_offset_ug)
    return {
        "increment_g": increment_g,
        "pause_s": pause_s,
        "feed_s": feed_s,
        "total_load_g": mass / 1_000_000,
        "initial_offset_g": initial_offset_ug / 1_000_000,
        "offset_change_g": (model.offset - initial_offset_ug) / 1_000_000,
        "maximum_10s_update_g": max_ten / 1_000_000,
        "update_count": model.updates,
        "disabled_offset_frozen": all(
            output[index][3] == output[index - 1][3]
            for index in range(1, feed_s + 1)
        ),
    }

def build_report():
    static = {name: static_case(path) for name, path in STATIC_PATHS.items()}
    static_detail = {}
    for name, value in static.items():
        static_detail[name] = {
            "slope": abs(value["corrected_slope_g_per_h"]) <= abs(value["input_slope_g_per_h"]) + 0.002,
            "endpoint": abs(value["corrected_endpoint_g"]) <= abs(value["input_endpoint_g"]) + 0.002,
            "range": value["corrected_range_g"] <= value["input_range_g"] + 0.002,
            "constant_tail": abs(value["extended_endpoint_g"]) <= abs(value["input_endpoint_g"]) + 0.002,
        }
    static_improvements = sum(
        abs(value["corrected_slope_g_per_h"]) < 0.9 * abs(value["input_slope_g_per_h"])
        for value in static.values()
    )
    real = []
    for name, path in REAL_LOAD_PATHS.items():
        for initial in (0, 5000, -5000):
            real.append({"case": name, **real_mode_case(path, initial)})
    synthetic = [
        synthetic_mode_case(increment, pause, duration, initial)
        for increment in (0.0001, 0.0005, 0.001, 0.002, 0.005, 0.01, 0.02)
        for pause in (2, 5, 10)
        for duration in (60, 120, 240, 600)
        for initial in (0, 5000, -5000)
    ]
    gates = {
        "static_non_amplification": all(all(item.values()) for item in static_detail.values()),
        "static_improvement_in_at_least_two_controls": static_improvements >= 2,
        "real_mode_gated_zero_loss": all(item["offset_change_g"] == 0 for item in real),
        "synthetic_mode_gated_zero_loss": all(item["offset_change_g"] == 0 for item in synthetic),
        "disabled_offset_frozen": all(item["disabled_offset_frozen"] for item in real + synthetic),
        "no_post_enable_chase": all(item["update_count"] == 0 for item in real + synthetic),
        "ten_second_update_budget": all(item["maximum_10s_update_g"] <= 0.001 for item in real + synthetic)
            and all(item["maximum_10s_update_g"] <= 0.001 for item in static.values()),
    }
    return {
        "status": "PRELIMINARY_OFFLINE_CANDIDATE_MODE_GATED" if all(gates.values()) else "OFFLINE_CANDIDATE_REJECTED",
        "source_commit": "de7f610cc5677b18337d477ec781e66217db2db0",
        "source_blobs": SOURCE_BLOBS,
        "config": asdict(CONFIG),
        "static": static,
        "static_gates": static_detail,
        "mode_gated_real_cases": real,
        "mode_gated_synthetic_summary": {
            "case_count": len(synthetic),
            "minimum_increment_g": 0.0001,
            "pauses_s": [2, 5, 10],
            "feed_durations_s": [60, 120, 240, 600],
            "initial_offsets_g": [0, 0.005, -0.005],
            "failures": [item for item in synthetic if item["offset_change_g"] != 0],
        },
        "gates": gates,
        "limitations": [
            "The minimum real load increment remains unknown.",
            "Pure automatic classification is not accepted: static local-rate noise overlaps 0.001 g per 10 s dosing.",
            "PLC or menu must disable compensation before dosing and re-enable it after dosing.",
            "This Python candidate has not completed fixed-point C parity or hardware shadow validation.",
            "The three static controls were used for selection; additional independent captures are required.",
        ],
        "product_integration_authorized": False,
        "hardware_shadow_authorized": False,
    }

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    output = Path(args.output)
    output.mkdir(parents=True, exist_ok=True)
    report = build_report()
    (output / "offline_report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"status": report["status"], "gates": report["gates"]}, indent=2))
    return 0 if all(report["gates"].values()) else 1

if __name__ == "__main__":
    raise SystemExit(main())
