"""Check 0x052A atomic weight invariants without inventing missing samples."""
import argparse
import csv
import json
from pathlib import Path


def analyze(path):
    result = {"rows": 0, "weight_mismatches": 0, "net_mismatches": 0,
              "sequence_regressions": 0, "host_poll_gaps_over_2s": 0,
              "fault_rows": 0, "overrun_rows": 0, "save_rows": 0,
              "min_uncompensated_ug": None, "max_uncompensated_ug": None,
              "max_weight_error_ug": 0, "max_net_error_ug": 0}
    previous_sequence = previous_host = None
    with path.open(encoding="utf-8", newline="") as stream:
        for row in csv.DictReader(stream):
            result["rows"] += 1
            get = lambda field: int(row[field])
            raw = get("uncompensated_gross_ug")
            applied = get("applied_offset_ug") if get("offset_applied") else 0
            formal = get("formal_gross_ug")
            net = get("formal_net_ug")
            gross_error = formal - (raw - applied)
            net_error = net - (formal - get("tare_ug"))
            result["weight_mismatches"] += gross_error != 0
            result["net_mismatches"] += net_error != 0
            result["max_weight_error_ug"] = max(result["max_weight_error_ug"], abs(gross_error))
            result["max_net_error_ug"] = max(result["max_net_error_ug"], abs(net_error))
            sequence = get("sequence")
            host = get("host_monotonic_ns")
            result["sequence_regressions"] += previous_sequence is not None and sequence < previous_sequence
            result["host_poll_gaps_over_2s"] += previous_host is not None and host - previous_host > 2_000_000_000
            result["fault_rows"] += get("fault") != 0
            result["overrun_rows"] += get("overrun") != 0
            result["save_rows"] += get("saved_revision") != 22
            result["min_uncompensated_ug"] = raw if result["min_uncompensated_ug"] is None else min(result["min_uncompensated_ug"], raw)
            result["max_uncompensated_ug"] = raw if result["max_uncompensated_ug"] is None else max(result["max_uncompensated_ug"], raw)
            previous_sequence, previous_host = sequence, host
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("csv", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = analyze(args.csv)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result))
    return int(result["weight_mismatches"] != 0 or result["net_mismatches"] != 0)


if __name__ == "__main__":
    raise SystemExit(main())
