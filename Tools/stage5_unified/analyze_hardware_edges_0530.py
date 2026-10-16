"""Bracket 500 g load edges from sparse non-atomic display recordings."""
import argparse
import csv
import hashlib
import json
from pathlib import Path

MASS_THRESHOLD_UG = 250_000_000
DISPLAY_THRESHOLD_COUNT = 25_000


def analyze(path):
    rows = list(csv.DictReader(path.open(newline="", encoding="utf-8")))
    if not rows:
        raise ValueError("empty display recording")
    crossings = []
    for index in range(1, len(rows)):
        before = rows[index - 1]
        after = rows[index]
        before_loaded = int(before["net_ug"]) >= MASS_THRESHOLD_UG
        after_loaded = int(after["net_ug"]) >= MASS_THRESHOLD_UG
        if before_loaded == after_loaded:
            continue
        direction = "LOAD" if after_loaded else "UNLOAD"
        observed_display_index = None
        for position in range(index, len(rows)):
            display_loaded = int(rows[position]["display_count"]) >= DISPLAY_THRESHOLD_COUNT
            if display_loaded == after_loaded:
                observed_display_index = position
                break
        visible = rows[observed_display_index] if observed_display_index is not None else None
        crossings.append({
            "direction": direction,
            "weight_bracket": {
                "before_utc": before["utc"], "after_utc": after["utc"],
                "before_monotonic_ns": int(before["host_monotonic_ns"]),
                "after_monotonic_ns": int(after["host_monotonic_ns"]),
                "before_sequence": int(before["sequence"]),
                "after_sequence": int(after["sequence"]),
                "before_net_ug": int(before["net_ug"]),
                "after_net_ug": int(after["net_ug"]),
            },
            "first_observed_display_crossing": None if visible is None else {
                "utc": visible["utc"], "monotonic_ns": int(visible["host_monotonic_ns"]),
                "sequence": int(visible["sequence"]),
                "display_count": int(visible["display_count"]),
                "relative_to_weight_after_ms": round((int(visible["host_monotonic_ns"]) -
                    int(after["host_monotonic_ns"])) / 1_000_000, 3),
            },
        })
    result = {
        "input": str(path),
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest().upper(),
        "rows": len(rows),
        "snapshot_atomic": False,
        "threshold_net_ug": MASS_THRESHOLD_UG,
        "threshold_display_count": DISPLAY_THRESHOLD_COUNT,
        "host_poll_sequence_gaps": sum(int(right["sequence"]) - int(left["sequence"]) > 1
            for left, right in zip(rows, rows[1:])),
        "device_sequence_regressions": sum(int(right["sequence"]) <= int(left["sequence"])
            for left, right in zip(rows, rows[1:])),
        "fault_rows": sum(int(row["fault"]) != 0 for row in rows),
        "overrun_rows": sum(int(row["overrun"]) != 0 for row in rows),
        "display_count_changes": sum(left["display_count"] != right["display_count"]
            for left, right in zip(rows, rows[1:])),
        "crossings": crossings,
        "limit": "Each row joins separate FC03 requests; brackets are observations, not exact physical edge times.",
    }
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("csv_path", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = analyze(args.csv_path)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"rows": result["rows"], "crossings": len(result["crossings"]),
                      "fault_rows": result["fault_rows"], "overrun_rows": result["overrun_rows"]}))


if __name__ == "__main__":
    main()
