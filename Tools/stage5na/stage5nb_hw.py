#!/usr/bin/env python3
"""Stage 5N-B engineering-only guarded ACTIVE monitor and controls."""

import argparse
import csv
import json
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools" / "stage5b_hw"))
sys.path.insert(0, str(ROOT / "Tools" / "stage5mr5b_beta"))
from hw_common import HardwareTestError, ModbusClient, execute_command
from serial_transport import SerialTransport
from stage5na2_hw import decode_display, decode_primary, read_static_context
from stage5na_hw import decode as decode_shadow
from r5_beta_hw import decode_beta

FIRST = 0x02C0
COUNT = 16
COMMAND_SET_MODE = 34
COMMAND_GET_STATUS = 35
MODES = {"off": 0, "static": 1, "dynamic": 2}


def decode(words):
    if len(words) != COUNT or words[0] != 0x5BB5:
        raise HardwareTestError("Stage 5N-B diagnostic signature mismatch")
    return {"signature": "0x%04X" % words[0], "mode": words[1],
        "generation": (words[2] << 16) | words[3], "reason": words[4],
        "formal_state": words[5], "armed": int(bool(words[6] & 1)),
        "green": int(bool(words[6] & 0x10)),
        "yellow": int(bool(words[6] & 0x20)),
        "red": int(bool(words[6] & 0x40)),
        "internal_buzzer": int(bool(words[6] & 0x80)),
        "external_buzzer": int(bool(words[6] & 0x100))}


def read_state(client):
    primary, _ = client.read(0, 0x20)
    words, _ = client.read(FIRST, COUNT)
    value = decode(words)
    value.update({"firmware": "0x%04X" % primary[15],
                  "map": "0x%04X" % primary[14],
                  "utc": time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime()) +
                    ".%03dZ" % int((time.time() % 1) * 1000)})
    if primary[15] != 0x0516 or primary[14] != 0x0104:
        raise HardwareTestError("expected firmware 0x0516 / Map 0x0104")
    return value


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--baud", type=int, default=115200)
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("probe")
    control = sub.add_parser("set-mode")
    control.add_argument("--mode", choices=sorted(MODES), required=True)
    control.add_argument("--allow-control", action="store_true")
    record = sub.add_parser("record")
    record.add_argument("--duration-s", type=float, required=True)
    record.add_argument("--poll-s", type=float, default=0.0)
    args = parser.parse_args()
    with SerialTransport(args.port, args.baud, "N", 1, 300) as transport:
        client = ModbusClient(transport, 1)
        before = read_state(client)
        if args.command == "probe":
            result = {"state": before, "writes": 0, "flash_operations": 0}
        elif args.command == "set-mode":
            if not args.allow_control:
                raise HardwareTestError("set-mode requires --allow-control")
            token = int(time.time() * 1000) & 0xFFFF or 1
            response = execute_command(client, token, COMMAND_SET_MODE,
                arg0=MODES[args.mode], arg1=before["generation"], flags=1)
            result = {"before": before, "response": response,
                      "after": read_state(client), "writes": 1,
                      "flash_operations": 0, "save_operations": 0}
        else:
            args.output.mkdir(parents=True, exist_ok=False)
            order = "low" if client.read(0x0103, 1)[0][0] else "high"
            context = read_static_context(client, order)
            started = time.monotonic(); rows = []; last_sequence = None
            while time.monotonic() - started < args.duration_s:
                cycle = time.monotonic()
                primary = decode_primary(client.read(0, 64)[0], order, 0x0516)
                shadow = decode_shadow(client.read(0x02E0, 31)[0], order)
                guarded = decode(client.read(FIRST, COUNT)[0])
                beta = decode_beta(client.read(0x0280, 40)[0], order)
                display = decode_display(client.read(0x01E0, 17)[0], order)
                if shadow["sample_sequence"] != last_sequence:
                    row = {"utc": time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime()) +
                        ".%03dZ" % int((time.time() % 1) * 1000),
                        "host_monotonic_ns": time.monotonic_ns(), **primary,
                        **display,
                        "shadow_static_class": shadow["static_class"],
                        "shadow_static_reason": shadow["static_reason"],
                        "shadow_dynamic_class": shadow["dynamic_confirmed"],
                        "shadow_dynamic_reason": shadow["dynamic_reason"],
                        "shadow_valid": shadow["valid"],
                        "low_limit_ug": shadow["low_limit_ug"],
                        "high_limit_ug": shadow["high_limit_ug"],
                        "guarded_mode": guarded["mode"],
                        "guarded_generation": guarded["generation"],
                        "guarded_reason": guarded["reason"],
                        "formal_state": guarded["formal_state"],
                        "guarded_armed": guarded["armed"],
                        "green": guarded["green"], "yellow": guarded["yellow"],
                        "red": guarded["red"],
                        "internal_buzzer": guarded["internal_buzzer"],
                        "external_buzzer": guarded["external_buzzer"],
                        "r5_application": beta["application"],
                        "r5_mode": beta["mode"], "r5_state": beta["state"],
                        "r5_offset_ug": beta["offset_ug"],
                        "r5_reference_ug": beta["reference_ug"],
                        "r5_rebase": beta["automatic_rebase_count"]}
                    rows.append(row); last_sequence = shadow["sample_sequence"]
                delay = args.poll_s - (time.monotonic() - cycle)
                if delay > 0: time.sleep(delay)
            with (args.output / "samples.csv").open("w", encoding="utf-8",
                    newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=list(rows[0]),
                                        lineterminator="\n")
                writer.writeheader(); writer.writerows(rows)
            result = {"records": len(rows), "duration_s": time.monotonic()-started,
                "context": context, "first": rows[0], "last": rows[-1],
                "read_errors": 0, "flash_operations": 0, "save_operations": 0}
            (args.output / "summary.json").write_bytes(
                (json.dumps(result, indent=2)+"\n").encode("utf-8"))
            print(json.dumps(result, indent=2)); return 0
        args.output.write_bytes((json.dumps(result, indent=2) + "\n").encode("utf-8"))
        print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
