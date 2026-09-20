#!/usr/bin/env python3
"""Finite Stage 5N-A candidate search over real 10 Hz and boundary data."""

import argparse
import csv
import hashlib
import itertools
import json
import statistics
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from checkweigh_model import (CheckweighShadow, ShadowConfig, INVALID,
    PENDING, LOW, OK, HIGH, calibrate_raw, classify)


RUNS = {
    "load_500g": "Results/stage5l_characterization/20260913_filter_compare/filt0_load_step/samples.csv",
    "unload_500g": "Results/stage5l_characterization/20260913_filter_compare/filt0_unload_step/samples.csv",
    "cycles_5": "Results/stage5mr4/20260916T_r4_load_unload_cycles_5/samples.csv",
    "empty_static": "Results/stage5l_characterization/20260912T192400Z_empty_10m/samples.csv",
    "loaded_static": "Results/stage5mr4/20260916T_r4_500g_constant_2/samples.csv",
    "slow_fill": "Results/stage5l_characterization/20260913_slow_fill/slow_continuous/samples.csv",
    "faster_fill": "Results/stage5l_characterization/20260913_slow_fill/faster_continuous/samples.csv",
}


def read_rows(relative):
    with (ROOT / relative).open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def sha(relative):
    return hashlib.sha256((ROOT / relative).read_bytes()).hexdigest().upper()


def value(row, *names, default=0):
    for name in names:
        if name in row and row[name] != "":
            return int(float(row[name]))
    return default


def real_samples(rows):
    samples = []
    for index, row in enumerate(rows):
        raw = value(row, "raw_adc", "raw_value")
        samples.append({"sequence": value(row, "sample_sequence", default=index),
            "timestamp": value(row, "mcu_uptime_ms", "uptime_ms", default=index * 100),
            "static": value(row, "net_ug", "net_mass_ug"),
            "dynamic": calibrate_raw(raw),
            "stable": bool(value(row, "stable", default=1)),
            "valid": value(row, "fault_mask") == 0,
            "overload": bool(value(row, "overload"))})
    return samples


def replay(samples, config, low, high, process_active=False):
    model = CheckweighShadow(config); output = []
    for sample in samples:
        output.append({**sample, **model.process(sequence=sample["sequence"],
            timestamp_ms=sample["timestamp"], static_weight_ug=sample["static"],
            dynamic_weight_ug=sample["dynamic"], low_limit_ug=low,
            high_limit_ug=high, stable=sample["stable"],
            process_active=process_active, valid=sample["valid"],
            overload=sample["overload"])})
    return output


def transition_metrics(output, low, high):
    raw_reference = [classify(row["dynamic"], low, high) for row in output]
    reference = [raw_reference[0]] * len(raw_reference)
    for index in range(1, len(raw_reference)):
        target = raw_reference[index]
        if target != reference[index - 1] and index + 1 < len(raw_reference) and \
                raw_reference[index + 1] == target:
            reference[index] = target
        else:
            reference[index] = reference[index - 1]
    changes = []
    for index in range(1, len(reference)):
        if reference[index] != reference[index - 1]:
            target = reference[index]
            found = next((later for later in range(index, len(output))
                if output[later]["dynamic_confirmed"] == target), None)
            next_reference_change = next((later for later in range(index + 1,
                len(reference)) if reference[later] != target), len(reference))
            missed = found is None or found >= next_reference_change
            changes.append({"index": index, "from": reference[index - 1],
                "to": target, "missed": missed,
                "delay_ms": None if missed else
                    output[found]["timestamp"] - output[index]["timestamp"]})
    valid_changes = [change for change in changes if not change["missed"]]
    wrong = 0; events = 0; last = PENDING
    for row in output:
        current = row["dynamic_confirmed"]
        if row["event"]:
            events += 1
            if current not in (LOW, OK, HIGH):
                wrong += 1
        last = current
    delays = [change["delay_ms"] for change in valid_changes]
    return {"reference_crossings": len(changes),
        "missed_crossings": sum(change["missed"] for change in changes),
        "direction_errors": wrong, "candidate_events": events,
        "delays_ms": delays, "median_delay_ms": statistics.median(delays) if delays else 0,
        "maximum_delay_ms": max(delays, default=0), "changes": changes}


def synthetic_samples(values, stable=True, process_active=False):
    return [{"sequence": index + 1, "timestamp": (index + 1) * 100,
        "static": mass, "dynamic": mass, "stable": stable,
        "valid": True, "overload": False, "process_active": process_active}
        for index, mass in enumerate(values)]


def synthetic_gates(config):
    low, high = 100000000, 400000000
    values = [0] * 20 + [150000000] * 20 + [500000000] * 20 + \
        [150000000] * 20 + [0] * 20
    transitions = transition_metrics(replay(synthetic_samples(values), config,
        low, high), low, high)
    far = replay(synthetic_samples([200000000, 200005000, 199995000] * 200),
        config, low, high)
    far_switches = sum(far[index]["dynamic_confirmed"] !=
        far[index - 1]["dynamic_confirmed"] for index in range(1, len(far)))
    boundary_values = [99995000, 100005000, 99990000, 100010000] * 150
    boundary = replay(synthetic_samples(boundary_values), config, low, high)
    raw_crossings = sum(classify(boundary[index]["dynamic"], low, high) !=
        classify(boundary[index - 1]["dynamic"], low, high)
        for index in range(1, len(boundary)))
    confirmed_changes = sum(boundary[index]["dynamic_confirmed"] !=
        boundary[index - 1]["dynamic_confirmed"] for index in range(1, len(boundary)))
    dosing = replay(synthetic_samples([150000000] * 100), config,
        low, high, process_active=True)
    unstable = replay(synthetic_samples([150000000] * 20, stable=False),
        config, low, high)
    invalid_model = CheckweighShadow(config)
    invalid_cases = []
    for index, kwargs in enumerate(({"valid": False}, {"fault": True},
            {"overload": True}, {"calibration": True}), 1):
        invalid_cases.append(invalid_model.process(sequence=index,
            timestamp_ms=index * 100, static_weight_ug=150000000,
            dynamic_weight_ug=150000000, low_limit_ug=low,
            high_limit_ug=high, stable=True, process_active=False, **kwargs))
    illegal = invalid_model.process(sequence=10, timestamp_ms=1000,
        static_weight_ug=0, dynamic_weight_ug=0, low_limit_ug=2,
        high_limit_ug=1, stable=True, process_active=False)
    static_final = replay(synthetic_samples([150000000] * 10), config,
        low, high)[-1]["static_class"]
    result = {"transitions": transitions, "far_false_switches": far_switches,
        "boundary_raw_crossings": raw_crossings,
        "boundary_confirmed_changes": confirmed_changes,
        "dosing_static_valid": sum(row["static_class"] in (LOW, OK, HIGH)
            for row in dosing),
        "unstable_static_valid": sum(row["static_class"] in (LOW, OK, HIGH)
            for row in unstable),
        "invalid_valid_outputs": sum(row["static_class"] in (LOW, OK, HIGH) or
            row["dynamic_confirmed"] in (LOW, OK, HIGH) for row in invalid_cases),
        "illegal_config_valid": int(illegal["static_class"] in (LOW, OK, HIGH) or
            illegal["dynamic_confirmed"] in (LOW, OK, HIGH)),
        "static_final": static_final}
    result["passed"] = transitions["missed_crossings"] == 0 and \
        transitions["direction_errors"] == 0 and \
        transitions["maximum_delay_ms"] <= 600 and far_switches == 0 and \
        result["dosing_static_valid"] == 0 and \
        result["unstable_static_valid"] == 0 and \
        result["invalid_valid_outputs"] == 0 and \
        result["illegal_config_valid"] == 0 and static_final == OK
    return result


def evaluate(config, datasets):
    real = {}
    for name, samples in datasets.items():
        masses = [sample["dynamic"] for sample in samples]
        if name in ("slow_fill", "faster_fill"):
            low = int(min(masses) + (max(masses) - min(masses)) / 3)
            high = int(min(masses) + 2 * (max(masses) - min(masses)) / 3)
        else:
            low, high = 100000000, 400000000
        output = replay(samples, config, low, high)
        metric = transition_metrics(output, low, high)
        metric.update({"low_limit_ug": low, "high_limit_ug": high,
            "records": len(samples),
            "unstable_static_valid": sum((not row["stable"]) and
                row["static_class"] in (LOW, OK, HIGH) for row in output),
            "stable_final_errors": sum(row["stable"] and
                row["static_class"] in (LOW, OK, HIGH) and
                row["static_class"] != classify(row["static"], low, high)
                for row in output)})
        real[name] = metric
    synthetic = synthetic_gates(config)
    dynamic_runs = [real[name] for name in
        ("load_500g", "unload_500g", "cycles_5", "slow_fill", "faster_fill")]
    passed = synthetic["passed"] and all(run["missed_crossings"] == 0 and
        run["direction_errors"] == 0 and run["maximum_delay_ms"] <= 600
        for run in dynamic_runs) and all(run["unstable_static_valid"] == 0 and
        run["stable_final_errors"] == 0 for run in real.values())
    return {"config": config.__dict__, "synthetic": synthetic,
        "real_runs": real, "passed": passed}


def main():
    parser = argparse.ArgumentParser(); parser.add_argument("--output", type=Path,
        required=True); args = parser.parse_args(); args.output.mkdir(parents=True,
        exist_ok=True)
    datasets = {name: real_samples(read_rows(path)) for name, path in RUNS.items()}
    candidates = []
    for static_n, hysteresis_d, confirm_n, dwell_n in itertools.product(
            (1, 2, 3, 5), (1, 2, 5), (1, 2, 3, 4, 5), (0, 2, 3, 5)):
        candidates.append(evaluate(ShadowConfig(static_n,
            hysteresis_d * 10000, confirm_n, dwell_n), datasets))
    candidates.sort(key=lambda item: (not item["passed"],
        item["synthetic"]["far_false_switches"],
        item["synthetic"]["boundary_confirmed_changes"],
        item["synthetic"]["transitions"]["maximum_delay_ms"],
        abs(item["config"]["static_stable_samples"] - 3),
        -item["config"]["dynamic_hysteresis_ug"],
        -item["config"]["dynamic_confirm_samples"],
        item["config"]["dynamic_min_dwell_samples"]))
    selected = next((candidate for candidate in candidates if candidate["passed"]), None)
    result = {"schema_version": 1, "classification": "DEVELOPMENT_AND_OPENED_REGRESSION",
        "search_space": len(candidates), "passing_candidates": sum(c["passed"] for c in candidates),
        "selected": selected, "candidates": candidates,
        "datasets": {path: sha(path) for path in RUNS.values()},
        "calibration": {"raw_zero": -44047, "raw_span": -487965,
            "span_mass_ug": 500000000},
        "result": "PASS" if selected else "NO_ACCEPTABLE_CANDIDATE"}
    (args.output / "candidate_results.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"result": result["result"],
        "passing_candidates": result["passing_candidates"],
        "selected": None if selected is None else selected["config"],
        "synthetic": None if selected is None else selected["synthetic"]}, indent=2))
    return 0 if selected else 2


if __name__ == "__main__":
    raise SystemExit(main())
