#!/usr/bin/env python3
"""Stage 5M-R3 offline search for an explicit-mode static drift compensator.

Offline only. Product firmware, persistent configuration, and hardware are not
modified. Decisions are made in calibration-normalized micrograms; raw ADC
counts remain an acquisition/diagnostic input and are never compared with
sensor-specific fixed count thresholds.
"""
from __future__ import annotations

import argparse
import json
import statistics
import sys
from functools import lru_cache
from dataclasses import asdict, dataclass, replace
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools" / "stage5mr2_replay"))
import stage5mr2_threshold_replay as r2

STATIC_PATHS = r2.STATIC_PATHS
REAL_LOAD_PATHS = r2.REAL_LOAD_PATHS
SOURCE_BLOBS = r2.SOURCE_BLOBS


@lru_cache(maxsize=None)
def load_seconds(relative_path):
    # Immutable Git-blob evidence is parsed once per process, then reused by
    # every parameter candidate. Return a tuple so callers cannot mutate it.
    return tuple(r2.load_seconds(relative_path))


@dataclass(frozen=True)
class Config:
    observation_window_s: int = 180
    endpoint_median_s: int = 15
    estimator_deadband_ug: int = 500
    max_static_rate_g_per_h: float = 0.5
    step_block_s: int = 3
    step_threshold_ug: int = 20000
    step_confirmations: int = 2
    hold_off_s: int = 15
    maximum_update_ug_per_s: int = 75


def normalize_adc_delta_to_ug(raw_delta, span_counts, span_mass_ug):
    """Convert an ADC delta with the active two-point calibration.

    The rounded physical value, not the sensor-specific count value, is the
    algorithm input. A calibration change must reset the runtime state.
    """
    if span_counts == 0 or span_mass_ug <= 0:
        raise ValueError("valid calibration span required")
    numerator = raw_delta * span_mass_ug
    magnitude = (abs(numerator) + abs(span_counts) // 2) // abs(span_counts)
    return magnitude if numerator * span_counts >= 0 else -magnitude


def improvement_fraction(before, after):
    denominator = abs(before)
    if denominator < 1e-12:
        return 0.0 if abs(after) < 1e-12 else -1.0
    return 1.0 - abs(after) / denominator


class StaticModeCompensator:
    """Anchored slow correction with an explicit enable safety boundary."""

    def __init__(self, config, initial_offset_ug=0):
        self.c = config
        self.offset = float(initial_offset_ug)
        self.target = float(initial_offset_ug)
        self.anchor = None
        self.previous_enabled = None
        self.hold_until = -1
        self.step_values = []
        self.slow_values = []
        self.step_count = 0
        self.step_sign = 0
        self.updates = 0
        self.step_events = 0
        self.decisions = []

    def _change_mode(self, enabled, second):
        if self.previous_enabled is None:
            self.previous_enabled = enabled
            return
        if enabled == self.previous_enabled:
            return
        self.target = self.offset
        self.anchor = None
        self.step_values.clear()
        self.slow_values.clear()
        self.step_count = 0
        self.step_sign = 0
        if enabled:
            self.hold_until = second + self.c.hold_off_s
        self.previous_enabled = enabled

    def _ramp(self):
        limit = self.c.maximum_update_ug_per_s
        if self.target > self.offset:
            self.offset = min(self.target, self.offset + limit)
        elif self.target < self.offset:
            self.offset = max(self.target, self.offset - limit)

    def _detect_step(self, mass_ug):
        self.step_values.append(mass_ug)
        block = self.c.step_block_s
        self.step_values = self.step_values[-2 * block :]
        if len(self.step_values) != 2 * block:
            return False
        delta = r2.median(self.step_values[block:]) - r2.median(self.step_values[:block])
        sign = 1 if delta > 0 else -1 if delta < 0 else 0
        if abs(delta) < self.c.step_threshold_ug:
            self.step_count = 0
            self.step_sign = 0
            return False
        if sign == self.step_sign:
            self.step_count += 1
        else:
            self.step_count = 1
            self.step_sign = sign
        return self.step_count >= self.c.step_confirmations

    def process(self, second, normalized_mass_ug, enabled):
        self._change_mode(enabled, second)
        if not enabled:
            return normalized_mass_ug - self.offset

        self._ramp()

        if self._detect_step(normalized_mass_ug):
            self.target = self.offset
            self.anchor = None
            self.step_values.clear()
            self.slow_values.clear()
            self.step_count = 0
            self.step_sign = 0
            self.hold_until = second + self.c.hold_off_s
            self.step_events += 1
            self.decisions.append({
                "second": second,
                "accepted": False,
                "reason": "FAST_STEP_REBASE",
                "offset_ug": self.offset,
            })
            return normalized_mass_ug - self.offset

        if second < self.hold_until:
            return normalized_mass_ug - self.offset

        self.slow_values.append(normalized_mass_ug)
        if len(self.slow_values) < self.c.observation_window_s:
            return normalized_mass_ug - self.offset

        edge = self.c.endpoint_median_s
        first = r2.median(self.slow_values[:edge])
        last = r2.median(self.slow_values[-edge:])
        if self.anchor is None:
            self.anchor = first - self.offset

        delta = last - first
        rate = abs(delta) * 3600.0 / (
            self.c.observation_window_s * 1_000_000
        )
        desired = last - self.anchor
        accepted = False
        if abs(delta) <= self.c.estimator_deadband_ug:
            reason = "BELOW_ESTIMATOR_DEADBAND"
        elif rate > self.c.max_static_rate_g_per_h:
            # In STATIC_COMPENSATION this is treated as an undeclared
            # disturbance. Rebase safely; do not turn a fast change into drift.
            self.anchor = last - self.offset
            self.target = self.offset
            reason = "RATE_LIMIT_REBASE"
        else:
            self.target = desired
            self.updates += 1
            accepted = True
            reason = "STATIC_DRIFT_ACCEPTED"

        self.decisions.append({
            "second": second,
            "first_ug": first,
            "last_ug": last,
            "delta_ug": delta,
            "estimated_rate_g_per_h": rate,
            "desired_offset_ug": desired,
            "offset_before_ramp_ug": self.offset,
            "target_offset_ug": self.target,
            "accepted": accepted,
            "reason": reason,
        })
        self.slow_values.clear()
        return normalized_mass_ug - self.offset


def replay(samples, config, initial_offset_ug=0):
    model = StaticModeCompensator(config, initial_offset_ug)
    output = []
    for second, mass_ug, enabled in samples:
        corrected = model.process(second, mass_ug, enabled)
        output.append((second, mass_ug, corrected, model.offset, enabled))
    maximum_10s = max(
        (
            abs(output[index][3] - output[index - 10][3])
            for index in range(10, len(output))
        ),
        default=0,
    )
    return model, output, maximum_10s


def static_case(relative_path, config, include_trace=False):
    source = load_seconds(relative_path)
    robust_final = r2.median([mass for _, mass in source[-60:]])
    samples = [(second, mass, True) for second, mass in source]
    last = samples[-1][0]
    samples.extend((last + i, robust_final, True) for i in range(1, 1801))
    model, output, maximum_10s = replay(samples, config)

    original = output[: len(source)]
    raw = [(second, mass) for second, mass in source]
    corrected = [(row[0], row[2]) for row in original]
    input_slope = r2.ols_g_per_h(raw, 1)
    corrected_slope = r2.ols_g_per_h(corrected, 1)
    learn_after = source[0][0] + 2 * config.observation_window_s
    raw_post = [row for row in raw if row[0] >= learn_after]
    corrected_post = [row for row in corrected if row[0] >= learn_after]
    input_post = r2.ols_g_per_h(raw_post, 1)
    corrected_post_slope = r2.ols_g_per_h(corrected_post, 1)

    raw_blocks = r2.block_metrics(raw, 1)
    corrected_blocks = r2.block_metrics(corrected, 1)
    constant_tail = [(row[0], row[2]) for row in output[-600:]]
    tail_offset = [row[3] for row in output[-600:]]
    result = {
        "input_slope_g_per_h": input_slope,
        "corrected_slope_g_per_h": corrected_slope,
        "full_run_improvement_fraction": improvement_fraction(
            input_slope, corrected_slope
        ),
        "post_learning_input_slope_g_per_h": input_post,
        "post_learning_corrected_slope_g_per_h": corrected_post_slope,
        "post_learning_improvement_fraction": improvement_fraction(
            input_post, corrected_post_slope
        ),
        "input_endpoint_g": raw_blocks["endpoint_g"],
        "corrected_endpoint_g": corrected_blocks["endpoint_g"],
        "input_range_g": raw_blocks["range_g"],
        "corrected_range_g": corrected_blocks["range_g"],
        "constant_tail_slope_g_per_h": r2.ols_g_per_h(constant_tail, 1),
        "constant_tail_offset_range_g": (
            max(tail_offset) - min(tail_offset)
        ) / 1_000_000,
        "final_offset_g": model.offset / 1_000_000,
        "update_count": model.updates,
        "step_event_count": model.step_events,
        "maximum_10s_update_g": maximum_10s / 1_000_000,
        "decision_reason_counts": {
            reason: sum(item["reason"] == reason for item in model.decisions)
            for reason in sorted({item["reason"] for item in model.decisions})
        },
    }
    if include_trace:
        result["window_trace"] = model.decisions
    return result


def candidate_grid():
    return [
        Config(
            observation_window_s=window,
            estimator_deadband_ug=deadband,
            max_static_rate_g_per_h=max_rate,
            maximum_update_ug_per_s=update,
        )
        for window in (120, 180, 300)
        for deadband in (0, 250, 500, 1000, 2000)
        for max_rate in (0.25, 0.5, 1.0)
        for update in (50, 75, 100)
    ]


def evaluate_candidate(config):
    static = {
        name: static_case(path, config)
        for name, path in STATIC_PATHS.items()
    }
    improvements = [
        value["full_run_improvement_fraction"] for value in static.values()
    ]
    non_amplification = all(
        abs(value["corrected_slope_g_per_h"])
        <= abs(value["input_slope_g_per_h"]) + 0.002
        for value in static.values()
    )
    tail_safe = all(
        abs(value["constant_tail_slope_g_per_h"]) <= 0.002
        and value["constant_tail_offset_range_g"] <= 0.001
        for value in static.values()
    )
    budget_safe = all(
        value["maximum_10s_update_g"] <= 0.001
        for value in static.values()
    )
    return {
        "config": asdict(config),
        "static": static,
        "minimum_full_run_improvement_fraction": min(improvements),
        "median_full_run_improvement_fraction": statistics.median(improvements),
        "non_amplification": non_amplification,
        "constant_tail_safe": tail_safe,
        "ten_second_budget_safe": budget_safe,
    }


def candidate_rank(item):
    eligible = (
        item["non_amplification"]
        and item["constant_tail_safe"]
        and item["ten_second_budget_safe"]
    )
    return (
        1 if eligible else 0,
        item["minimum_full_run_improvement_fraction"],
        item["median_full_run_improvement_fraction"],
        -item["config"]["maximum_update_ug_per_s"],
        -item["config"]["observation_window_s"],
    )


def real_mode_case(relative_path, config, initial_offset_ug):
    source = load_seconds(relative_path)
    final_mass = r2.median([mass for _, mass in source[-60:]])
    samples = [(second, mass, False) for second, mass in source]
    last = samples[-1][0]
    samples.extend((last + i, final_mass, True) for i in range(1, 1801))
    model, output, maximum_10s = replay(samples, config, initial_offset_ug)
    disabled_offsets = [row[3] for row in output[: len(source)]]
    enabled_offsets = [row[3] for row in output[len(source):]]
    return {
        "initial_offset_g": initial_offset_ug / 1_000_000,
        "final_offset_g": model.offset / 1_000_000,
        "disabled_offset_range_g": (
            max(disabled_offsets) - min(disabled_offsets)
        ) / 1_000_000,
        "post_enable_offset_range_g": (
            max(enabled_offsets) - min(enabled_offsets)
        ) / 1_000_000,
        "maximum_10s_update_g": maximum_10s / 1_000_000,
    }


def synthetic_mode_case(increment_g, pause_s, feed_s, config, initial_offset_ug):
    mass = 0
    samples = []
    for second in range(feed_s + 1801):
        enabled = second > feed_s
        if not enabled and second and second % pause_s == 0:
            mass += round(increment_g * 1_000_000)
        samples.append((second, mass, enabled))
    model, output, maximum_10s = replay(samples, config, initial_offset_ug)
    disabled_offsets = [row[3] for row in output[: feed_s + 1]]
    enabled_offsets = [row[3] for row in output[feed_s + 1 :]]
    return {
        "increment_g": increment_g,
        "pause_s": pause_s,
        "feed_s": feed_s,
        "total_load_g": mass / 1_000_000,
        "initial_offset_g": initial_offset_ug / 1_000_000,
        "final_offset_g": model.offset / 1_000_000,
        "disabled_offset_range_g": (
            max(disabled_offsets) - min(disabled_offsets)
        ) / 1_000_000,
        "post_enable_offset_range_g": (
            max(enabled_offsets) - min(enabled_offsets)
        ) / 1_000_000,
        "maximum_10s_update_g": maximum_10s / 1_000_000,
    }


def sensor_scaling_invariance(config):
    physical = []
    mass = 2_000_000
    for second in range(1501):
        enabled = not (601 <= second <= 720)
        if second and second <= 600 and second % 30 == 0:
            mass += 1000
        if 601 <= second <= 720 and second % 10 == 0:
            mass += 1000
        physical.append((second, mass, enabled))

    base_model, base_output, _ = replay(physical, config)
    cases = []
    for counts_per_g in (1000, 2000, 4000):
        zero = -50000
        normalized = []
        for second, mass_ug, enabled in physical:
            raw = zero + mass_ug * counts_per_g // 1_000_000
            converted = normalize_adc_delta_to_ug(
                raw - zero, counts_per_g, 1_000_000
            )
            normalized.append((second, converted, enabled))
        model, output, _ = replay(normalized, config)
        cases.append({
            "counts_per_g": counts_per_g,
            "decision_reasons_equal": [
                item["reason"] for item in model.decisions
            ] == [item["reason"] for item in base_model.decisions],
            "offsets_equal": [row[3] for row in output]
            == [row[3] for row in base_output],
            "disabled_offset_frozen": max(
                row[3] for row in output[601:721]
            ) == min(row[3] for row in output[601:721]),
        })
    return {
        "domain": "calibration_normalized_micrograms",
        "raw_counts_used_for": "acquisition_and_diagnostics_only",
        "calibration_reset_required": True,
        "cases": cases,
        "passed": all(
            item["decision_reasons_equal"]
            and item["offsets_equal"]
            and item["disabled_offset_frozen"]
            for item in cases
        ),
    }


def build_report():
    evaluated = [evaluate_candidate(config) for config in candidate_grid()]
    evaluated.sort(key=candidate_rank, reverse=True)
    selected_summary = evaluated[0]
    selected = Config(**selected_summary["config"])
    selected_static = {
        name: static_case(path, selected, include_trace=True)
        for name, path in STATIC_PATHS.items()
    }

    real = []
    for name, path in REAL_LOAD_PATHS.items():
        for initial in (0, 5000, -5000):
            real.append({
                "case": name,
                **real_mode_case(path, selected, initial),
            })
    synthetic = [
        synthetic_mode_case(increment, pause, duration, selected, initial)
        for increment in (0.0001, 0.0005, 0.001, 0.002, 0.005, 0.01, 0.02)
        for pause in (2, 5, 10)
        for duration in (60, 120, 240, 600)
        for initial in (0, 5000, -5000)
    ]
    scaling = sensor_scaling_invariance(selected)
    improvements = [
        item["full_run_improvement_fraction"]
        for item in selected_static.values()
    ]
    mode_cases = real + synthetic
    gates = {
        "static_non_amplification_all_runs": all(
            abs(item["corrected_slope_g_per_h"])
            <= abs(item["input_slope_g_per_h"]) + 0.002
            for item in selected_static.values()
        ),
        "static_every_run_improves_at_least_50_percent": min(improvements) >= 0.50,
        "static_median_improvement_at_least_70_percent": (
            statistics.median(improvements) >= 0.70
        ),
        "constant_tail_no_chase": all(
            abs(item["constant_tail_slope_g_per_h"]) <= 0.002
            and item["constant_tail_offset_range_g"] <= 0.001
            for item in selected_static.values()
        ),
        "real_dosing_mode_zero_offset_change": all(
            item["disabled_offset_range_g"] == 0
            and item["post_enable_offset_range_g"] == 0
            for item in real
        ),
        "synthetic_dosing_mode_zero_offset_change": all(
            item["disabled_offset_range_g"] == 0
            and item["post_enable_offset_range_g"] == 0
            for item in synthetic
        ),
        "ten_second_update_budget": all(
            item["maximum_10s_update_g"] <= 0.001
            for item in mode_cases + list(selected_static.values())
        ),
        "sensor_scaling_invariance": scaling["passed"],
    }
    passed = all(gates.values())
    return {
        "status": (
            "STAGE_5M_R3_STATIC_MODE_OFFLINE_CANDIDATE_PASSED"
            if passed
            else "STAGE_5M_R3_NO_ACCEPTABLE_CANDIDATE"
        ),
        "source_commit": "db612a8bd8f06f80faf3feffe40466c746dcca26",
        "source_blobs": SOURCE_BLOBS,
        "operating_contract": {
            "boot_default": "DOSING_NO_COMPENSATION",
            "STATIC_COMPENSATION": (
                "declares that a constant load is expected; slow drift may be corrected"
            ),
            "DOSING_NO_COMPENSATION": (
                "mandatory before and during liquid, powder, or repeated dosing"
            ),
            "reenable": "15 second hold-off and reference rebuild",
        },
        "algorithm_input_contract": {
            "sample_source": "raw ADC acquisition before drift correction",
            "decision_domain": "calibration-normalized micrograms",
            "thresholds_must_not_be_raw_adc_counts": True,
            "reset_on": [
                "calibration change",
                "sample-rate change",
                "gain change",
                "sensor replacement",
                "invalid calibration",
            ],
        },
        "candidate_count": len(evaluated),
        "selected_config": asdict(selected),
        "selected_static": selected_static,
        "top_candidates": [
            {
                key: value
                for key, value in item.items()
                if key != "static"
            }
            for item in evaluated[:20]
        ],
        "mode_gated_real_cases": real,
        "mode_gated_synthetic_summary": {
            "case_count": len(synthetic),
            "minimum_increment_g": 0.0001,
            "pauses_s": [2, 5, 10],
            "feed_durations_s": [60, 120, 240, 600],
            "initial_offsets_g": [0, 0.005, -0.005],
            "failures": [
                item for item in synthetic
                if item["disabled_offset_range_g"] != 0
                or item["post_enable_offset_range_g"] != 0
            ],
        },
        "sensor_scaling_invariance": scaling,
        "gates": gates,
        "limitations": [
            "All real captures currently come from one physical sensor.",
            "Scaling invariance is mathematical/synthetic until another calibrated sensor is captured.",
            "Slow real loading can be corrected if the operator leaves STATIC_COMPENSATION enabled.",
            "Fast response and fast settling remain deferred to a later filter stage.",
            "Fixed-point C parity and hardware shadow validation are not yet complete.",
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
    (output / "offline_report.json").write_text(
        json.dumps(report, indent=2) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    print(json.dumps({
        "status": report["status"],
        "selected_config": report["selected_config"],
        "static": {
            name: {
                "input": item["input_slope_g_per_h"],
                "corrected": item["corrected_slope_g_per_h"],
                "improvement": item["full_run_improvement_fraction"],
                "decisions": item["decision_reason_counts"],
            }
            for name, item in report["selected_static"].items()
        },
        "gates": report["gates"],
    }, indent=2))
    return 0 if all(report["gates"].values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())
