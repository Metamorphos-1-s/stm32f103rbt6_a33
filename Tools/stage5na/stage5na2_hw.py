#!/usr/bin/env python3
"""Read-only combined recorder for Stage 5N-A2 physical qualification."""

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
sys.path.insert(0, str(Path(__file__).resolve().parent))

from hw_common import HardwareTestError, ModbusClient
from modbus_frame import decode_i32_words, decode_i64_words
from r5_beta_hw import BETA_COUNT, BETA_FIRST, decode_beta
from serial_transport import SerialTransport
from stage5na_hw import COUNT as SHADOW_COUNT
from stage5na_hw import FIRST as SHADOW_FIRST
from stage5na_hw import decode as decode_shadow


def utc_now():
    return time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime()) + \
        ".%03dZ" % int((time.time() % 1) * 1000)


def atomic_json(path, value):
    path = Path(path)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    os.replace(str(temporary), str(path))


def append_jsonl(path, value):
    with Path(path).open("a", encoding="utf-8", newline="\n") as stream:
        stream.write(json.dumps(value, separators=(",", ":")) + "\n")


def decode_primary(words, order, expected_firmware=0x0515):
    if (len(words) != 64 or words[14] != 0x0104 or
            words[15] != expected_firmware):
        raise HardwareTestError("unexpected Map or firmware identity")
    flags = words[4] | (words[5] << 16)
    return {
        "firmware": "0x%04X" % words[15],
        "map": "0x%04X" % words[14],
        "display_count": decode_i32_words(words[0:2], order),
        "display_decimals": words[2],
        "status_flags": flags,
        "net_ug": decode_i64_words(words[16:20], order),
        "gross_ug": decode_i64_words(words[20:24], order),
        "tare_ug": decode_i64_words(words[24:28], order),
        "raw_adc": decode_i32_words(words[28:30], order),
        "filtered_raw": decode_i32_words(words[30:32], order),
        "sample_sequence": (words[32] << 16) | words[33],
        "mcu_uptime_ms": (words[34] << 16) | words[35],
        "official_stable": int(bool(flags & (1 << 4))),
        "official_overload": int(bool(flags & (1 << 7))),
        "profile": words[40],
        "sample_rate": words[41],
        "gain": words[42],
        "cs1237_state": words[43],
        "buffered_samples": words[44],
        "overrun_count": (words[45] << 16) | words[46],
        "dirty": words[50],
        "revision": (words[51] << 16) | words[52],
        "saved_revision": (words[53] << 16) | words[54],
        "fault_mask": (words[57] << 16) | words[58],
        "calibration_valid": words[59],
    }


def decode_display(words, order):
    return {
        "display_condition_state": words[0],
        "display_locked": words[1],
        "conditioned_display_ug": decode_i64_words(words[2:6], order),
        "display_anchor_ug": decode_i64_words(words[6:10], order),
    }


def read_static_context(client, order):
    active, _ = client.read(0x0100, 64)
    calibration, _ = client.read(0x0190, 10)
    storage, _ = client.read(0x01C0, 10)
    profile = active[31]
    base = 32 if profile == 0 else 46
    return {
        "schema": active[62],
        "persistent_format": storage[0],
        "active_slot": storage[1],
        "storage_sequence": (storage[2] << 16) | storage[3],
        "profile": profile,
        "sample_rate": active[base],
        "gain": active[base + 1],
        "filter_mode": active[base + 2],
        "filter_strength": active[base + 3],
        "calibration": {
            "raw_zero": decode_i32_words(calibration[0:2], order),
            "raw_span": decode_i32_words(calibration[2:4], order),
            "span_mass_ug": decode_i64_words(calibration[4:8], order),
            "sequence": (calibration[8] << 16) | calibration[9],
        },
        "active_config": active,
    }


def record(args):
    output = Path(args.output)
    output.mkdir(parents=True, exist_ok=False)
    samples_path = output / "samples.csv"
    events_path = output / "events.jsonl"
    stop_path = output / "stop.request"
    started_wall = time.time()
    started_mono = time.monotonic()
    records = duplicates = read_errors = sequence_gaps = 0
    last_sequence = None
    maximum_host_gap_s = 0.0
    last_host = None
    final = first = None

    append_jsonl(events_path, {"utc": utc_now(), "event": "RECORDER_STARTED"})
    with SerialTransport(args.port, args.baud, "N", 1,
                         args.timeout_ms) as transport:
        client = ModbusClient(transport, 1)
        order = "low" if client.read(0x0103, 1)[0][0] else "high"
        context = read_static_context(client, order)
        atomic_json(output / "environment.json", {
            "started_utc": utc_now(), "port": args.port,
            "baud": args.baud, "word_order": order,
            "repository_head": os.popen("git rev-parse HEAD").read().strip(),
            "read_only": True, "writes": 0, "flash_operations": 0,
            **context,
        })
        primary = None
        display = {"display_condition_state": "", "display_locked": "",
                   "conditioned_display_ug": "", "display_anchor_ug": ""}
        r5 = {key: "" for key in ("application", "mode", "state", "limited",
              "reason", "offset_ug", "reference_ug", "evaluation_count",
              "automatic_rebase_count", "save_request_count_low")}
        next_primary = next_aux = 0.0
        with samples_path.open("w", encoding="utf-8", newline="") as stream:
            writer = None
            while time.monotonic() - started_mono < args.duration_s:
                if stop_path.exists():
                    break
                cycle = time.monotonic()
                try:
                    shadow = decode_shadow(
                        client.read(SHADOW_FIRST, SHADOW_COUNT)[0], order)
                    if primary is None or cycle >= next_primary:
                        primary = decode_primary(client.read(0, 64)[0], order)
                        next_primary = cycle + args.primary_interval_s
                    if cycle >= next_aux:
                        display = decode_display(client.read(0x01E0, 17)[0], order)
                        beta = decode_beta(client.read(BETA_FIRST, BETA_COUNT)[0], order)
                        r5 = {key: beta[key] for key in r5}
                        next_aux = cycle + args.aux_interval_s
                except Exception as exc:
                    read_errors += 1
                    append_jsonl(events_path, {"utc": utc_now(),
                        "event": "READ_ERROR", "error": str(exc)})
                    if read_errors > args.max_errors:
                        raise
                    continue
                sequence = shadow["sample_sequence"]
                if sequence == last_sequence:
                    duplicates += 1
                    continue
                if last_sequence is not None:
                    gap = (sequence - last_sequence - 1) & 0xFFFFFFFF
                    if gap < 0x80000000:
                        sequence_gaps += gap
                now = time.monotonic()
                if last_host is not None:
                    maximum_host_gap_s = max(maximum_host_gap_s, now - last_host)
                last_host = now
                last_sequence = sequence
                row = {"utc": utc_now(), "host_monotonic_ns": time.monotonic_ns(),
                       **primary, "primary_sample_sequence": primary["sample_sequence"],
                       **display, **r5, **shadow}
                if writer is None:
                    writer = csv.DictWriter(stream, fieldnames=list(row),
                                            lineterminator="\n")
                    writer.writeheader()
                    first = row
                writer.writerow(row)
                stream.flush()
                records += 1
                final = row
                delay = args.poll_s - (time.monotonic() - cycle)
                if delay > 0:
                    time.sleep(delay)
    ended_wall = time.time()
    summary = {
        "status": "COMPLETE", "started_utc": time.strftime(
            "%Y-%m-%dT%H:%M:%SZ", time.gmtime(started_wall)),
        "ended_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(ended_wall)),
        "duration_s": ended_wall - started_wall, "records": records,
        "duplicates": duplicates, "read_errors": read_errors,
        "unobserved_sample_sequences": sequence_gaps,
        "maximum_host_gap_s": maximum_host_gap_s,
        "first_state": first, "last_state": final,
        "writes": 0, "flash_operations": 0,
    }
    atomic_json(output / "summary.json", summary)
    if stop_path.exists():
        stop_path.unlink()
    append_jsonl(events_path, {"utc": utc_now(), "event": "RECORDER_STOPPED",
                               "records": records})
    print(json.dumps(summary, indent=2))
    return 0


def mark_event(args):
    output = Path(args.output)
    if not output.is_dir():
        raise HardwareTestError("recording directory does not exist")
    value = {"utc": utc_now(), "host_monotonic_ns": time.monotonic_ns(),
             "event": args.event}
    if args.note:
        value["note"] = args.note
    append_jsonl(output / "events.jsonl", value)
    print(json.dumps(value))
    return 0


def stop(args):
    output = Path(args.output)
    if not output.is_dir():
        raise HardwareTestError("recording directory does not exist")
    (output / "stop.request").touch(exist_ok=True)
    return 0


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    capture = sub.add_parser("record")
    capture.add_argument("--port", required=True)
    capture.add_argument("--output", required=True)
    capture.add_argument("--duration-s", type=float, default=1800.0)
    capture.add_argument("--poll-s", type=float, default=0.0)
    capture.add_argument("--primary-interval-s", type=float, default=0.25)
    capture.add_argument("--aux-interval-s", type=float, default=1.0)
    capture.add_argument("--baud", type=int, default=115200)
    capture.add_argument("--timeout-ms", type=int, default=300)
    capture.add_argument("--max-errors", type=int, default=10)
    marker = sub.add_parser("mark-event")
    marker.add_argument("--output", required=True)
    marker.add_argument("--event", required=True)
    marker.add_argument("--note")
    stopper = sub.add_parser("stop")
    stopper.add_argument("--output", required=True)
    args = parser.parse_args()
    if args.command == "record":
        return record(args)
    if args.command == "mark-event":
        return mark_event(args)
    return stop(args)


if __name__ == "__main__":
    raise SystemExit(main())
