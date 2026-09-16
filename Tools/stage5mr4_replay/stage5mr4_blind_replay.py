#!/usr/bin/env python3
"""Blind replay of the frozen Stage 5M-R3 candidate on Stage 5M-R4 data."""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import statistics
import subprocess
import sys
from dataclasses import asdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools" / "stage5mr3_replay"))
import stage5mr3_static_replay as r3

FROZEN = r3.Config(
    observation_window_s=180,
    endpoint_median_s=15,
    estimator_deadband_ug=2000,
    max_static_rate_g_per_h=1.0,
    step_block_s=3,
    step_threshold_ug=20000,
    step_confirmations=2,
    hold_off_s=15,
    maximum_update_ug_per_s=75,
    correction_gain_permille=875,
)

RUNS = (
    ("cold_empty_1", "20260915T_r4_cold_empty_1", None),
    ("cold_empty_2", "20260915T_r4_cold_empty_2", None),
    ("hot_empty", "20260915T_r4_hot_empty", None),
    ("loaded_500g_1", "20260916T_r4_500g_constant_1", None),
    ("loaded_500g_2", "20260916T_r4_500g_constant_2", None),
    (
        "loaded_1kg",
        "20260916T_r4_1kg_constant",
        "2026-09-16T06:00:11.791Z",
    ),
)
CYCLE_RUN = "20260916T_r4_load_unload_cycles_5"
MODE_SOURCE = "20260915T_r4_cold_empty_1"
MODE_SWITCH_SECONDS = (60, 420) + tuple(range(450, 961, 30))


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def evidence_source(run_id):
    directory = ROOT / "Results" / "stage5mr4" / run_id
    files = []
    for name in ("samples.csv", "run_manifest_v2.json"):
        path = directory / name
        files.append({
            "path": path.relative_to(ROOT).as_posix(),
            "length": path.stat().st_size,
            "sha256": sha256(path),
        })
    return {"run_id": run_id, "files": files}


def read_rows(run_id):
    path = ROOT / "Results" / "stage5mr4" / run_id / "samples.csv"
    with path.open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def per_second(rows, start_utc=None):
    if start_utc is not None:
        rows = [row for row in rows if row["utc"] >= start_utc]
    origin = int(rows[0]["mcu_uptime_ms"])
    buckets = {}
    for row in rows:
        second = (int(row["mcu_uptime_ms"]) - origin) // 1000
        buckets.setdefault(second, []).append(int(row["uncompensated_gross_ug"]))
    return [(second, statistics.median(values)) for second, values in sorted(buckets.items())]


def slope(rows):
    if len(rows) < 2:
        return 0.0
    xs = [row[0] for row in rows]
    ys = [row[1] for row in rows]
    xm = statistics.fmean(xs)
    ym = statistics.fmean(ys)
    denominator = sum((x - xm) ** 2 for x in xs)
    return 0.0 if denominator == 0 else (
        sum((x - xm) * (y - ym) for x, y in zip(xs, ys))
        / denominator
        * 3600
        / 1_000_000
    )


def improvement(before, after):
    return None if abs(before) < 1e-12 else 1.0 - abs(after) / abs(before)


def replay_static(name, run_id, start_utc):
    all_rows = read_rows(run_id)
    source = per_second(all_rows, start_utc)
    model, output, maximum_10s = r3.replay(
        [(second, mass, True) for second, mass in source], FROZEN
    )
    raw = [(row[0], row[1]) for row in output]
    corrected = [(row[0], row[2]) for row in output]
    offsets = [row[3] for row in output]
    raw_slope = slope(raw)
    corrected_slope = slope(corrected)
    correction_steps = [
        abs((right[2] - left[2]) - (right[1] - left[1]))
        for left, right in zip(output, output[1:])
    ]
    return {
        "name": name,
        "run_id": run_id,
        "evaluation_interval": (
            "FULL_CAPTURE" if start_utc is None else "PREDECLARED_STEADY_INTERVAL"
        ),
        "predeclared_start_utc": start_utc,
        "source_csv_records": len(all_rows),
        "replay_seconds": len(source),
        "raw_slope_g_per_h": raw_slope,
        "corrected_slope_g_per_h": corrected_slope,
        "improvement_fraction": improvement(raw_slope, corrected_slope),
        "reverse_amplification": abs(corrected_slope) > abs(raw_slope) + 0.002,
        "maximum_absolute_offset_g": max(map(abs, offsets)) / 1_000_000,
        "maximum_10s_offset_change_g": maximum_10s / 1_000_000,
        "total_offset_change_g": (offsets[-1] - offsets[0]) / 1_000_000,
        "maximum_correction_induced_sample_step_g": (
            max(correction_steps, default=0) / 1_000_000
        ),
        "update_count": model.updates,
        "step_event_count": model.step_events,
        "decision_reason_counts": {
            reason: sum(item["reason"] == reason for item in model.decisions)
            for reason in sorted({item["reason"] for item in model.decisions})
        },
        "window_trace": model.decisions,
    }


def replay_cycles():
    source = per_second(read_rows(CYCLE_RUN))
    static_model, static_output, static_10s = r3.replay(
        [(second, mass, True) for second, mass in source], FROZEN
    )
    dosing_model, dosing_output, dosing_10s = r3.replay(
        [(second, mass, False) for second, mass in source], FROZEN
    )
    static_offsets = [row[3] for row in static_output]
    dosing_offsets = [row[3] for row in dosing_output]
    return {
        "run_id": CYCLE_RUN,
        "replay_seconds": len(source),
        "static_step_events": static_model.step_events,
        "static_offset_range_g": (max(static_offsets) - min(static_offsets)) / 1e6,
        "static_maximum_10s_offset_change_g": static_10s / 1e6,
        "dosing_offset_range_g": (max(dosing_offsets) - min(dosing_offsets)) / 1e6,
        "dosing_maximum_10s_offset_change_g": dosing_10s / 1e6,
        "dosing_offset_strictly_frozen": max(dosing_offsets) == min(dosing_offsets),
    }


def mode_for_second(second):
    enabled = False
    for switch in MODE_SWITCH_SECONDS:
        if second < switch:
            break
        enabled = not enabled
    return enabled


def replay_mode_switches():
    source = per_second(read_rows(MODE_SOURCE))
    last = MODE_SWITCH_SECONDS[-1] + 30
    samples = [
        (second, mass, mode_for_second(second))
        for second, mass in source
        if second <= last
    ]
    model, output, maximum_10s = r3.replay(samples, FROZEN)
    by_second = {row[0]: row for row in output}
    transition_rows = []
    holdoff_pass = True
    instant_pass = True
    for switch in MODE_SWITCH_SECONDS:
        before = by_second[switch - 1]
        after = by_second[switch]
        enabled = after[4]
        correction_jump = (after[2] - before[2]) - (after[1] - before[1])
        instant_pass &= abs(correction_jump) <= 1e-9
        holdoff_offsets = [
            by_second[second][3]
            for second in range(switch, min(switch + 15, last + 1))
        ]
        if enabled:
            holdoff_pass &= max(holdoff_offsets) == min(holdoff_offsets)
        transition_rows.append({
            "second": switch,
            "to_mode": "STATIC_COMPENSATION" if enabled else "DOSING_NO_COMPENSATION",
            "correction_induced_jump_g": correction_jump / 1e6,
            "holdoff_offset_range_g": (
                max(holdoff_offsets) - min(holdoff_offsets)
            ) / 1e6,
        })
    dosing_frozen = True
    for left, right in zip(output, output[1:]):
        if not right[4] and right[3] != left[3]:
            dosing_frozen = False
    return {
        "source_run_id": MODE_SOURCE,
        "schedule_definition": "fixed before first blind replay",
        "switch_count": len(MODE_SWITCH_SECONDS),
        "switch_seconds": list(MODE_SWITCH_SECONDS),
        "dosing_offset_strictly_frozen": dosing_frozen,
        "static_reenable_15s_holdoff_pass": holdoff_pass,
        "switch_has_no_correction_induced_jump": instant_pass,
        "maximum_10s_offset_change_g": maximum_10s / 1e6,
        "transitions": transition_rows,
        "decision_trace": model.decisions,
    }


def build_report():
    static = [replay_static(*run) for run in RUNS]
    cycles = replay_cycles()
    modes = replay_mode_switches()
    significant = [
        run["improvement_fraction"]
        for run in static
        if abs(run["raw_slope_g_per_h"]) >= 0.015
    ]
    e_g = 0.05
    gates = {
        "static_no_reverse_amplification": all(not run["reverse_amplification"] for run in static),
        "significant_drift_median_improvement_at_least_50_percent": (
            bool(significant) and statistics.median(significant) >= 0.50
        ),
        "maximum_10s_correction_at_most_0_001g": all(
            run["maximum_10s_offset_change_g"] <= 0.001 for run in static
        ) and cycles["static_maximum_10s_offset_change_g"] <= 0.001
        and modes["maximum_10s_offset_change_g"] <= 0.001,
        "no_correction_step_above_one_display_division": all(
            run["maximum_correction_induced_sample_step_g"] <= e_g for run in static
        ),
        "dosing_mode_offset_strictly_frozen": (
            cycles["dosing_offset_strictly_frozen"]
            and modes["dosing_offset_strictly_frozen"]
        ),
        "static_reenable_15s_holdoff": modes["static_reenable_15s_holdoff_pass"],
        "mode_switch_has_no_correction_induced_jump": modes["switch_has_no_correction_induced_jump"],
    }
    tool = Path(__file__).resolve()
    source_ids = [run[1] for run in RUNS] + [CYCLE_RUN]
    return {
        "schema_version": 1,
        "status": (
            "STAGE_5M_R4_FROZEN_CANDIDATE_PASSED_INDEPENDENT_VALIDATION"
            if all(gates.values())
            else "STAGE_5M_R4_FROZEN_CANDIDATE_FAILED_INDEPENDENT_VALIDATION"
        ),
        "blind_replay": True,
        "parameters_tuned_on_r4_data": False,
        "repository_head": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "tool": {
            "path": tool.relative_to(ROOT).as_posix(),
            "length": tool.stat().st_size,
            "sha256": sha256(tool),
        },
        "product_release_sha256": "82E726F5B32A0DE36A5E686F62A937EC4FD9CBB488DB9733E83D2062673EF486",
        "evidence_sources": [evidence_source(run_id) for run_id in source_ids],
        "frozen_config": asdict(FROZEN),
        "display_division_e_g": e_g,
        "static_runs": static,
        "significant_drift_run_count": len(significant),
        "significant_drift_median_improvement_fraction": (
            statistics.median(significant) if significant else None
        ),
        "stretch_70_percent_met": bool(significant) and statistics.median(significant) >= 0.70,
        "load_unload_cycles": cycles,
        "mode_switches": modes,
        "gates": gates,
        "candidate_passed": all(gates.values()),
        "fixed_point_and_shadow_authorized": all(gates.values()),
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    output = Path(args.output)
    output.mkdir(parents=True, exist_ok=False)
    report = build_report()
    (output / "blind_report.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n"
    )
    print(json.dumps({
        "status": report["status"],
        "median_improvement": report["significant_drift_median_improvement_fraction"],
        "gates": report["gates"],
        "runs": [{
            "run_id": run["run_id"],
            "raw": run["raw_slope_g_per_h"],
            "corrected": run["corrected_slope_g_per_h"],
            "improvement": run["improvement_fraction"],
        } for run in report["static_runs"]],
    }, indent=2))
    return 0 if report["candidate_passed"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
