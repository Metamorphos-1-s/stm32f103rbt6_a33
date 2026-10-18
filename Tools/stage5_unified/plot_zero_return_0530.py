"""Plot observed R5 block values; no interpolation or synthetic OFF control."""
import argparse
import csv
import gzip
import json
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--analysis", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    audit = json.loads((args.analysis / "analysis.json").read_text())
    events = audit["runs"]["three_round_edges"]["edges"]["events"]
    with gzip.open(args.analysis / "three_round_edges_recovered_r5.csv.gz", "rt", newline="") as stream:
        rows = list(csv.DictReader(stream))
    origin = int(rows[0]["host_monotonic_ns"])
    minutes, before, after, offset = [], [], [], []
    for row in rows:
        stamp = int(row["r5_rx_ns"])
        minutes.append((stamp - origin) / 60e9)
        current = int(row["r5_corrected_ug"])
        nominal = 500_000_000 if current >= 250_000_000 else 0
        omitted = any(abs(stamp - event["origin_ns"]) < 15e9 for event in events)
        before.append(float("nan") if omitted else (int(row["r5_uncompensated_ug"]) - nominal) / 1e6)
        after.append(float("nan") if omitted else (current - nominal) / 1e6)
        offset.append(int(row["r5_offset_ug"]) / 1e6)
    fig, axes = plt.subplots(2, 1, figsize=(11, 6), sharex=True,
                            gridspec_kw={"height_ratios": [2, 1]})
    axes[0].plot(minutes, before, color="#3478b8", lw=.7, alpha=.8, label="Before compensation (observed)")
    axes[0].plot(minutes, after, color="#16734b", lw=.7, alpha=.8, label="After compensation (observed)")
    axes[0].set_ylabel("Residual after nominal load subtraction (g)")
    axes[0].legend(loc="upper right", fontsize=9)
    axes[0].set_title("0x0530: loaded and empty plateaus from the same R5 response block", loc="left", pad=22)
    axes[1].plot(minutes, offset, color="#a34b28", lw=1.1)
    axes[1].set_ylabel("R5 offset (g)")
    axes[1].set_xlabel("Minutes since recording start")
    for index, event in enumerate(events, 1):
        x = (event["origin_ns"] - origin) / 60e9
        for axis in axes:
            axis.axvspan(x - .25, x + .25, color="#555555", alpha=.08)
            axis.axvline(x, color="#777777", lw=.6, ls="--")
        label = ("U" if event["direction"] == "UNLOAD" else "L") + str((index + 1) // 2)
        axes[0].text(x, 1.015, label, transform=axes[0].get_xaxis_transform(), ha="center", fontsize=9)
    for axis in axes:
        axis.grid(alpha=.18)
        axis.spines[["top", "right"]].set_visible(False)
    fig.text(.075, .018,
        "Sparse host observations; ±15 s around edges omitted from upper panel. Nominal 500 g is not a certified reference.",
        fontsize=8, color="#444444")
    fig.tight_layout(rect=(0, .04, 1, 1))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.output, dpi=180, metadata={"Software": "A33 offline zero-return review"})
    plt.close(fig)


if __name__ == "__main__":
    main()
