#!/usr/bin/env python3
"""Analyze A4 hot-scale records without modifying firmware."""

import argparse
import csv
import json
import statistics
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools" / "stage5mr5b_beta"))
from reference_lock_model import Mode, ReferenceLock  # noqa: E402

FIELDS = ("raw_adc", "filtered_raw", "gross_ug",
          "display_count", "conditioned_display_ug", "display_anchor_ug")
EVENTS = (("unload1", "UNLOAD_1_OPERATOR_CONFIRMED", False),
          ("load2", "LOAD_2_OPERATOR_CONFIRMED", True),
          ("unload2", "UNLOAD_2_OPERATOR_CONFIRMED", False))


def read_csv(path):
    with path.open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def ns(row):
    return int(row["host_monotonic_ns"])


def value(row, field):
    item = row.get(field)
    return int(item) if item not in (None, "") else None


def median(rows, field):
    nums = [value(row, field) for row in rows]
    nums = [n for n in nums if n is not None]
    return statistics.median(nums) if nums else None


def window(rows, origin, begin_s, end_s):
    return [row for row in rows if begin_s <= (ns(row) - origin) / 1e9 < end_s]


def edges_from_samples(rows, events):
    results = {}
    for name, label, is_load in EVENTS:
        event = next(item for item in events if item.get("event") == label)
        human_ns = int(event["host_monotonic_ns"])
        matches = []
        for i in range(1, len(rows)):
            previous, current = rows[i - 1], rows[i]
            if not 0 <= (human_ns - ns(current)) / 1e9 <= 180:
                continue
            before = value(previous, "gross_ug")
            after = value(current, "gross_ug")
            if ((before < 250_000_000 <= after) if is_load else
                    (before > 250_000_000 >= after)):
                matches.append(i)
        if len(matches) != 1:
            raise ValueError("%s has %d possible edges" % (name, len(matches)))
        i = matches[0]
        previous, current = rows[i - 1], rows[i]
        center = (ns(previous) + ns(current)) // 2
        first = median(window(rows, center, -30, -5), "gross_ug")
        second = median(window(rows, center, 15, 45), "gross_ug")
        if first is None or second is None or (is_load and (first >= 50_000_000 or second <= 450_000_000)) or (
                not is_load and (first <= 450_000_000 or second >= 50_000_000)):
            raise ValueError("%s lacks stable full-step context" % name)
        raw_steps = [(j, abs(value(rows[j], "raw_adc") - value(rows[j-1], "raw_adc")))
                     for j in range(max(1, i - 10), min(len(rows), i + 11))]
        raw_index, raw_size = max(raw_steps, key=lambda pair: pair[1])
        results[name] = {
            "operator_confirmation_utc": event["utc"],
            "gross_midpoint_bracket_utc": [previous["utc"], current["utc"]],
            "bracket_width_s": (ns(current)-ns(previous))/1e9,
            "operator_message_lag_after_bracket_s": round((human_ns-ns(current))/1e9, 3),
            "raw_largest_jump_bracket_utc": [rows[raw_index-1]["utc"], rows[raw_index]["utc"]],
            "raw_jump_counts": raw_size,
            "raw_note": "primary block asynchronous with mass/status block; not an ADC DRDY timestamp",
            "pre_step_ug": first, "post_15_45s_ug": second,
            "physical_step_ug": second-first,
            "edge_center_monotonic_ns": center,
            "row_index": i}
    if not (results["unload1"]["row_index"] < results["load2"]["row_index"] < results["unload2"]["row_index"]):
        raise ValueError("incorrect edge order")
    return results


def control_trend(rows):
    first = median(rows[:600], "gross_ug")
    last = median(rows[-600:], "gross_ug")
    duration_minutes = (ns(rows[-1])-ns(rows[0]))/60e9
    slope = (last-first)/(duration_minutes-5)
    return {"initial_5min_median_ug": first, "final_5min_median_ug": last,
            "observed_change_ug": last-first, "linear_slope_ug_per_min": slope,
            "assumption": "earlier empty-only run; not contemporaneous control; slope extrapolation not causal"}


def phase(rows, origin, end, slope):
    within = [row for row in rows if origin <= ns(row) < end]
    early = window(within, origin, 15, 45)
    reference = {field: median(early, field) for field in FIELDS}
    checkpoints = {}
    for minute in (1, 2, 5, 10, 15, 30):
        section = window(within, origin, minute*60, (minute+1)*60)
        if len(section) < 100:
            checkpoints[str(minute)] = {"status": "NOT RUN: missing full 60-second window",
                                         "samples": len(section)}
            continue
        current = {field: median(section, field) for field in FIELDS}
        change = {field: current[field]-reference[field] if current[field] is not None
                  and reference[field] is not None else None for field in FIELDS}
        checkpoints[str(minute)] = {
            "samples": len(section), "medians": current, "delta_from_early": change,
            "gross_after_linear_empty_control_ug": round(
                change["gross_ug"]-slope*minute),
            "linear_control_change_ug": round(slope*minute)}
    return {"duration_s": (end-origin)/1e9, "samples": len(within),
            "early_15_45s_samples": len(early), "early_medians": reference,
            "checkpoints": checkpoints,
            "filtered_weight_ug": None,
            "filtered_weight_note": "not available from this device register map; filtered_raw is ADC counts"}


def quality(rows, summary, environment):
    expected = {"firmware": "0x051C", "map": "0x0104", "sample_rate": "0",
                "profile": "0", "gain": "3", "application": "0", "mode": "0",
                "offset_ug": "0", "reference_ug": "0", "dirty": "0",
                "revision": "15", "saved_revision": "15", "fault_mask": "0",
                "overrun_count": "0", "save_request_count_low": "6"}
    observed = {field: sorted(set(row.get(field) for row in rows)) for field in expected}
    violated = {field: observed[field] for field in expected
                if observed[field] != [expected[field]]}
    gaps = [(ns(after)-ns(before))/1e9 for before, after in zip(rows, rows[1:])]
    sequences = [value(row, "sample_sequence") for row in rows]
    uptime = [value(row, "mcu_uptime_ms") for row in rows]
    return {"recorder": {key: summary.get(key) for key in (
                "status", "records", "duration_s", "duplicates", "read_errors",
                "maximum_host_gap_s", "writes", "flash_operations")},
            "poll_gaps_over_2s": sum(gap > 2 for gap in gaps),
            "max_adjacent_poll_gap_s": max(gaps, default=0),
            "sample_sequence_regressions": sum(b <= a for a,b in zip(sequences,sequences[1:])),
            "mcu_uptime_regressions": sum(b < a for a,b in zip(uptime,uptime[1:])),
            "invariant_failures": violated,
            "static_profile": {key: environment.get(key) for key in (
                "sample_rate", "filter_mode", "filter_strength", "calibration")}}


def panel_transient(rows, edge, is_load):
    center = edge["edge_center_monotonic_ns"]
    previous = median(window(rows, center, -30, -5), "display_count")
    later = median(window(rows, center, 15, 45), "display_count")
    if previous is None or later is None:
        return {"status": "INSUFFICIENT_DATA"}
    midpoint = (previous + later) / 2
    target90 = previous + 0.9 * (later-previous)
    around = window(rows, center, -2, 15)
    midpoint_at = next((row for row in around if
        (value(row, "display_count") >= midpoint if is_load else
         value(row, "display_count") <= midpoint)), None)
    ninety_at = next((row for row in around if
        (value(row, "display_count") >= target90 if is_load else
         value(row, "display_count") <= target90)), None)
    edge_near = window(rows, center, 0, 10)
    jumps = [abs(value(a, "display_count")-value(b, "display_count"))
             for b, a in zip(edge_near, edge_near[1:])]
    return {"pre_count": previous, "post_15_45s_count": later,
            "panel_midpoint_utc": midpoint_at["utc"] if midpoint_at else None,
            "panel_midpoint_vs_gross_center_s": round((ns(midpoint_at)-center)/1e9, 3)
                if midpoint_at else None,
            "panel_90pct_utc": ninety_at["utc"] if ninety_at else None,
            "panel_90pct_vs_gross_center_s": round((ns(ninety_at)-center)/1e9, 3)
                if ninety_at else None,
            "max_panel_single_poll_jump_counts_10s": max(jumps, default=0),
            "note": "2 Hz host polls and 0.01 g quantization; not DRDY-level lag"}


def replay_r5(rows, edges, trace_path=None):
    """Retrospective schedule, one continuous state; not real firmware execution."""
    start = ns(rows[0])
    by_second = defaultdict(list)
    for row in rows:
        by_second[(ns(row)-start)//1_000_000_000].append(
            value(row, "gross_ug"))
    schedule = []
    for name in ("unload1", "load2", "unload2"):
        second = max(2, round((edges[name]["edge_center_monotonic_ns"]-start)/1e9))
        schedule.extend(((second-2, "DOSING", name+"_before_step"),
                         (second+60, "STATIC", name+"_after_60s")))
    schedule.sort()
    model = ReferenceLock()
    model.set_mode(Mode.STATIC_COMPENSATION)
    transitions = [{"second": 0, "mode": "STATIC", "context": "already loaded at recorder start"}]
    index = 0
    maximum_dosing_change = 0
    last_dosing = None
    trace = []
    for second, masses in sorted(by_second.items()):
        while index < len(schedule) and second >= schedule[index][0]:
            planned, mode, reason = schedule[index]
            before = model.offset_ug
            model.set_mode(Mode.DOSING_NO_COMPENSATION if mode == "DOSING" else Mode.STATIC_COMPENSATION)
            transitions.append({"second": planned, "mode": mode, "reason": reason,
                                "offset_before_ug": before, "offset_after_ug": model.offset_ug})
            last_dosing = None
            index += 1
        mass = round(statistics.median(masses))
        snap = model.process_second(second, mass)
        trace.append({"second": second, "mode": mode_name(model),
                      "input_ug": mass, "corrected_ug": snap["corrected_gross_ug"],
                      "offset_ug": snap["offset_ug"], "state": snap["state"],
                      "automatic_rebuild_count": snap["automatic_rebase_count"]})
        if mode_name(model) == "DOSING":
            if last_dosing is not None:
                maximum_dosing_change = max(maximum_dosing_change,
                                            abs(last_dosing-snap["offset_ug"]))
            last_dosing = snap["offset_ug"]
    if trace_path is not None:
        with trace_path.open("w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(trace[0]), lineterminator="\n")
            writer.writeheader()
            writer.writerows(trace)
    checkpoints = {}
    for pos, (name, _, _) in enumerate(EVENTS):
        edge_s = (edges[name]["edge_center_monotonic_ns"]-start)/1e9
        phase_end = ((edges[EVENTS[pos+1][0]]["edge_center_monotonic_ns"]-start)/1e9
                     if pos+1 < len(EVENTS) else trace[-1]["second"]+1)
        early = [item for item in trace if 15 <= item["second"]-edge_s < 45
                 and item["second"] < phase_end]
        base = statistics.median(item["corrected_ug"] for item in early)
        checkpoints[name] = {}
        for minute in (1, 2, 5, 10, 15, 30):
            selected = [item for item in trace if minute*60 <= item["second"]-edge_s < (minute+1)*60
                        and item["second"] < phase_end]
            checkpoints[name][str(minute)] = (
                {"corrected_change_from_15_45s_ug": round(statistics.median(
                    item["corrected_ug"] for item in selected)-base),
                 "offset_median_ug": statistics.median(item["offset_ug"] for item in selected),
                 "seconds": len(selected)} if len(selected) >= 50 else
                {"status": "NOT RUN: incomplete 60-second window", "seconds": len(selected)})
    return {"actual_r5": "OFF, offset 0 throughout; this is counterfactual only",
            "mode_switches": transitions, "reset_count": 0,
            "one_second_trace_file": trace_path.name if trace_path is not None else None,
            "event_relative_checkpoints": checkpoints,
            "max_dosing_offset_change_ug": maximum_dosing_change,
            "automatic_rebuild_count": model.automatic_rebase_count,
            "final_mode": mode_name(model), "final_offset_ug": model.offset_ug,
            "limitations": "one-second median, not 10 Hz sample admission; retrospective edge-based schedule not a live control policy"}


def mode_name(model):
    return "DOSING" if model.mode == Mode.DOSING_NO_COMPENSATION else "STATIC"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path,
                        help="Results/stage5pa4/thermal_cycle")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    folder = args.input
    paths = {name: folder / name for name in ("empty_30m", "load1_30m", "continuous_3h")}
    rows = {name: read_csv(path / "samples.csv") for name, path in paths.items()}
    environment = {name: json.loads((path / "environment.json").read_text(encoding="utf-8"))
                   for name, path in paths.items()}
    summaries = {name: json.loads((path / "summary.json").read_text(encoding="utf-8"))
                 for name, path in paths.items()}
    events = [json.loads(line) for line in
              (paths["continuous_3h"] / "events.jsonl").read_text(encoding="utf-8").splitlines()]
    continuous = rows["continuous_3h"]
    edges = edges_from_samples(continuous, events)
    for name, _, is_load in EVENTS:
        edges[name]["panel_transient"] = panel_transient(continuous, edges[name], is_load)
    trend = control_trend(rows["empty_30m"])
    stages = {}
    for name, following in (("unload1", "load2"), ("load2", "unload2"),
                            ("unload2", None)):
        begin = edges[name]["edge_center_monotonic_ns"]
        end = edges[following]["edge_center_monotonic_ns"] if following else ns(continuous[-1])
        stages[name] = phase(continuous, begin, end, trend["linear_slope_ug_per_min"])
    # These are discontinuous files. Never substitute their start for the
    # physical first placement or join them into a fictitious continuous run.
    empty_end = rows["empty_30m"][-1]["utc"]
    loaded_start = rows["load1_30m"][0]["utc"]
    loaded_end = rows["load1_30m"][-1]["utc"]
    continuous_start = continuous[0]["utc"]
    report = {
        "schema_version": 1,
        "classification": "STAGE5PA4_NEW_DEVELOPMENT_MECHANISM_EVIDENCE_NOT_HOLDOUT",
        "units": "ADC counts; mass micrograms; panel integer display_count; UTC and monotonic nanoseconds",
        "quality": {name: quality(data, summaries[name], environment[name])
                    for name, data in rows.items()},
        "first_load": {"edge": "NOT OBSERVED", "operator_report": "LOAD_1_OPERATOR_CONFIRMED",
                       "empty_segment_last_utc": empty_end, "loaded_segment_first_utc": loaded_start,
                       "loaded_segment_last_utc": loaded_end,
                       "continuous_first_utc": continuous_start,
                       "event_relative_1_2_5_10_15_30min": "NOT SCORABLE",
                       "segment_start_relative_only": phase(rows["load1_30m"],
                           ns(rows["load1_30m"][0]), ns(rows["load1_30m"][-1])+1,
                           trend["linear_slope_ug_per_min"])},
        "empty_control": trend,
        "edges": {name: {key: item for key, item in data.items() if key != "row_index"}
                  for name, data in edges.items()},
        "event_relative_stages": stages,
        "counterfactual_r5": replay_r5(continuous, edges,
            args.output.with_name("r5_counterfactual_trace.csv")),
        "measured_filtered_weight_ug": None,
        "independent_uncompensated_diagnostic_ug": None,
        "authoritative_mass_column": "gross_ug; R5 remained OFF",
        "measured_temperature_c": None,
        "model_decision": "MODEL SELECTION INCOMPLETE"}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    with args.output.with_name("event_timeline.csv").open(
            "w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(("event", "operator_utc", "gross_before_utc", "gross_after_utc",
                         "bracket_width_s", "operator_lag_s", "raw_before_utc",
                         "raw_after_utc", "physical_step_ug", "classification"))
        writer.writerow(("load1", "2026-09-25T07:38:10.706Z", "", "", "", "",
                         "", "", "", "EDGE_NOT_RECORDED_SEGMENT_GAP"))
        for name in ("unload1", "load2", "unload2"):
            edge = report["edges"][name]
            writer.writerow((name, edge["operator_confirmation_utc"],
                *edge["gross_midpoint_bracket_utc"], edge["bracket_width_s"],
                edge["operator_message_lag_after_bracket_s"],
                *edge["raw_largest_jump_bracket_utc"], edge["physical_step_ug"],
                "HOST_POLL_BRACKET_NOT_DRDY"))
    print(json.dumps({"status": report["model_decision"],
                      "edges": {name: item["gross_midpoint_bracket_utc"]
                                for name, item in report["edges"].items()},
                      "records": len(continuous)}))


if __name__ == "__main__":
    main()
