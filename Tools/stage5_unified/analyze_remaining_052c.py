"""Recompute same-sample and zero-indicator evidence for Stage 5P 0x052C."""
import argparse
import csv
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = {"runs": {}}
    for csv_path in sorted(args.root.glob("*/samples.csv")):
        rows = list(csv.DictReader(csv_path.open(newline="", encoding="utf-8")))
        if "active_sequence" not in (rows[0] if rows else {}):
            result["runs"][csv_path.parent.name] = {
                "rows": len(rows),
                "sha256": hashlib.sha256(csv_path.read_bytes()).hexdigest().upper(),
                "read_errors": len((csv_path.parent / "errors.jsonl").read_text().splitlines()),
                "kind": "zero_status_or_power_cycle_record",
            }
            continue
        matched = [row for row in rows
                   if row.get("active_sequence") == row.get("shadow_sequence")]
        result["runs"][csv_path.parent.name] = {
            "rows": len(rows),
            "sha256": hashlib.sha256(csv_path.read_bytes()).hexdigest().upper(),
            "matched_sequence_rows": len(matched),
            "nonzero_offset_matched": sum(int(row["r5_offset_ug"]) != 0 for row in matched),
            "static_net_mismatch": sum(int(row["static_input_ug"]) !=
                                        int(row["formal_net_ug"]) for row in matched),
            "formal_weight_equation_mismatch": sum(
                int(row["formal_gross_ug"]) != int(row["uncompensated_gross_ug"]) -
                int(row["r5_offset_ug"]) for row in matched),
            "read_errors": len((csv_path.parent / "errors.jsonl").read_text().splitlines()),
        }
    text = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.write_text(text, encoding="utf-8")
    print(text)


if __name__ == "__main__":
    main()
