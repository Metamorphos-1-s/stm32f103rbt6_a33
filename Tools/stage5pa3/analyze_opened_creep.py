#!/usr/bin/env python3
"""Describe opened creep evidence without selecting or qualifying an algorithm.

All repository objects are pinned to the frozen 0x051C commit. A sparse
checkout can read the blobs directly from git; a full checkout reads files.
"""

import argparse
import csv
import io
import json
import os
import statistics
import subprocess
import sys
from pathlib import Path

SOURCE_COMMIT = "d6312dd9898ba8f01a43cd83a76fa41546d88275"
ROOT = Path(__file__).resolve().parents[2]
RUNS = {
    "loaded_500g_30m": "Results/stage5l_characterization/20260913T064617Z_creep_500g_30m/samples.csv",
    "unloaded_zero_15m": "Results/stage5l_characterization/20260913T072000Z_zero_return_15m/samples.csv",
    "loaded_500g_1h_a": "Results/stage5mr4/20260916T_r4_500g_constant_1/samples.csv",
    "loaded_500g_1h_b": "Results/stage5mr4/20260916T_r4_500g_constant_2/samples.csv",
    "cold_empty_1h_r4": "Results/stage5mr4/20260915T_r4_cold_empty_1/samples.csv",
    "cold_empty_1h_5l": "Results/stage5l_characterization/20260913T_cold_start_empty_60m/samples.csv",
}
OLD_EARLY_REPORT = (
    "Results/stage5pa/20260923T142159Z_controlled_product/"
    "early_tracking_offline.json"
)
CHECKPOINTS_S = (60, 120, 300, 600, 900, 1500, 2700)
FIT_TIME_CONSTANTS_S = (30, 60, 120, 240, 480, 900, 1800, 3600)


def read_pinned(relative, data_root=None):
    """Verify every local candidate against the pinned Git blob."""
    expected = subprocess.check_output(
        ["git", "-C", str(ROOT), "rev-parse",
         f"{SOURCE_COMMIT}:{relative}"], text=True).strip()
    for candidate in (ROOT / relative,
                      data_root / relative if data_root is not None else None):
        if candidate is None or not candidate.is_file():
            continue
        actual = subprocess.check_output(
            ["git", "-C", str(ROOT), "hash-object", str(candidate)],
            text=True).strip()
        if actual != expected:
            raise ValueError(f"data file differs from pinned blob: {relative}")
        return candidate.read_text(encoding="utf-8")
    env = dict(os.environ, GIT_NO_LAZY_FETCH="1")
    return subprocess.check_output(
        ["git", "-C", str(ROOT), "show", f"{SOURCE_COMMIT}:{relative}"],
        text=True, env=env)


def second_series(source):
    reader = csv.DictReader(io.StringIO(source))
    seconds = {}
    first_ms = None
    row_count = 0
    for row in reader:
        time_ms = int(row.get("mcu_uptime_ms") or row["uptime_ms"])
        if first_ms is None:
            first_ms = time_ms
        second = (time_ms - first_ms) // 1000
        if second < 0:
            raise ValueError("MCU uptime moved backwards")
        current = seconds.setdefault(second, {})
        for field in ("uncompensated_gross_ug", "raw_adc", "filtered_raw",
                      "raw_calibrated_mass_ug", "filtered_mass_ug",
                      "conditioned_display_mass_ug"):
            if row.get(field):
                current.setdefault(field, []).append(int(row[field]))
        row_count += 1
    if row_count == 0:
        raise ValueError("empty raw record")
    channels = {field: [(second, int(statistics.median(values[field])))
                for second, values in sorted(seconds.items())
                if field in values]
                for field in ("uncompensated_gross_ug", "raw_adc",
                              "filtered_raw",
                              "raw_calibrated_mass_ug", "filtered_mass_ug",
                              "conditioned_display_mass_ug")}
    return row_count, channels["uncompensated_gross_ug"], channels


def median_between(series, first, last):
    values = [value for second, value in series if first <= second < last]
    return int(statistics.median(values)) if values else None


def replay_r5(series):
    sys.path.insert(0, str(ROOT / "Tools" / "stage5mr5b_beta"))
    from reference_lock_model import Mode, ReferenceLock
    model = ReferenceLock()
    model.set_mode(Mode.STATIC_COMPENSATION)
    values = []
    for second, mass in series:
        snap = model.process_second(second, mass)
        values.append((second, snap["corrected_gross_ug"], snap["offset_ug"]))
    return values, model.automatic_rebase_count


def fit_one_exponential(series, anchor):
    """Exploratory early fit; subsequent samples are not fitted."""
    fit_end, check_start, check_end = (
        (300, 600, 840) if series[-1][0] < 1500 else (600, 900, 1500))
    training = [(t, mass - anchor) for t, mass in series
                if 60 <= t < fit_end and t % 10 == 0]
    testing = [(t, mass - anchor) for t, mass in series
               if check_start <= t < check_end and t % 10 == 0]
    if len(training) < 15 or len(testing) < 15:
        return {"status": "INSUFFICIENT_DATA"}
    best = None
    for tau in FIT_TIME_CONSTANTS_S:
        # exp is only used by this offline diagnostic, not by the firmware.
        from math import exp
        xs = [1.0 - exp(-t / tau) for t, _ in training]
        amplitude = sum(x * y for x, (_, y) in zip(xs, training)) / sum(x * x for x in xs)
        loss = sum((y - amplitude * x) ** 2 for x, (_, y) in zip(xs, training))
        if best is None or loss < best[0]:
            best = (loss, tau, amplitude)
    _, tau, amplitude = best
    from math import exp
    error = [abs(y - amplitude * (1.0 - exp(-t / tau))) for t, y in testing]
    return {"status": "OPENED_TEMPORAL_CHECK", "fit_seconds": [60, fit_end],
            "check_seconds": [check_start, check_end], "tau_s": tau,
            "fitted_amplitude_ug": round(amplitude),
            "out_of_fit_median_abs_error_ug": round(statistics.median(error)),
            "uncompensated_median_abs_change_ug":
                round(statistics.median(abs(y) for _, y in testing))}


def analyze(name, source):
    row_count, series, channels = second_series(source)
    anchor = median_between(series, 15, 45)
    if anchor is None:
        raise ValueError(f"{name}: no 15-45 second reference")
    corrected, rebases = replay_r5(series)
    observations = {}
    for start in CHECKPOINTS_S:
        raw = median_between(series, start, start + 60)
        corrected_value = median_between(
            [(s, value) for s, value, _ in corrected], start, start + 60)
        offset = median_between(
            [(s, value) for s, _, value in corrected], start, start + 60)
        if raw is not None:
            observations[str(start)] = {
                "raw_relative_to_initial_ug": raw - anchor,
                "r5_relative_to_initial_ug": corrected_value - anchor,
                "r5_offset_ug": offset}
    channel_change = {}
    for field in ("raw_adc", "filtered_raw", "raw_calibrated_mass_ug",
                  "filtered_mass_ug", "conditioned_display_mass_ug"):
        values = channels[field]
        if values:
            field_reference = median_between(values, 15, 45)
            field_after = median_between(values, 300, 360)
            channel_change[field] = (field_after - field_reference
                                     if field_after is not None else None)
    return {"classification": "OPENED DEVELOPMENT / NOT INDEPENDENT",
            "source": RUNS[name], "sample_count": row_count,
            "duration_s": series[-1][0], "reference_15_45s_ug": anchor,
            "r5_rebase_count": rebases, "checkpoints": observations,
            "other_channel_change_300_360_vs_15_45_native_units":
                channel_change,
            "single_exponential_diagnostic": fit_one_exponential(series, anchor)}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--data-root", type=Path,
                        help="checkout with original Results blobs; SHA checked")
    args = parser.parse_args()
    results = {name: analyze(name, read_pinned(relative, args.data_root))
               for name, relative in RUNS.items()}
    prior = json.loads(read_pinned(OLD_EARLY_REPORT, args.data_root))
    report = {"schema_version": 1, "source_commit": SOURCE_COMMIT,
              "classification": "OPENED DEVELOPMENT / NO QUALIFICATION",
              "definition": "Relative to 15-45 s median; this is not a known true mass",
              "channel_units": {"raw_adc": "ADC counts",
                  "filtered_raw": "ADC counts",
                  "filtered_mass_ug": "ug",
                  "raw_calibrated_mass_ug": "ug",
                  "conditioned_display_mass_ug": "ug"},
              "runs": results,
              "frozen_early_candidate": {
                  "decision": prior["decision"],
                  "failed_improvement_runs": [name for name, item in
                      prior["datasets"].items()
                      if item["candidate"].get("eligible") and
                      name != "dosing_slow_fill" and
                      item["candidate"]["improved_5_10_15_count"] < 2]}}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8")
    print(json.dumps({"runs": list(results),
        "prior_early_decision": report["frozen_early_candidate"]["decision"]}))


if __name__ == "__main__":
    main()
