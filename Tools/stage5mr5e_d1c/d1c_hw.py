#!/usr/bin/env python3
"""Read-only true-sample D1-C display qualification recorder."""

import argparse
import csv
import json
import os
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools" / "stage5b_hw"))
sys.path.insert(0, str(ROOT / "Tools" / "stage5mr5b_beta"))
from hw_common import HardwareTestError, ModbusClient, execute_command
from serial_transport import SerialTransport
from r5_beta_hw import COMMAND_SET_APPLICATION, i32, i64


D1C_FIRST = 0x02A8
D1C_COUNT = 24
FIELDS = ("utc", "uptime_ms", "sample_sequence", "follower_sequence",
    "firmware", "map", "authoritative_display_input_ug", "panel_display_count",
    "desired_count", "display_count", "delta_count", "desired_division",
    "display_division", "delta_divisions", "anchor_division", "direction",
    "evidence", "anchor_count", "locked", "stable", "large_step",
    "release_reason", "division", "application", "mode", "state",
    "uncompensated_gross_ug", "corrected_gross_ug", "offset_ug",
    "reference_ug", "automatic_rebase_count", "fault_mask", "overrun_count",
    "dirty", "save_request_count_low", "revision", "saved_revision")


def signed16(value):
    return value - 0x10000 if value & 0x8000 else value


def signed8(value):
    return value - 0x100 if value & 0x80 else value


def decode_d1c(words, order="high"):
    if len(words) != D1C_COUNT or words[0] != 0xD1C1:
        raise HardwareTestError("D1-C diagnostic signature mismatch")
    flags = words[13]
    return {"d1c_signature": "0x%04X" % words[0],
        "firmware": "0x0514", "map": "0x0104", "persistent_format": 3,
        "sample_sequence": (words[1] << 16) | words[2],
        "follower_sequence": (words[1] << 16) | words[2],
        "uptime_ms": (words[3] << 16) | words[4],
        "authoritative_display_input_ug": i32(words[5:7], order) * 100,
        "desired_division": i32(words[7:9], order),
        "display_division": i32(words[9:11], order),
        "delta_divisions": i32(words[7:9], order) - i32(words[9:11], order),
        "evidence": signed8(words[11] >> 8),
        "direction": signed8(words[11] & 0xFF),
        "anchor_division": i32(words[9:11], order),
        "initialized": int(bool((words[12] >> 8) & 1)),
        "locked": int(bool((words[12] >> 8) & 2)),
        "large_step": int(bool((words[12] >> 8) & 4)),
        "stable": int(bool((words[12] >> 8) & 8)),
        "active": int(bool((words[12] >> 8) & 16)),
        "limited": int(bool((words[12] >> 8) & 32)),
        "release_reason": (words[12] >> 4) & 0x0F,
        "division": words[12] & 0x0F, "application": words[13] >> 12,
        "mode": (words[13] >> 8) & 0x0F, "state": words[13] & 0xFF,
        "uncompensated_gross_ug": i32(words[14:16], order) * 100,
        "corrected_gross_ug": i32(words[16:18], order) * 100,
        "offset_ug": i32(words[18:20], order) * 100,
        "reference_ug": i32(words[20:22], order) * 100,
        "automatic_rebase_count": (words[22] >> 8) & 0x0F,
        "fault_mask": (words[22] >> 4) & 0x0F,
        "overrun_count": words[22] & 0x0F,
        "dirty": int(bool(words[22] & 0x8000)),
        "save_request_count_low": (words[22] >> 12) & 0x07,
        "revision": words[23] >> 8, "saved_revision": words[23] & 0xFF}


def atomic_json(path, value):
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    os.replace(str(temporary), str(path))


def combined_state(client, retries=3):
    del retries
    words, _ = client.read(D1C_FIRST, D1C_COUNT)
    state = decode_d1c(words, "high")
    if state["firmware"] != "0x0514" or state["map"] != "0x0104":
        raise HardwareTestError("expected D1-C Firmware 0x0514 / Map 0x0104")
    state["utc"] = time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime()) + \
        ".%03dZ" % int((time.time() % 1) * 1000)
    state["desired_count"] = state["desired_division"] * state["division"]
    state["display_count"] = state["display_division"] * state["division"]
    state["panel_display_count"] = state["display_count"]
    state["delta_count"] = state["delta_divisions"] * state["division"]
    state["anchor_count"] = state["anchor_division"] * state["division"]
    return state


def record(args):
    output = Path(args.output); output.mkdir(parents=True, exist_ok=False)
    samples_path = output / "samples.csv"
    frames_path = output / "frames.jsonl"
    started = time.monotonic(); last_sequence = None; last_utc = None
    next_poll = started
    records = duplicates = errors = reconnects = maximum_gap = 0
    first_sequence = final_sequence = None; final = None
    desired_min = desired_max = trigger_monotonic = None
    status = "COMPLETE"
    frame_stream = None if args.no_frames else frames_path.open(
        "a", encoding="utf-8", newline="")
    def frame_log(line):
        if frame_stream is not None:
            frame_stream.write(json.dumps({"monotonic_ns": time.monotonic_ns(),
                "frame": line}, separators=(",", ":")) + "\n")
    transport = SerialTransport(args.port, args.baud, args.parity,
        args.stopbits, args.timeout_ms,
        frame_logger=None if args.no_frames else frame_log)
    client = ModbusClient(transport, args.slave)
    try:
        if args.activate_before_record:
            if not args.allow_control:
                raise HardwareTestError(
                    "--activate-before-record requires --allow-control")
            token = int(time.time() * 1000) & 0xFFFF or 1
            response = execute_command(client, token,
                COMMAND_SET_APPLICATION, arg0=1)
            atomic_json(output / "activation_event.json", {
                "utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                "command": "SET_APPLICATION_ACTIVE", "response": response})
        with samples_path.open("w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=FIELDS,
                lineterminator="\n")
            writer.writeheader()
            while time.monotonic() - started < args.duration_s:
                delay = next_poll - time.monotonic()
                if delay > 0:
                    time.sleep(delay)
                request_started = time.monotonic()
                try:
                    state = combined_state(client)
                except Exception:
                    errors += 1
                    if errors > args.max_errors:
                        raise
                    next_poll = time.monotonic() + args.poll_s
                    continue
                sequence = state["sample_sequence"]
                if sequence == last_sequence:
                    duplicates += 1
                    next_poll = time.monotonic() + args.duplicate_retry_s
                    continue
                next_poll = request_started + args.poll_s
                now = time.monotonic()
                if last_utc is not None:
                    maximum_gap = max(maximum_gap, now - last_utc)
                last_utc = now; last_sequence = sequence
                first_sequence = sequence if first_sequence is None else first_sequence
                final_sequence = sequence; final = state
                desired_min = state["desired_count"] if desired_min is None else \
                    min(desired_min, state["desired_count"])
                desired_max = state["desired_count"] if desired_max is None else \
                    max(desired_max, state["desired_count"])
                writer.writerow({field: state.get(field, 0) for field in FIELDS})
                records += 1
                if records % 10 == 0:
                    stream.flush()
                    if frame_stream is not None:
                        frame_stream.flush()
                if records % 100 == 0:
                    atomic_json(output / "summary.json", {
                        "status": "RUNNING", "records": records,
                        "first_sequence": first_sequence,
                        "final_sequence": final_sequence,
                        "duplicates_skipped": duplicates,
                        "read_errors": errors, "reconnects": reconnects,
                        "maximum_accepted_gap_s": maximum_gap,
                        "last_state": final})
                trigger_ready = args.stop_on_trigger and \
                    abs(state["offset_ug"]) >= args.offset_threshold_ug and \
                    desired_max - desired_min >= args.division_crossing and \
                    state["application"] == 1 and state["mode"] == 2 and \
                    state["state"] == 5 and state["stable"] and \
                    state["fault_mask"] == 0 and state["overrun_count"] == 0 and \
                    state["dirty"] == 0 and state["save_request_count_low"] == 0
                if trigger_ready and trigger_monotonic is None:
                    trigger_monotonic = time.monotonic()
                    atomic_json(output / "trigger_event.json", {
                        "utc": state["utc"], "record": records,
                        "sample_sequence": sequence, "offset_ug": state["offset_ug"],
                        "desired_min": desired_min, "desired_max": desired_max})
                if trigger_monotonic is not None and \
                        time.monotonic() - trigger_monotonic >= args.post_trigger_s:
                    status = "TRIGGER_CAPTURED"
                    break
    finally:
        transport.close()
        if frame_stream is not None:
            frame_stream.close()
    expected = (None if first_sequence is None else
        ((final_sequence - first_sequence) & 0xFFFFFFFF) + 1)
    summary = {"status": status, "duration_s": time.monotonic() - started,
        "records": records, "expected_sequence_records": expected,
        "coverage": None if not expected else records / expected,
        "first_sequence": first_sequence, "final_sequence": final_sequence,
        "duplicates_skipped": duplicates, "read_errors": errors,
        "reconnects": reconnects, "maximum_accepted_gap_s": maximum_gap,
        "desired_min": desired_min, "desired_max": desired_max,
        "trigger_reached": trigger_monotonic is not None,
        "last_state": final, "writes": 0, "flash_operations": 0}
    atomic_json(output / "summary.json", summary)
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--parity", default="N")
    parser.add_argument("--stopbits", type=int, default=1)
    parser.add_argument("--slave", type=int, default=1)
    parser.add_argument("--timeout-ms", type=int, default=300)
    parser.add_argument("--duration-s", type=float, required=True)
    parser.add_argument("--max-errors", type=int, default=10)
    parser.add_argument("--poll-s", type=float, default=0.1)
    parser.add_argument("--duplicate-retry-s", type=float, default=0.015)
    parser.add_argument("--no-frames", action="store_true")
    parser.add_argument("--activate-before-record", action="store_true")
    parser.add_argument("--allow-control", action="store_true")
    parser.add_argument("--stop-on-trigger", action="store_true")
    parser.add_argument("--offset-threshold-ug", type=int, default=20000)
    parser.add_argument("--division-crossing", type=int, default=2)
    parser.add_argument("--post-trigger-s", type=float, default=300)
    return record(parser.parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
