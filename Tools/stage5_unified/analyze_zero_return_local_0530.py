"""Offline analysis for the current 0x0530 read-only zero-return capture."""
import argparse
import json
import statistics
from pathlib import Path

THRESHOLD = 250_000_000


def read_rows(path):
    return [json.loads(line) for line in path.open(encoding="utf-8")]


def edge_rows(rows):
    edges = []
    for left, right in zip(rows, rows[1:]):
        before = left["authority"]["net_ug"] >= THRESHOLD
        after = right["authority"]["net_ug"] >= THRESHOLD
        if before != after:
            edges.append({"direction": "LOAD" if after else "UNLOAD",
                          "before": left, "after": right})
    return edges


def window(rows, edge, low, high):
    edge_ns = edge["after"]["host_monotonic_ns"]
    values = [row for row in rows if low * 1_000_000_000 <=
              row["host_monotonic_ns"] - edge_ns < high * 1_000_000_000]
    if not values:
        return {"rows": 0, "complete": False}
    authority = [row["authority"] for row in values]
    return {"rows": len(values), "complete": True,
            "first_utc": values[0]["utc"], "last_utc": values[-1]["utc"],
            "net_median_g": statistics.median(x["net_ug"] for x in authority) / 1e6,
            "net_mean_g": statistics.mean(x["net_ug"] for x in authority) / 1e6,
            "uncomp_median_g": statistics.median(x["uncompensated_ug"] for x in authority) / 1e6,
            "offset_median_g": statistics.median(x["applied_offset_ug"] for x in authority) / 1e6,
            "stable_fraction": statistics.mean(row["observation"]["stable"] for row in values),
            "max_gap_ms": max((b["host_monotonic_ns"] - a["host_monotonic_ns"])
                              for a, b in zip(values, values[1:])) / 1e6 if len(values) > 1 else 0}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--extension", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    rows = read_rows(args.capture)
    extension = read_rows(args.extension) if args.extension else []
    all_rows = rows + extension
    edges = edge_rows(all_rows)
    equations = [row for row in all_rows if
                 row["authority"]["gross_ug"] != row["authority"]["uncompensated_ug"] -
                 row["authority"]["applied_offset_ug"] or
                 row["authority"]["net_ug"] != row["authority"]["gross_ug"] -
                 row["authority"]["tare_ug"]]
    complete_cycles = []
    for index in range(len(edges) - 1):
        if edges[index]["direction"] == "LOAD" and edges[index + 1]["direction"] == "UNLOAD":
            load, unload = edges[index], edges[index + 1]
            cycle = {"load_edge": {"before_utc": load["before"]["utc"],
                                   "after_utc": load["after"]["utc"],
                                   "before_sequence": load["before"]["authority"]["sequence"],
                                   "after_sequence": load["after"]["authority"]["sequence"]},
                     "unload_edge": {"before_utc": unload["before"]["utc"],
                                     "after_utc": unload["after"]["utc"],
                                     "before_sequence": unload["before"]["authority"]["sequence"],
                                     "after_sequence": unload["after"]["authority"]["sequence"]},
                     "load_pre_45_15": window(all_rows, load, -45, -15),
                     "loaded_15_45": window(all_rows, load, 15, 45),
                     "unload_post_15_45": window(all_rows, unload, 15, 45),
                     "unload_post_60_90": window(all_rows, unload, 60, 90),
                     "unload_post_90_120": window(all_rows, unload, 90, 120)}
            complete_cycles.append(cycle)
    result = {"capture_rows": len(rows), "extension_rows": len(extension),
              "capture_sha256": __import__("hashlib").sha256(args.capture.read_bytes()).hexdigest().upper(),
              "equation_errors": len(equations), "edges": len(edges),
              "complete_cycles": complete_cycles,
              "scope": "FC03 authority is atomic; other blocks are separate requests; no interpolation",
              "tail_gap_requires_incomplete_600s": bool(extension)}
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"rows": len(rows), "extension_rows": len(extension),
                      "edges": len(edges), "complete_cycles": len(complete_cycles),
                      "equation_errors": len(equations)}))


if __name__ == "__main__":
    main()
