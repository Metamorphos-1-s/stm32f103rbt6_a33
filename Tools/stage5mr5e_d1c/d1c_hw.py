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
from hw_common import HardwareTestError, ModbusClient
from serial_transport import SerialTransport
from r5_beta_hw import i32, i64


D1C_FIRST = 0x02A8
D1C_COUNT = 51
FIELDS = ("utc", "uptime_ms", "sample_sequence", "follower_sequence",
    "firmware", "map", "authoritative_display_input_ug", "panel_display_count",
    "desired_count", "display_count", "delta_count", "desired_division",
    "display_division", "delta_divisions", "anchor_division", "direction",
    "evidence", "anchor_count",
    "locked", "stable", "large_step", "release_reason", "source", "unit",
    "decimals", "division", "application", "mode", "state",
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
        "firmware": "0x%04X" % words[1], "map": "0x%04X" % words[2],
        "persistent_format": words[3],
        "sample_sequence": (words[4] << 16) | words[5],
        "follower_sequence": (words[4] << 16) | words[5],
        "uptime_ms": (words[6] << 16) | words[7],
        "authoritative_display_input_ug": i64(words[8:12], order),
        "desired_division": i32(words[12:14], order),
        "display_division": i32(words[14:16], order),
        "delta_divisions": i32(words[16:18], order),
        "evidence": signed8(words[18] >> 8),
        "direction": signed8(words[18] & 0xFF),
        "anchor_division": i32(words[19:21], order),
        "initialized": int(bool(words[21] & 1)),
        "locked": int(bool(words[21] & 2)),
        "large_step": int(bool(words[21] & 4)),
        "stable": int(bool(words[21] & 8)),
        "active": int(bool(words[21] & 16)),
        "limited": int(bool(words[21] & 32)),
        "release_reason": words[22] >> 8, "source": words[22] & 0xFF,
        "unit": words[23] >> 8, "decimals": words[23] & 0xFF,
        "division": words[24], "application": words[25] >> 12,
        "mode": (words[25] >> 8) & 0x0F, "state": words[25] & 0xFF,
        "uncompensated_gross_ug": i64(words[26:30], order),
        "corrected_gross_ug": i64(words[30:34], order),
        "offset_ug": i64(words[34:38], order),
        "reference_ug": i64(words[38:42], order),
        "automatic_rebase_count": (words[42] << 16) | words[43],
        "fault_mask": (words[44] << 16) | words[45],
        "overrun_count": (words[46] << 16) | words[47],
        "dirty": int(bool(words[48] & 0x8000)),
        "save_request_count_low": words[48] & 0x7FFF,
        "revision": words[49], "saved_revision": words[50]}


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
    state["authoritative_display_input_ug"] = state["net_mass_ug"]
    return state


def record(args):
    output = Path(args.output); output.mkdir(parents=True, exist_ok=False)
    samples_path = output / "samples.csv"
    frames_path = output / "frames.jsonl"
    started = time.monotonic(); last_sequence = None; last_utc = None
    records = duplicates = errors = reconnects = maximum_gap = 0
    first_sequence = final_sequence = None; final = None
    def frame_log(line):
        with frames_path.open("a", encoding="utf-8", newline="") as stream:
            stream.write(json.dumps({"monotonic_ns": time.monotonic_ns(),
                "frame": line}, separators=(",", ":")) + "\n")
    transport = SerialTransport(args.port, args.baud, args.parity,
        args.stopbits, args.timeout_ms, frame_logger=frame_log)
    client = ModbusClient(transport, args.slave)
    try:
        with samples_path.open("w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=FIELDS,
                lineterminator="\n")
            writer.writeheader()
            while time.monotonic() - started < args.duration_s:
                try:
                    state = combined_state(client)
                except Exception:
                    errors += 1
                    if errors > args.max_errors:
                        raise
                    continue
                sequence = state["sample_sequence"]
                if sequence == last_sequence:
                    duplicates += 1
                    continue
                now = time.monotonic()
                if last_utc is not None:
                    maximum_gap = max(maximum_gap, now - last_utc)
                last_utc = now; last_sequence = sequence
                first_sequence = sequence if first_sequence is None else first_sequence
                final_sequence = sequence; final = state
                writer.writerow({field: state.get(field, 0) for field in FIELDS})
                stream.flush(); records += 1
                if records % 10 == 0:
                    atomic_json(output / "summary.json", {
                        "status": "RUNNING", "records": records,
                        "first_sequence": first_sequence,
                        "final_sequence": final_sequence,
                        "duplicates_skipped": duplicates,
                        "read_errors": errors, "reconnects": reconnects,
                        "maximum_accepted_gap_s": maximum_gap,
                        "last_state": final})
    finally:
        transport.close()
    expected = (None if first_sequence is None else
        ((final_sequence - first_sequence) & 0xFFFFFFFF) + 1)
    summary = {"status": "COMPLETE", "duration_s": time.monotonic() - started,
        "records": records, "expected_sequence_records": expected,
        "coverage": None if not expected else records / expected,
        "first_sequence": first_sequence, "final_sequence": final_sequence,
        "duplicates_skipped": duplicates, "read_errors": errors,
        "reconnects": reconnects, "maximum_accepted_gap_s": maximum_gap,
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
    return record(parser.parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
