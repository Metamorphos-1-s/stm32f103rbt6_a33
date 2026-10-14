"""Offline, monotonic-time scoring of focused 0x052B load traces."""
import argparse
import csv
import json
from pathlib import Path
from statistics import median


def read_rows(path):
    with path.open(encoding="utf-8", newline="") as stream:
        return [{key: (value if key == "utc" else int(value))
                 for key, value in row.items()}
                for row in csv.DictReader(stream)]


def window(rows, start_ns, end_ns):
    selected = [row for row in rows if start_ns <= row["host_monotonic_ns"] < end_ns]
    if not selected:
        return None
    result = {field + "_median_ug": median(row[field] for row in selected)
              for field in ("uncompensated_gross_ug", "formal_gross_ug",
                            "formal_net_ug", "applied_offset_ug")}
    result["rows"] = len(selected)
    return result


def analyze(path):
    rows = read_rows(path)
    if not rows:
        raise ValueError("empty trace")
    loads = [i for i, row in enumerate(rows)
             if row["uncompensated_gross_ug"] > 100_000_000]
    if not loads or loads[0] == 0 or loads[-1] == len(rows) - 1:
        raise ValueError("trace lacks complete load/unload edges")
    load_i, last_load_i = loads[0], loads[-1]
    unload_i = next(i for i in range(last_load_i + 1, len(rows))
                    if rows[i]["uncompensated_gross_ug"] < 100_000_000)
    load_ns = rows[load_i]["host_monotonic_ns"]
    unload_ns = rows[unload_i]["host_monotonic_ns"]
    seconds = 1_000_000_000
    baseline = window(rows, load_ns - 5 * seconds, load_ns)
    loaded = window(rows, load_ns + 15 * seconds,
                    min(load_ns + 45 * seconds, unload_ns))
    returned = window(rows, unload_ns + 15 * seconds,
                      unload_ns + 45 * seconds)
    if baseline is None or loaded is None or returned is None:
        raise ValueError("insufficient stable scoring windows")
    gross = "uncompensated_gross_ug_median_ug"
    formal = "formal_gross_ug_median_ug"
    values = dict(source=str(path), rows=len(rows),
        first_utc=rows[0]["utc"], last_utc=rows[-1]["utc"],
        load_bracket=[rows[load_i - 1]["utc"], rows[load_i]["utc"]],
        unload_bracket=[rows[last_load_i]["utc"], rows[unload_i]["utc"]],
        baseline=baseline, load_15_45s=loaded, unload_15_45s=returned,
        uncompensated_span_ug=loaded[gross] - baseline[gross],
        formal_span_ug=loaded[formal] - baseline[formal],
        uncompensated_unload_step_ug=returned[gross] - loaded[gross],
        formal_unload_step_ug=returned[formal] - loaded[formal],
        formal_low_rows=sum(row["check_formal"] == 1 for row in rows),
        formal_high_rows=sum(row["check_formal"] == 3 for row in rows),
        formal_disabled_rows=sum(row["check_formal"] == 0 for row in rows),
        offset_min_ug=min(row["applied_offset_ug"] for row in rows),
        offset_max_ug=max(row["applied_offset_ug"] for row in rows),
        weight_equation_mismatches=sum(row["formal_gross_ug"] !=
            row["uncompensated_gross_ug"] -
            (row["applied_offset_ug"] if row["offset_applied"] else 0)
            for row in rows),
        net_equation_mismatches=sum(row["formal_net_ug"] !=
            row["formal_gross_ug"] - row["tare_ug"] for row in rows),
        fault_rows=sum(row["fault"] != 0 for row in rows),
        overrun_rows=sum(row["overrun"] != 0 for row in rows),
        sequence_regressions=sum(rows[i]["sequence"] < rows[i - 1]["sequence"]
                                 for i in range(1, len(rows))),
        host_gaps_gt_2s=sum(rows[i]["host_monotonic_ns"] -
            rows[i - 1]["host_monotonic_ns"] > 2 * seconds
            for i in range(1, len(rows))))
    if "shadow_sequence" in rows[0]:
        values["shadow_same_sample_rows"] = sum(row["same_sample"] for row in rows)
    return values


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = analyze(args.input)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
