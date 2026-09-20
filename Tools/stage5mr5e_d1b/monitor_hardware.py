#!/usr/bin/env python3
"""Read-only D1-B nonzero-offset and panel-follow qualification recorder."""

import argparse
import csv
import json
import os
import sys
import time
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools" / "stage5b_hw"))
sys.path.insert(0, str(ROOT / "Tools" / "stage5mr5b_beta"))
from hw_common import ModbusClient
from serial_transport import SerialTransport
from modbus_frame import decode_i64_words
import r5_beta_hw
from display_model import quantize_count


FIELDS = ("utc", "uptime_ms", "sample_sequence", "firmware", "map",
    "application", "mode", "state", "limited", "reason", "offset_ug",
    "reference_ug", "reference_error_ug", "uncompensated_gross_ug",
    "corrected_gross_ug", "net_mass_ug", "gross_mass_ug", "display_count",
    "display_decimals", "desired_display_count", "display_condition_state",
    "display_locked", "display_mass_ug", "display_anchor_ug",
    "release_threshold_ug", "display_candidate_elapsed_ms",
    "display_release_reason", "stable", "fault_mask", "overrun_count",
    "dirty", "revision", "saved_revision", "save_request_count_low")


def atomic_json(path, value):
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    os.replace(str(temporary), str(path))


def display_state(client, order):
    words, _ = client.read(0x01E0, 17)
    return {"display_condition_state": words[0], "display_locked": words[1],
        "display_mass_ug": decode_i64_words(words[2:6], order),
        "display_anchor_ug": decode_i64_words(words[6:10], order),
        "release_threshold_ug": decode_i64_words(words[10:14], order),
        "display_candidate_elapsed_ms": (words[14] << 16) | words[15],
        "display_release_reason": words[16]}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--duration-s", type=int, default=43200)
    parser.add_argument("--poll-s", type=float, default=1.0)
    parser.add_argument("--offset-threshold-ug", type=int, default=20000)
    parser.add_argument("--division-counts", type=int, default=2)
    parser.add_argument("--expected-firmware", type=lambda value: int(value, 0),
        default=0x0512)
    args = parser.parse_args()
    r5_beta_hw.EXPECTED_FIRMWARE = args.expected_firmware
    output = Path(args.output); output.mkdir(parents=True, exist_ok=True)
    sample_path = output / "samples.csv"
    started = time.monotonic(); records = errors = 0
    first_desired = None; desired_min = desired_max = None
    condition_started = None; recent = deque(); max_lag = max_jump = 0
    previous_display = None; reversals = 0; previous_direction = 0
    status = "RUNNING"; last = None
    transport = SerialTransport(args.port, 115200, "N", 1, 500)
    client = ModbusClient(transport, 1)
    try:
        with sample_path.open("w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=FIELDS, lineterminator="\n")
            writer.writeheader()
            deadline = started + args.duration_s
            next_poll = started
            while time.monotonic() < deadline:
                try:
                    state = r5_beta_hw.read_state(client)
                    panel = display_state(client, state["word_order"])
                except Exception:
                    errors += 1
                    time.sleep(0.2)
                    continue
                desired = quantize_count(state["net_mass_ug"], 1000000, 100, 1)
                row = {key: state.get(key, panel.get(key, 0)) for key in FIELDS}
                row.update(panel); row["desired_display_count"] = desired
                row["stable"] = 1 if state["status_flags"] & 16 else 0
                writer.writerow(row); stream.flush(); records += 1; last = row
                if first_desired is None:
                    first_desired = desired; desired_min = desired_max = desired
                desired_min = min(desired_min, desired); desired_max = max(desired_max, desired)
                lag = abs(state["display_count"] - desired); max_lag = max(max_lag, lag)
                if previous_display is not None:
                    delta = state["display_count"] - previous_display
                    max_jump = max(max_jump, abs(delta))
                    direction = 1 if delta > 0 else -1 if delta < 0 else 0
                    if direction and previous_direction and direction != previous_direction:
                        reversals += 1
                    if direction: previous_direction = direction
                previous_display = state["display_count"]
                qualifies = (abs(state["offset_ug"]) >= args.offset_threshold_ug and
                    desired_max - desired_min >= args.division_counts and
                    state["application"] == 1 and state["mode"] == 2 and
                    state["state"] == 5 and row["stable"] and
                    state["fault_mask"] == 0 and state["overrun_count"] == 0 and
                    state["dirty"] == 0)
                if qualifies:
                    if condition_started is None: condition_started = time.monotonic()
                    if time.monotonic() - condition_started >= 5.0:
                        status = "QUALIFIED_TRIGGER_REACHED"; break
                else: condition_started = None
                atomic_json(output / "summary.json", {"status": status,
                    "duration_s": time.monotonic() - started, "records": records,
                    "read_errors": errors, "last_state": last,
                    "desired_min": desired_min, "desired_max": desired_max,
                    "maximum_lag_divisions": max_lag,
                    "maximum_jump_divisions": max_jump,
                    "display_direction_reversals": reversals})
                next_poll += args.poll_s
                time.sleep(max(0, next_poll - time.monotonic()))
    except KeyboardInterrupt:
        status = "INTERRUPTED"
    finally:
        transport.close()
    if status == "RUNNING": status = "TIMEOUT_INSUFFICIENT_NATURAL_DRIFT"
    atomic_json(output / "summary.json", {"status": status,
        "duration_s": time.monotonic() - started, "records": records,
        "read_errors": errors, "last_state": last,
        "desired_min": desired_min, "desired_max": desired_max,
        "maximum_lag_divisions": max_lag, "maximum_jump_divisions": max_jump,
        "display_direction_reversals": reversals})
    return 0 if status == "QUALIFIED_TRIGGER_REACHED" else 3


if __name__ == "__main__": raise SystemExit(main())
