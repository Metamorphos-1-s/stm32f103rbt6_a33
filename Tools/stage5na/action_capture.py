#!/usr/bin/env python3
"""Capture Alarm SHADOW around one authorized volatile device action."""

import argparse
import csv
import json
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools" / "stage5b_hw"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from hw_common import ModbusClient, execute_command
from serial_transport import SerialTransport
from stage5na_hw import FIELDS, read_state

ACTIONS = {"zero": 1, "tare": 3, "clear-tare": 4}


def main():
    parser = argparse.ArgumentParser(); parser.add_argument("--port", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--action", choices=sorted(ACTIONS), required=True)
    parser.add_argument("--allow-control", action="store_true")
    parser.add_argument("--before-samples", type=int, default=20)
    parser.add_argument("--after-samples", type=int, default=80)
    args = parser.parse_args()
    if not args.allow_control: raise RuntimeError("action requires --allow-control")
    args.output.mkdir(parents=True, exist_ok=False); rows = []; last = None
    event = None
    with SerialTransport(args.port, 115200, "N", 1, 300) as transport:
        client = ModbusClient(transport, 1)
        while len(rows) < args.before_samples + args.after_samples:
            started = time.monotonic(); state = read_state(client)
            if state["sample_sequence"] != last:
                rows.append(state); last = state["sample_sequence"]
            if len(rows) == args.before_samples and event is None:
                token = int(time.time() * 1000) & 0xFFFF or 1
                response = execute_command(client, token, ACTIONS[args.action])
                event = {"utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                    "action": args.action, "after_sequence": last,
                    "response": response}
            delay = 0.09 - (time.monotonic() - started)
            if delay > 0: time.sleep(delay)
    with (args.output / "samples.csv").open("w", encoding="utf-8",
            newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS, lineterminator="\n")
        writer.writeheader(); writer.writerows({field: row[field]
            for field in FIELDS} for row in rows)
    (args.output / "event.json").write_text(json.dumps(event, indent=2) + "\n",
        encoding="utf-8")
    summary = {"status": "COMPLETE", "records": len(rows),
        "first_sequence": rows[0]["sample_sequence"],
        "final_sequence": rows[-1]["sample_sequence"], "event": event,
        "last_state": rows[-1]}
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n",
        encoding="utf-8")
    print(json.dumps(summary, indent=2)); return 0


if __name__ == "__main__": raise SystemExit(main())
