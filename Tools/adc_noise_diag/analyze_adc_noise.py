#!/usr/bin/env python3
"""Analyze one ADC noise CSV and create JSON, Markdown, and PNG outputs."""

import argparse
import json
import math
from pathlib import Path
import sys

try:
    import numpy as np
    from png_plot import histogram, line_plot
except ImportError as exc:
    raise SystemExit("install dependencies: pip install -r Tools/stage5b_hw/requirements.txt") from exc

from diag_common import load_csv_rows


PERCENTILES = (1, 5, 25, 75, 95, 99)
AVERAGING_WINDOWS = (2, 4, 8, 16, 32)


def _float(row, name, default=math.nan):
    value = row.get(name, "")
    return default if value in (None, "") else float(value)


def _extend_u32(values):
    if not values:
        return np.array([], dtype=np.float64), 0
    result = [int(values[0])]
    anomalies = 0
    for previous, current in zip(values, values[1:]):
        delta = (int(current) - int(previous)) & 0xFFFFFFFF
        if delta == 0 or delta > 0x7FFFFFFF:
            anomalies += 1
            delta = 0
        result.append(result[-1] + delta)
    return np.asarray(result, dtype=np.float64), anomalies


def _block_average_std(values, window):
    count = len(values) // window
    if count < 2:
        return None
    averaged = values[:count * window].reshape(count, window).mean(axis=1)
    return float(np.std(averaged, ddof=1))


def _allan_deviation(values, sample_rate):
    output = []
    maximum = len(values) // 4
    window = 1
    while window <= maximum:
        count = len(values) // window
        averages = values[:count * window].reshape(count, window).mean(axis=1)
        if len(averages) >= 2:
            deviation = math.sqrt(0.5 * float(np.mean(np.diff(averages) ** 2)))
            output.append({"tau_s": window / sample_rate,
                           "deviation_count": deviation})
        window *= 2
    return output


def compute_statistics(rows):
    if len(rows) < 2:
        raise ValueError("at least two non-excluded samples are required")
    raw = np.asarray([int(row["raw_count"]) for row in rows], dtype=np.float64)
    host = np.asarray([float(row["elapsed_s"]) for row in rows], dtype=np.float64)
    host -= host[0]
    device_values = [int(row["device_timestamp_ms"]) for row in rows]
    device, timestamp_anomalies = _extend_u32(device_values)
    device = (device - device[0]) / 1000.0
    duration = float(device[-1]) if device[-1] > 0 else float(host[-1])
    if duration <= 0:
        raise ValueError("capture duration is not positive")
    sample_rate = (len(raw) - 1) / duration
    slope, intercept = np.polyfit(host, raw, 1)
    detrended = raw - (slope * host + intercept)
    median = float(np.median(raw))
    mad = float(np.median(np.abs(raw - median)))
    standard_deviation = float(np.std(raw, ddof=1))
    detrended_std = float(np.std(detrended, ddof=1))
    rms = float(math.sqrt(np.mean((raw - np.mean(raw)) ** 2)))
    quantiles = np.percentile(raw, PERCENTILES)
    lost_samples = max(int(row.get("lost_samples", 0) or 0) for row in rows)
    overruns = [int(row.get("overrun_count", 0) or 0) for row in rows]
    fifo_overruns = max(overruns) - min(overruns)
    device_deltas = np.diff(device)
    if len(device_deltas):
        median_period = float(np.median(device_deltas[device_deltas > 0])) \
            if np.any(device_deltas > 0) else 0.0
        if median_period > 0:
            timestamp_anomalies += int(np.sum(
                ((device_deltas > 0) &
                 (device_deltas < median_period * 0.25)) |
                (device_deltas > median_period * 3.0)))
    counts_per_g = next((_float(row, "counts_per_g") for row in rows
                         if row.get("counts_per_g", "") not in (None, "")), math.nan)
    averaging = {str(window): _block_average_std(detrended, window)
                 for window in AVERAGING_WINDOWS}
    result = {
        "test_id": rows[0].get("test_id", ""),
        "mode": rows[0].get("mode", ""),
        "load_g": _float(rows[0], "load_g", 0.0),
        "valid_samples": len(raw),
        "duration_s": duration,
        "average_sample_rate_hz": sample_rate,
        "mean_count": float(np.mean(raw)),
        "median_count": median,
        "minimum_count": int(np.min(raw)),
        "maximum_count": int(np.max(raw)),
        "peak_to_peak_count": int(np.max(raw) - np.min(raw)),
        "std_count": standard_deviation,
        "mad_count": mad,
        "percentiles_count": {str(percent): float(value)
                              for percent, value in zip(PERCENTILES, quantiles)},
        "rms_zero_mean_count": rms,
        "drift_count_per_min": float(slope * 60.0),
        "detrended_std_count": detrended_std,
        "maximum_adjacent_change_count": float(np.max(np.abs(np.diff(raw)))),
        "lost_samples": lost_samples,
        "fifo_overruns": fifo_overruns,
        "timestamp_anomalies": timestamp_anomalies,
        "counts_per_g": None if math.isnan(counts_per_g) else counts_per_g,
        "averaging_std_count": averaging,
        "allan_deviation": _allan_deviation(detrended, sample_rate),
        "notes": rows[0].get("notes", ""),
    }
    if not math.isnan(counts_per_g) and counts_per_g > 0:
        result["equivalent_std_g"] = standard_deviation / counts_per_g
        result["equivalent_detrended_std_g"] = detrended_std / counts_per_g
        result["equivalent_peak_to_peak_g"] = result["peak_to_peak_count"] / counts_per_g
        result["equivalent_mass_is_estimate"] = True
    return result, host, raw, detrended


def create_plots(output_dir, stats, host, raw, detrended):
    line_plot(output_dir / "raw_vs_time.png", "Raw ADC count vs time",
              "Elapsed time (s)", "ADC count", host, raw)
    line_plot(output_dir / "detrended_raw_vs_time.png",
               "Detrended raw ADC count", "Elapsed time (s)",
               "Detrended count", host, detrended)
    histogram(output_dir / "histogram.png", "Raw ADC count histogram",
              "ADC count", "Frequency", raw)
    window = max(2, int(round(stats["average_sample_rate_hz"] * 10.0)))
    rolling = np.array([np.ptp(raw[max(0, index - window + 1):index + 1])
                        for index in range(len(raw))])
    line_plot(output_dir / "rolling_peak_to_peak.png",
               "Rolling 10 s peak-to-peak", "Elapsed time (s)",
               "Peak-to-peak count", host, rolling)
    averaging = [(int(key), value) for key, value in
                 stats["averaging_std_count"].items() if value is not None]
    line_plot(output_dir / "averaging_noise.png",
               "Noise after non-overlapping averaging", "Average window (samples)",
               "Detrended standard deviation (count)",
               np.asarray([item[0] for item in averaging]),
               np.asarray([item[1] for item in averaging]))
    allan = stats["allan_deviation"]
    line_plot(output_dir / "allan_deviation.png", "Allan deviation",
               "Averaging time (s)", "Deviation (count)",
               np.asarray([item["tau_s"] for item in allan]),
               np.asarray([item["deviation_count"] for item in allan]))
    rate = stats["average_sample_rate_hz"]
    spectrum = np.fft.rfft(detrended)
    frequencies = np.fft.rfftfreq(len(detrended), d=1.0 / rate)
    psd = (np.abs(spectrum) ** 2) / (rate * len(detrended))
    limit = frequencies <= min(5.0, rate / 2.0)
    line_plot(output_dir / "psd_0_5hz.png",
               "Detrended PSD (Nyquist-limited)", "Frequency (Hz)",
               "Count^2/Hz", frequencies[limit], psd[limit])


def write_markdown(path, stats):
    lines = [
        "# ADC Noise Analysis: %s" % stats["test_id"], "",
        "Status: SOFTWARE ANALYSIS OF CAPTURED DATA", "",
        "| Metric | Value |", "|---|---:|",
        "| Mode | %s |" % stats["mode"],
        "| Valid samples | %d |" % stats["valid_samples"],
        "| Duration | %.3f s |" % stats["duration_s"],
        "| Effective sample rate | %.6f Hz |" % stats["average_sample_rate_hz"],
        "| Mean | %.6f count |" % stats["mean_count"],
        "| Median | %.6f count |" % stats["median_count"],
        "| Min / max | %d / %d count |" %
            (stats["minimum_count"], stats["maximum_count"]),
        "| Peak-to-peak | %d count |" % stats["peak_to_peak_count"],
        "| Sample std | %.6f count |" % stats["std_count"],
        "| Detrended std | %.6f count |" % stats["detrended_std_count"],
        "| MAD | %.6f count |" % stats["mad_count"],
        "| RMS zero-mean noise | %.6f count |" % stats["rms_zero_mean_count"],
        "| Linear drift | %.6f count/min |" % stats["drift_count_per_min"],
        "| Max adjacent change | %.6f count |" %
            stats["maximum_adjacent_change_count"],
        "| Lost samples | %d |" % stats["lost_samples"],
        "| FIFO overruns | %d |" % stats["fifo_overruns"],
        "| Timestamp anomalies | %d |" % stats["timestamp_anomalies"],
    ]
    if stats.get("equivalent_std_g") is not None:
        lines += ["", "Equivalent mass values are estimates derived from the "
                  "provided or existing calibration; they are not actual loads.", "",
                  "- Estimated standard deviation: %.9g g" % stats["equivalent_std_g"],
                  "- Estimated peak-to-peak: %.9g g" %
                  stats["equivalent_peak_to_peak_g"]]
    lines += ["", "At a 10 Hz output rate the Nyquist frequency is 5 Hz. This "
              "report cannot distinguish 50 Hz from 60 Hz mains interference "
              "directly from the output spectrum."]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--include-warmup", action="store_true")
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    output_dir = args.output_dir or args.csv.parent / (args.csv.stem + "_analysis")
    output_dir.mkdir(parents=True, exist_ok=True)
    rows = load_csv_rows(args.csv, include_excluded=args.include_warmup)
    stats, host, raw, detrended = compute_statistics(rows)
    json_path = output_dir / (args.csv.stem + ".json")
    markdown_path = output_dir / (args.csv.stem + ".md")
    json_path.write_text(json.dumps(stats, indent=2, ensure_ascii=False) + "\n",
                         encoding="utf-8")
    write_markdown(markdown_path, stats)
    create_plots(output_dir, stats, host, raw, detrended)
    print("JSON:", json_path)
    print("Markdown:", markdown_path)
    print("Plots:", output_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
