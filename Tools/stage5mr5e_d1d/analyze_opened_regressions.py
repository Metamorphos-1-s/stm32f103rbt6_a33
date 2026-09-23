#!/usr/bin/env python3
"""Replay opened display datasets against baseline, D1-C and D1-D."""

import argparse
import csv
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(ROOT / "Tools" / "stage5mr5e_d1c"))
from unified_display_model import UnifiedDisplay, UnifiedInput, mass_to_count
from directional_model import DirectionalConfig, DirectionalDisplay
from evaluate_directional_candidates import D1B, RUNS


EXTRA = {
    "stage5l_static": "Results/stage5l_characterization/20260912T193600Z_empty_hot_60m/samples.csv",
    "stage5l_load": "Results/stage5l_characterization/20260913_filter_compare/filt3_load_step/samples.csv",
    "stage5l_unload": "Results/stage5l_characterization/20260913_filter_compare/filt3_unload_step/samples.csv",
    "stage5l_slow": "Results/stage5l_characterization/20260913_slow_fill/slow_continuous/samples.csv",
    "stage5na_cycles": "Results/stage5na/20260921T_hw_shadow/cycle1_load/samples.csv",
    "stage5na_slow": "Results/stage5na/20260921T_hw_shadow/slow_load_500g/samples.csv",
    "stage5na2_pauses": "Results/stage5na2/20260921T095640Z_shadow_hardware_closure/slow_fill_and_pauses/samples.csv",
    "stage5na2_disturbance": "Results/stage5na2/20260921T095640Z_shadow_hardware_closure/mechanical_disturbance/samples.csv",
    "stage5na2_tare": "Results/stage5na2/20260921T095640Z_shadow_hardware_closure/physical_tare_clear/samples.csv",
    "stage5nb_off": "Results/stage5nb/20260922T072718Z_guarded_active_alarm/hardware/beta_final_local_supervised_record/samples.csv",
}


def rows(relative):
    with (ROOT / relative).open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def value(row, *names, default=0):
    for name in names:
        if name in row and row[name] != "":
            return int(row[name])
    return default


def quantile(values, fraction):
    if not values:
        return 0
    ordered = sorted(values)
    return ordered[round((len(ordered) - 1) * fraction)]


def make_input(row, index):
    unit = value(row, "unit", default=1)
    decimals = value(row, "display_decimals", "decimal_places", default=2)
    division = value(row, "division_digit", "division", default=1)
    division_code = {1: 0, 2: 1, 5: 2}.get(division, 3)
    application = value(row, "application")
    view = value(row, "weight_view")
    source = (0x100 if view == 1 else 0) | ((unit & 3) << 6) | \
        ((decimals & 7) << 3) | (division_code << 1) | int(application == 1)
    mass = value(row, "gross_mass_ug", "corrected_gross_ug") if view == 1 \
        else value(row, "net_mass_ug", "gross_mass_ug",
            "corrected_gross_ug")
    stable = bool(value(row, "stable", default=
        int(bool(value(row, "status_flags") & 16))))
    valid = value(row, "fault_mask") == 0
    return UnifiedInput(mass, value(row, "sample_sequence", default=index),
        value(row, "uptime_ms", "mcu_uptime_ms", default=index * 100),
        source, unit, decimals, division, stable, valid)


def metrics(trace):
    errors = [abs(item["desired"] - item["displayed"]) //
        max(1, item["division"]) for item in trace]
    updates = wrong = aba = max_jump = max_slow_jump = max_step_loss = 0
    max_aba_10s = 0
    lag_durations = {"gt_1d_ms": 0, "gt_2d_ms": 0, "gt_4d_ms": 0}
    longest_gt_2d_ms = current_gt_2d_ms = 0
    aba_times = []
    for index in range(1, len(trace)):
        before, item = trace[index - 1], trace[index]
        dt = max(0, min(10000, item["time"] - before["time"]))
        error = errors[index]
        for threshold, key in ((1, "gt_1d_ms"), (2, "gt_2d_ms"),
                (4, "gt_4d_ms")):
            if error > threshold:
                lag_durations[key] += dt
        if error > 2:
            current_gt_2d_ms += dt
            longest_gt_2d_ms = max(longest_gt_2d_ms, current_gt_2d_ms)
        else:
            current_gt_2d_ms = 0
        jump = item["displayed"] - before["displayed"]
        desired_delta = item["desired"] - before["desired"]
        pre_delta = item["desired"] - before["displayed"]
        if jump:
            updates += 1
            max_jump = max(max_jump, abs(jump) // max(1, item["division"]))
            if item.get("locked", True) and before.get("locked", True) and \
                    abs(pre_delta) <= 8 * item["division"]:
                max_slow_jump = max(max_slow_jump,
                    abs(jump) // max(1, item["division"]))
            if pre_delta and ((jump > 0) != (pre_delta > 0)):
                wrong += 1
        if abs(desired_delta) > 8 * item["division"]:
            max_step_loss = max(max_step_loss,
                abs(item["desired"] - item["displayed"]) //
                max(1, item["division"]))
        if index >= 2 and item.get("locked", True) and \
                before.get("locked", True) and \
                trace[index - 2].get("locked", True) and \
                trace[index - 2]["displayed"] == item["displayed"] and \
                before["displayed"] != item["displayed"]:
            aba += 1
            aba_times.append(item["time"])
        aba_times = [time for time in aba_times if item["time"] - time <= 10000]
        max_aba_10s = max(max_aba_10s, len(aba_times))
    return {"samples": len(trace), "max_error_d": max(errors, default=0),
        "p50_error_d": quantile(errors, .50),
        "p95_error_d": quantile(errors, .95),
        "p99_error_d": quantile(errors, .99), **lag_durations,
        "longest_gt_2d_ms": longest_gt_2d_ms, "updates": updates,
        "wrong_direction_updates": wrong, "aba_count": aba,
        "aba_10s": max_aba_10s,
        "max_jump_d": max_jump, "max_slow_jump_d": max_slow_jump,
        "max_large_step_loss_d": max_step_loss,
        "final_error_d": errors[-1] if errors else 0}


def replay(relative):
    records = rows(relative)
    unified = UnifiedDisplay()
    d1c = DirectionalDisplay(DirectionalConfig(1, 1, 1, 5, False, 8))
    traces = {"original": [], "d1c": [], "unified": []}
    for index, row in enumerate(records):
        item = make_input(row, index)
        desired = mass_to_count(item.mass_ug, item.unit, item.decimals,
            item.division)
        if desired is None:
            continue
        panel = value(row, "display_count")
        d1c_result = d1c.process(desired // item.division, item.stable,
            value(row, "application") == 1, item.valid,
            panel // item.division, item.source)
        unified_result = unified.process(item)
        common = {"desired": desired, "time": item.now_ms,
            "division": item.division}
        traces["original"].append({**common, "displayed": panel,
            "locked": bool(value(row, "display_locked", default=1))})
        traces["d1c"].append({**common,
            "displayed": d1c_result["current_count"] * item.division,
            "locked": bool(d1c_result["locked"])})
        traces["unified"].append({**common,
            "displayed": unified_result["displayed"],
            "locked": bool(unified_result["locked"])})
    result = {name: metrics(trace) for name, trace in traces.items()}
    if records:
        first_time = traces["unified"][0]["time"]
        initial = traces["unified"][0]["displayed"]
        first_change = next((item["time"] for item in traces["unified"][1:]
            if item["displayed"] != initial), None)
        result["unified"]["first_display_change_ms"] = None if \
            first_change is None else first_change - first_time
    return result


def deadzone_experiment():
    desired_values = [0] * 12 + [2] * 5 + [0] * 5 + [2] * 5
    d1c = DirectionalDisplay(DirectionalConfig(1, 1, 1, 5, False, 8))
    unified = UnifiedDisplay()
    traces = {"d1c": [], "unified": []}
    for index, desired in enumerate(desired_values):
        mass = desired * 10000
        old = d1c.process(desired, True, True, True, 0, 0x40)
        new = unified.process(UnifiedInput(mass, index, index * 100,
            0x40, 1, 2, 1, True))
        common = {"desired": desired, "time": index * 100, "division": 1,
            "locked": index >= 10}
        traces["d1c"].append({**common,
            "displayed": old["current_count"]})
        traces["unified"].append({**common, "displayed": new["displayed"],
            "locked": bool(new["locked"])})
    result = {}
    for name, trace in traces.items():
        changed = [trace[0]["displayed"]]
        for item in trace[1:]:
            if item["displayed"] != changed[-1]:
                changed.append(item["displayed"])
        change_aba = sum(1 for index in range(2, len(changed))
            if changed[index] == changed[index - 2] and
            changed[index] != changed[index - 1])
        result[name] = {**metrics(trace), "changed_values": changed,
            "change_sequence_aba": change_aba}
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    inputs = {**RUNS, "d1b_opened_regression": D1B, **EXTRA}
    results = {}
    for name, relative in inputs.items():
        if (ROOT / relative).is_file():
            results[name] = {"path": relative,
                "classification": "DEVELOPMENT / OPENED REGRESSION",
                "metrics": replay(relative)}
    d1b = results["d1b_opened_regression"]["metrics"]["unified"]
    deadzone = deadzone_experiment()
    value_out = {"schema_version": 1,
        "classification": "DEVELOPMENT / OPENED REGRESSION",
        "datasets": results,
        "d1b_gate": {
            "no_146_second_wait": d1b["longest_gt_2d_ms"] < 146000,
            "wrong_direction_updates": d1b["wrong_direction_updates"],
            "final_error_d": d1b["final_error_d"],
            "final_error_gate_d": 1,
            "max_slow_jump_d": d1b["max_slow_jump_d"],
            "first_correct_update_ms": d1b["first_display_change_ms"]},
        "deadzone_comparison": {
            "classification": "DEVELOPMENT SYNTHETIC",
            "scenario": "+2d evidence, adjacent -1d target noise, +2d return",
            "d1c_no_1d_deadzone": deadzone["d1c"],
            "unified_1d_deadzone": deadzone["unified"]}}
    args.output.write_text(json.dumps(value_out, indent=2) + "\n",
        encoding="utf-8")
    print(json.dumps({"datasets": len(results),
        "d1b_gate": value_out["d1b_gate"],
        "deadzone_comparison": {
            "d1c_aba": deadzone["d1c"]["change_sequence_aba"],
            "unified_aba": deadzone["unified"]["change_sequence_aba"]}}, indent=2))


if __name__ == "__main__":
    main()
