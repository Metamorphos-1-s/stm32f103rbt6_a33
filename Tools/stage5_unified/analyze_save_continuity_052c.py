"""Recompute the 0x052C SAVE continuity gates from archived CSV and events."""
import argparse
import csv
import hashlib
import json
from pathlib import Path


def analyze(run):
    samples = run / "samples.csv"
    rows = list(csv.DictReader(samples.open(newline="", encoding="utf-8")))
    events = json.loads((run / "events.json").read_text(encoding="utf-8"))
    completed = [event for event in events if event["kind"] == "SAVE_COMPLETED"]
    commands = [event for event in events if event["kind"] == "SAVE_COMMAND"]
    if not rows or len(completed) != 1 or len(commands) != 1:
        raise ValueError("missing samples or unique SAVE evidence")
    finish = completed[0]["value"]
    before = [row for row in rows if int(row["saved_revision"]) < int(finish["saved_revision"])]
    after = [row for row in rows if int(row["saved_revision"]) == int(finish["saved_revision"])]
    if not before or not after:
        raise ValueError("missing a SAVE boundary")
    equation_errors = sum(
        int(row["formal_gross_ug"]) !=
        int(row["uncompensated_gross_ug"]) - int(row["applied_offset_ug"]) or
        int(row["formal_net_ug"]) != int(row["formal_gross_ug"]) - int(row["tare_ug"])
        for row in rows)
    host_sequence_gaps = sum(
        int(next_row["sequence"]) - int(row["sequence"]) > 1
        for row, next_row in zip(rows, rows[1:]))
    output = {
        "source": str(samples),
        "sha256": hashlib.sha256(samples.read_bytes()).hexdigest().upper(),
        "rows": len(rows), "read_errors": len((run / "errors.jsonl").read_text().splitlines()),
        "save_commands": len(commands), "save_completed_events": len(completed),
        "save_result": commands[0]["response"]["result_name"],
        "before_rows": len(before), "after_rows": len(after),
        "before_last": {key: before[-1][key] for key in
                        ("utc", "sequence", "r5_offset_ug", "revision", "saved_revision")},
        "after_first": {key: after[0][key] for key in
                        ("utc", "sequence", "r5_offset_ug", "revision", "saved_revision")},
        "after_last": {key: after[-1][key] for key in
                       ("utc", "sequence", "r5_offset_ug", "revision", "saved_revision")},
        "post_save_wrong_r5_pair": sum(row["r5_application"] != "1" or row["r5_mode"] != "2"
                                       for row in after),
        "post_save_wrong_check_mode": sum(row["check_mode"] != "1" for row in after),
        "post_save_offset_changes": sum(row["r5_offset_ug"] != before[-1]["r5_offset_ug"]
                                        for row in after),
        "formal_weight_equation_errors": equation_errors,
        "host_poll_sequence_gaps": host_sequence_gaps,
        "fault_or_overrun_rows": sum(row["fault"] != "0" or row["overrun"] != "0"
                                     for row in rows),
        "post_save_dirty_rows": sum(row["dirty"] != "0" for row in after),
        "post_save_storage_sequences": sorted(set(row["storage_sequence"] for row in after)),
    }
    return output


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("run", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = analyze(args.run)
    serialized = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.write_text(serialized, encoding="utf-8")
    print(serialized)


if __name__ == "__main__":
    main()
