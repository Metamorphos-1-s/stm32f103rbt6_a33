#!/usr/bin/env python3
import argparse
import csv
import json
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools" / "stage5b_hw"))
from hw_common import ModbusClient, HardwareTestError, execute_command
from serial_transport import SerialTransport
import modbus_frame as frame

BETA_FIRST = 0x0280
BETA_COUNT = 40
COMMAND_SET_MODE = 29
COMMAND_RESET = 30
COMMAND_GET_STATUS = 31
COMMAND_SET_APPLICATION = 32
MODES = {"off": 0, "dosing": 1, "static": 2}
APPLICATIONS = {"shadow": 0, "active": 1}


def i32(words, order="high"):
    return frame.decode_i32_words(words, order)


def i64(words, order="high"):
    return frame.decode_i64_words(words, order)


def decode_beta(words, order="high"):
    if len(words) != BETA_COUNT or words[0] != 0x55B5:
        raise HardwareTestError("R5 Beta diagnostic signature mismatch")
    return {
        "signature": "0x%04X" % words[0],
        "application": words[1], "mode": words[2], "state": words[3],
        "limited": words[4], "reason": words[5],
        "offset_ug": i64(words[6:10], order),
        "uncompensated_gross_ug": i64(words[10:14], order),
        "corrected_gross_ug": i64(words[14:18], order),
        "reference_ug": i64(words[18:22], order),
        "current_window_ug": i64(words[22:26], order),
        "reference_error_ug": i64(words[26:30], order),
        "correction_rate_milli_ug_per_s": i32(words[30:32], order),
        "holdoff_remaining": words[32], "reference_fill": words[33],
        "observation_fill": words[34],
        "automatic_rebase_count": ((words[35] << 16) | words[36]),
        "evaluation_count": ((words[37] << 16) | words[38]),
        "reserved": words[39],
    }


def read_state(client):
    realtime, _ = client.read(0, 0x20)
    diag, _ = client.read(0x20, 0x1B)
    storage, _ = client.read(0x1C0, 10)
    order = "low" if client.read(0x103, 1)[0][0] else "high"
    beta, _ = client.read(BETA_FIRST, BETA_COUNT)
    if realtime[14] != 0x0104 or realtime[15] != 0x0511:
        raise HardwareTestError("expected Beta identity Map 0x0104 / Firmware 0x0511")
    return {
        "utc": time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime()) +
               ".%03dZ" % int((time.time() % 1) * 1000),
        "firmware": "0x%04X" % realtime[15],
        "map": "0x%04X" % realtime[14],
        "display_count": i32(realtime[0:2], order),
        "display_decimals": realtime[2],
        "status_flags": realtime[4] | (realtime[5] << 16),
        "net_mass_ug": i64(realtime[16:20], order),
        "gross_mass_ug": i64(realtime[20:24], order),
        "tare_mass_ug": i64(realtime[24:28], order),
        "sample_sequence": (diag[0] << 16) | diag[1],
        "uptime_ms": (diag[2] << 16) | diag[3],
        "profile": diag[8], "sample_rate": diag[9], "gain": diag[10],
        "overrun_count": (diag[13] << 16) | diag[14],
        "storage_state": diag[16], "power_safe": diag[17],
        "dirty": diag[18],
        "revision": (diag[19] << 16) | diag[20],
        "saved_revision": (diag[21] << 16) | diag[22],
        "fault_mask": (diag[25] << 16) | diag[26],
        "persistent_format": storage[0], "active_slot": storage[1],
        "storage_sequence": (storage[2] << 16) | storage[3],
        "word_order": order,
        **decode_beta(beta, order),
    }


def write_json(path, value):
    Path(path).write_bytes((json.dumps(value, indent=2) + "\n").encode("utf-8"))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--parity", default="N")
    parser.add_argument("--stopbits", type=int, default=1)
    parser.add_argument("--slave", type=int, default=1)
    parser.add_argument("--timeout-ms", type=int, default=300)
    parser.add_argument("--output", required=True)
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("probe")
    command = sub.add_parser("control")
    group = command.add_mutually_exclusive_group(required=True)
    group.add_argument("--mode", choices=sorted(MODES))
    group.add_argument("--application", choices=sorted(APPLICATIONS))
    group.add_argument("--reset", action="store_true")
    command.add_argument("--allow-control", action="store_true")
    command.add_argument("--allow-active", action="store_true")
    capture = sub.add_parser("capture")
    capture.add_argument("--duration-s", type=float, required=True)
    args = parser.parse_args()
    frames = []
    def log(line): frames.append({"monotonic_ns": time.monotonic_ns(), "frame": line})
    with SerialTransport(args.port, args.baud, args.parity, args.stopbits,
                         args.timeout_ms, frame_logger=log) as transport:
        client = ModbusClient(transport, args.slave)
        started = time.time()
        if args.command == "probe": result = {"state": read_state(client)}
        elif args.command == "control":
            if not args.allow_control:
                raise HardwareTestError("control requires --allow-control")
            before = read_state(client)
            token = int(time.time() * 1000) & 0xFFFF or 1
            if args.mode is not None:
                response = execute_command(client, token, COMMAND_SET_MODE,
                                           arg0=MODES[args.mode])
            elif args.application is not None:
                if args.application == "active" and not args.allow_active:
                    raise HardwareTestError("ACTIVE requires --allow-active")
                response = execute_command(client, token,
                    COMMAND_SET_APPLICATION, arg0=APPLICATIONS[args.application])
            else: response = execute_command(client, token, COMMAND_RESET)
            after = read_state(client)
            result = {"before": before, "response": response, "after": after}
        else:
            rows = []
            deadline = time.time() + args.duration_s
            while time.time() < deadline:
                rows.append(read_state(client))
                delay = started + len(rows) - time.time()
                if delay > 0: time.sleep(delay)
            result = {"duration_s": time.time() - started, "records": len(rows),
                      "rows": rows}
        result["started_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ",
                                              time.gmtime(started))
        result["ended_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ",
                                            time.gmtime())
        result["writes"] = 0 if args.command in ("probe", "capture") else 1
        result["flash_operations"] = 0
        result["frames"] = frames
        write_json(args.output, result)
    return 0


if __name__ == "__main__": raise SystemExit(main())
