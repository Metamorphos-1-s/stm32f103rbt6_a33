#!/usr/bin/env python3
"""Compare multiple ADC noise runs and estimate layered variance increments."""

import argparse
import json
import math
from pathlib import Path
import sys

try:
    import numpy as np
    from png_plot import bar_plot
except ImportError as exc:
    raise SystemExit("install dependencies: pip install -r Tools/stage5b_hw/requirements.txt") from exc

from analyze_adc_noise import compute_statistics
from diag_common import load_csv_rows


def load_result(path):
    if path.suffix.lower() == ".json":
        return json.loads(path.read_text(encoding="utf-8"))
    if path.suffix.lower() == ".csv":
        result, _, _, _ = compute_statistics(load_csv_rows(path))
        return result
    raise ValueError("input must be analysis JSON or capture CSV: %s" % path)


def classify(result):
    name = result.get("test_id", "").lower()
    mode = result.get("mode", "").lower()
    if mode == "internal_short" or "internal_short" in name:
        return "internal_short"
    if "external" in name and ("short" in name or "common" in name):
        return "external_short"
    if "dummy" in name or "bridge" in name and "sensor" not in name:
        return "dummy_bridge"
    if "sensor" in name:
        return "real_sensor"
    return "other"


def layered_variance(results):
    groups = {}
    for result in results:
        groups.setdefault(classify(result), []).append(result)

    def variance(name):
        values = groups.get(name, [])
        if not values:
            return None
        return float(np.mean([item["detrended_std_count"] ** 2
                              for item in values]))

    internal = variance("internal_short")
    external = variance("external_short")
    bridge = variance("dummy_bridge")
    layers = {
        "assumption": "Approximation assumes independent noise sources. Correlated noise, drift, and mains interference cannot be separated by variance subtraction.",
        "var_adc": internal,
        "var_pcb_increment": None if internal is None or external is None else
            max(0.0, external - internal),
        "var_bridge_supply_increment": None if external is None or bridge is None else
            max(0.0, bridge - external),
        "real_sensor": [],
    }
    for result in groups.get("real_sensor", []):
        sensor_variance = result["detrended_std_count"] ** 2
        layers["real_sensor"].append({
            "test_id": result["test_id"],
            "var_sensor_mechanical_increment": None if bridge is None else
                max(0.0, sensor_variance - bridge),
        })
    return layers


def save_bar(path, results, metric, title, ylabel, absolute=False):
    labels = [item.get("test_id", "unnamed") for item in results]
    values = [abs(item[metric]) if absolute else item[metric] for item in results]
    bar_plot(path, title, ylabel, labels, values)


def table_value(value):
    if value is None or isinstance(value, float) and math.isnan(value):
        return "N/A"
    if isinstance(value, float):
        return "%.9g" % value
    return str(value)


def write_markdown(path, results, layers):
    columns = [
        "test_id", "mode", "load_g", "valid_samples", "duration_s",
        "mean_count", "std_count", "detrended_std_count", "peak_to_peak_count",
        "mad_count", "drift_count_per_min", "equivalent_std_g",
        "equivalent_peak_to_peak_g", "lost_samples", "fifo_overruns",
    ]
    lines = ["# ADC Noise Comparison", "",
             "Status: SOFTWARE ANALYSIS OF PROVIDED RUNS", "",
             "| " + " | ".join(columns) + " |",
             "|" + "---|" * len(columns)]
    for result in results:
        lines.append("| " + " | ".join(table_value(result.get(column))
                                         for column in columns) + " |")
    lines += ["", "## Approximate variance layers", "",
              "- `var_adc`: %s" % table_value(layers["var_adc"]),
              "- `var_pcb_increment`: %s" %
              table_value(layers["var_pcb_increment"]),
              "- `var_bridge_supply_increment`: %s" %
              table_value(layers["var_bridge_supply_increment"])]
    for sensor in layers["real_sensor"]:
        lines.append("- `%s` sensor/mechanical increment: %s" %
                     (sensor["test_id"],
                      table_value(sensor["var_sensor_mechanical_increment"])))
    lines += ["", "The decomposition assumes approximately independent noise "
              "sources. Correlated noise, drift, temperature effects, and mains "
              "interference cannot be isolated by simple variance subtraction. "
              "A single short run does not prove any component has reached its "
              "performance limit."]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", nargs="+", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("."))
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    results = [load_result(path) for path in args.inputs]
    layers = layered_variance(results)
    combined = {"runs": results, "variance_layers": layers}
    json_path = args.output_dir / "ADC_NOISE_COMPARISON.json"
    markdown_path = args.output_dir / "ADC_NOISE_COMPARISON.md"
    json_path.write_text(json.dumps(combined, indent=2, ensure_ascii=False) + "\n",
                         encoding="utf-8")
    write_markdown(markdown_path, results, layers)
    save_bar(args.output_dir / "comparison_std.png", results, "std_count",
             "ADC noise standard deviation", "Standard deviation (count)")
    save_bar(args.output_dir / "comparison_peak_to_peak.png", results,
             "peak_to_peak_count", "ADC noise peak-to-peak",
             "Peak-to-peak (count)")
    save_bar(args.output_dir / "comparison_drift.png", results,
             "drift_count_per_min", "Absolute linear drift",
             "Absolute drift (count/min)", absolute=True)
    print("Comparison:", markdown_path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
