#!/usr/bin/env python3
"""Stage 5N-A engineering-only Alarm SHADOW recorder and controls."""

import argparse
import csv
import json
import os
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools" / "stage5b_hw"))
from hw_common import HardwareTestError, ModbusClient, execute_command
from modbus_frame import decode_i32_words, decode_i64_words
from serial_transport import SerialTransport


FIRST = 0x02E0
COUNT = 31
COMMAND_SET_LIMITS = 33
FIELDS = ("utc", "sample_sequence", "timestamp_ms", "static_input_ug",
    "dynamic_input_ug", "low_limit_ug", "high_limit_ug",
    "static_immediate", "static_class", "static_stable_count",
    "static_reason", "dynamic_immediate", "dynamic_candidate",
    "dynamic_confirmed", "dynamic_confirm_count", "dynamic_reason",
    "process_active", "valid", "event_count", "formal_state",
    "formal_alarm_active", "formal_green", "formal_yellow", "formal_red",
    "formal_internal_buzzer", "formal_external_buzzer", "official_stable",
    "official_overload", "fault", "overrun", "dirty", "save_count",
    "revision", "saved_revision", "r5_input_ug")


def decode(words, order="high"):
    if len(words) != COUNT or words[0] != 0x5AA5:
        raise HardwareTestError("Stage 5N-A diagnostic signature mismatch")
    classes, static_detail, dynamic_detail = words[21:24]
    formal, safety, revision_saved = words[24:27]
    return {"signature": "0x%04X" % words[0],
        "static_input_ug": decode_i64_words(words[1:5], order),
        "dynamic_input_ug": decode_i64_words(words[5:9], order),
        "low_limit_ug": decode_i64_words(words[9:13], order),
        "high_limit_ug": decode_i64_words(words[13:17], order),
        "sample_sequence": (words[17] << 16) | words[18],
        "event_count": (words[19] << 16) | words[20],
        "static_immediate": (classes >> 12) & 0x0F,
        "static_class": (classes >> 8) & 0x0F,
        "dynamic_immediate": (classes >> 4) & 0x0F,
        "dynamic_confirmed": classes & 0x0F,
        "static_stable_count": static_detail >> 8,
        "static_reason": static_detail & 0xFF,
        "dynamic_candidate": (dynamic_detail >> 12) & 0x0F,
        "dynamic_confirm_count": (dynamic_detail >> 8) & 0x0F,
        "dynamic_reason": (dynamic_detail >> 4) & 0x0F,
        "process_active": int(bool(dynamic_detail & 2)),
        "valid": int(bool(dynamic_detail & 1)),
        "formal_state": formal & 0x0F,
        "formal_alarm_active": int(bool(formal & 0x10)),
        "formal_green": int(bool(formal & 0x20)),
        "formal_yellow": int(bool(formal & 0x40)),
        "formal_red": int(bool(formal & 0x80)),
        "formal_internal_buzzer": int(bool(formal & 0x100)),
        "formal_external_buzzer": int(bool(formal & 0x200)),
        "official_stable": int(bool(formal & 0x400)),
        "official_overload": int(bool(formal & 0x800)),
        "dirty": int(bool(safety & 0x8000)),
        "save_count": (safety >> 12) & 0x07,
        "fault": int(bool(safety & 0x0F00)),
        "overrun": (safety >> 4) & 0x0F,
        "revision": revision_saved >> 8,
        "saved_revision": revision_saved & 0xFF,
        "r5_input_ug": decode_i32_words(words[27:29], order) * 100,
        "timestamp_ms": (words[29] << 16) | words[30]}


def read_state(client):
    words, _ = client.read(FIRST, COUNT)
    value = decode(words)
    value["utc"] = time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime()) + \
        ".%03dZ" % int((time.time() % 1) * 1000)
    return value


def atomic_json(path, value):
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    os.replace(str(temporary), str(path))


def record(args, client):
    output = Path(args.output); output.mkdir(parents=True, exist_ok=False)
    started = time.monotonic(); next_poll = started; last_sequence = None
    records = duplicates = errors = 0; first_sequence = final_sequence = None
    final = None; maximum_gap = 0; last_host = None
    with (output / "samples.csv").open("w", encoding="utf-8",
            newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS, lineterminator="\n")
        writer.writeheader()
        while time.monotonic() - started < args.duration_s:
            delay = next_poll - time.monotonic()
            if delay > 0: time.sleep(delay)
            request_start = time.monotonic(); next_poll = request_start + args.poll_s
            try: state = read_state(client)
            except Exception:
                errors += 1
                if errors > args.max_errors: raise
                continue
            if state["sample_sequence"] == last_sequence:
                duplicates += 1; continue
            now = time.monotonic()
            if last_host is not None: maximum_gap = max(maximum_gap, now - last_host)
            last_host = now; last_sequence = state["sample_sequence"]
            first_sequence = state["sample_sequence"] if first_sequence is None else first_sequence
            final_sequence = state["sample_sequence"]; final = state
            writer.writerow({field: state[field] for field in FIELDS}); records += 1
            if records % 10 == 0: stream.flush()
            if records % 100 == 0:
                atomic_json(output / "summary.json", {"status": "RUNNING",
                    "records": records, "duplicates": duplicates,
                    "read_errors": errors, "maximum_host_gap_s": maximum_gap,
                    "last_state": final})
    expected = ((final_sequence - first_sequence) & 0xFFFFFFFF) + 1
    result = {"status": "COMPLETE", "duration_s": time.monotonic() - started,
        "records": records, "expected_records": expected,
        "coverage": records / expected, "first_sequence": first_sequence,
        "final_sequence": final_sequence, "duplicates": duplicates,
        "read_errors": errors, "maximum_host_gap_s": maximum_gap,
        "last_state": final, "writes": 0, "flash_operations": 0}
    atomic_json(output / "summary.json", result); return result


def main():
    parser = argparse.ArgumentParser(); parser.add_argument("--port", required=True)
    parser.add_argument("--output", required=True); parser.add_argument("--baud",
        type=int, default=115200); parser.add_argument("--timeout-ms", type=int,
        default=300); sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("probe")
    limits = sub.add_parser("set-limits"); limits.add_argument("--low-ug",
        type=int, required=True); limits.add_argument("--high-ug", type=int,
        required=True); limits.add_argument("--allow-control", action="store_true")
    capture = sub.add_parser("record"); capture.add_argument("--duration-s",
        type=float, required=True); capture.add_argument("--poll-s", type=float,
        default=0.09); capture.add_argument("--max-errors", type=int, default=10)
    args = parser.parse_args()
    with SerialTransport(args.port, args.baud, "N", 1, args.timeout_ms) as transport:
        client = ModbusClient(transport, 1)
        if args.command == "probe": result = {"state": read_state(client),
            "writes": 0, "flash_operations": 0}
        elif args.command == "set-limits":
            if not args.allow_control: raise HardwareTestError(
                "set-limits requires --allow-control")
            before = read_state(client); token = int(time.time() * 1000) & 0xFFFF or 1
            response = execute_command(client, token, COMMAND_SET_LIMITS,
                arg0=args.low_ug, arg1=args.high_ug); after = read_state(client)
            result = {"before": before, "response": response, "after": after,
                "writes": 1, "flash_operations": 0}
        else:
            result = record(args, client); return 0
        Path(args.output).write_text(json.dumps(result, indent=2) + "\n",
            encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
