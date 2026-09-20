#!/usr/bin/env python3
import argparse
import csv
import hashlib
import json
import os
import platform
import subprocess
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
EXPECTED_FIRMWARE = 0x0515


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
        "save_request_count_low": words[39],
    }


def read_state(client):
    realtime, _ = client.read(0, 0x20)
    diag, _ = client.read(0x20, 0x1B)
    storage, _ = client.read(0x1C0, 10)
    order = "low" if client.read(0x103, 1)[0][0] else "high"
    beta, _ = client.read(BETA_FIRST, BETA_COUNT)
    if realtime[14] != 0x0104 or realtime[15] != EXPECTED_FIRMWARE:
        raise HardwareTestError(
            "expected Stage 5N-A Beta identity Map 0x0104 / Firmware 0x0515")
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


def atomic_json(path, value):
    path = Path(path)
    temporary = path.with_suffix(path.suffix + ".tmp")
    write_json(temporary, value)
    os.replace(str(temporary), str(path))


def append_jsonl(path, value):
    with Path(path).open("ab") as stream:
        stream.write((json.dumps(value, separators=(",", ":")) + "\n").encode("utf-8"))


def utc_now():
    return time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime()) + \
           ".%03dZ" % int((time.time() % 1) * 1000)


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def environment(args):
    tool = Path(__file__).resolve()
    try:
        head = subprocess.check_output(["git", "rev-parse", "HEAD"],
            cwd=ROOT, text=True).strip()
    except (OSError, subprocess.CalledProcessError):
        head = "UNAVAILABLE"
    return {"repository_head": head, "python": sys.version,
        "platform": platform.platform(), "tool_path": tool.relative_to(ROOT).as_posix(),
        "tool_length": tool.stat().st_size, "tool_sha256": sha256(tool),
        "port": args.port, "baud": args.baud, "parity": args.parity,
        "stopbits": args.stopbits, "slave": args.slave,
        "poll_interval_s": args.poll_interval_s,
        "algorithm": "R5_REFERENCE_LOCK_10S_BLOCK_MEDIAN_BETA",
        "writes": 0, "flash_operations": 0}


def record(args):
    output = Path(args.output)
    output.mkdir(parents=True, exist_ok=False)
    samples_path = output / "samples.csv"
    events_path = output / "events.jsonl"
    frames_path = output / "frames.jsonl"
    atomic_json(output / "environment.json", environment(args))
    started = time.time(); deadline = started + args.duration_s
    records = retries = unobserved = poll_gaps = errors = 0
    last_sequence = last_mode = last_state = last_application = None
    last_poll_monotonic = None
    first_state = last_state_row = None
    interrupted = False
    transport = None

    def frame_log(line):
        append_jsonl(frames_path, {"utc": utc_now(),
            "monotonic_ns": time.monotonic_ns(), "frame": line})

    def open_client():
        current = SerialTransport(args.port, args.baud, args.parity,
            args.stopbits, args.timeout_ms, frame_logger=frame_log)
        return current, ModbusClient(current, args.slave)

    try:
        transport, client = open_client()
        with samples_path.open("w", newline="", encoding="utf-8") as stream:
            writer = None
            next_poll = time.monotonic()
            while time.time() < deadline:
                try:
                    row = read_state(client)
                except Exception as exc:
                    errors += 1
                    append_jsonl(events_path, {"utc": utc_now(),
                        "event": "READ_ERROR", "error": str(exc),
                        "retry": retries + 1})
                    if transport is not None: transport.close()
                    if retries >= args.max_retries: raise
                    retries += 1
                    time.sleep(args.retry_delay_s)
                    transport, client = open_client()
                    next_poll = time.monotonic()
                    continue
                if writer is None:
                    writer = csv.DictWriter(stream, fieldnames=list(row),
                                            lineterminator="\n")
                    writer.writeheader()
                    first_state = row
                sequence = int(row["sample_sequence"])
                poll_now = time.monotonic()
                if last_poll_monotonic is not None and \
                   (poll_now - last_poll_monotonic) > args.poll_interval_s * 1.5:
                    poll_gaps += 1
                    append_jsonl(events_path, {"utc": row["utc"],
                        "event": "HOST_POLL_GAP",
                        "elapsed_s": poll_now - last_poll_monotonic})
                if last_sequence is not None:
                    delta = (sequence - last_sequence) & 0xFFFFFFFF
                    if delta > 1 and delta < 0x80000000:
                        unobserved += delta - 1
                        append_jsonl(events_path, {"utc": row["utc"],
                            "event": "DEVICE_SEQUENCE_ADVANCE",
                            "unobserved_sequences": delta - 1})
                for name, previous, current in (
                    ("MODE_CHANGE", last_mode, row["mode"]),
                    ("STATE_CHANGE", last_state, row["state"]),
                    ("APPLICATION_CHANGE", last_application, row["application"])):
                    if previous is not None and previous != current:
                        append_jsonl(events_path, {"utc": row["utc"],
                            "event": name, "before": previous, "after": current,
                            "offset_ug": row["offset_ug"]})
                writer.writerow(row); stream.flush(); records += 1
                last_sequence = sequence; last_mode = row["mode"]
                last_poll_monotonic = poll_now
                last_state = row["state"]; last_application = row["application"]
                last_state_row = row
                summary = {"status": "RUNNING", "started_utc": first_state["utc"],
                    "last_utc": row["utc"], "duration_s": time.time() - started,
                    "records": records, "read_errors": errors,
                    "reconnect_retries": retries,
                    "unobserved_device_sequences": unobserved,
                    "host_poll_gap_count": poll_gaps, "last_state": row,
                    "writes": 0, "flash_operations": 0}
                atomic_json(output / "summary.json", summary)
                next_poll += args.poll_interval_s
                delay = next_poll - time.monotonic()
                if delay > 0: time.sleep(delay)
                else: next_poll = time.monotonic()
    except KeyboardInterrupt:
        interrupted = True
        append_jsonl(events_path, {"utc": utc_now(), "event": "HOST_INTERRUPT"})
    finally:
        if transport is not None: transport.close()
        ended = time.time()
        summary = {"status": "INTERRUPTED" if interrupted else "COMPLETE",
            "started_utc": None if first_state is None else first_state["utc"],
            "ended_utc": utc_now(), "duration_s": ended - started,
            "records": records, "read_errors": errors,
            "reconnect_retries": retries,
            "unobserved_device_sequences": unobserved,
            "host_poll_gap_count": poll_gaps,
            "first_state": first_state, "last_state": last_state_row,
            "samples_length": samples_path.stat().st_size if samples_path.exists() else 0,
            "samples_sha256": sha256(samples_path) if samples_path.exists() else None,
            "events_length": events_path.stat().st_size if events_path.exists() else 0,
            "frames_length": frames_path.stat().st_size if frames_path.exists() else 0,
            "writes": 0, "flash_operations": 0}
        atomic_json(output / "summary.json", summary)
    return 130 if interrupted else 0


def main():
    global EXPECTED_FIRMWARE
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--parity", default="N")
    parser.add_argument("--stopbits", type=int, default=1)
    parser.add_argument("--slave", type=int, default=1)
    parser.add_argument("--timeout-ms", type=int, default=300)
    parser.add_argument("--expected-firmware", type=lambda value: int(value, 0),
                        default=EXPECTED_FIRMWARE)
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
    recorder = sub.add_parser("record")
    recorder.add_argument("--duration-s", type=float, required=True)
    recorder.add_argument("--poll-interval-s", type=float, default=1.0)
    recorder.add_argument("--max-retries", type=int, default=10)
    recorder.add_argument("--retry-delay-s", type=float, default=1.0)
    args = parser.parse_args()
    EXPECTED_FIRMWARE = args.expected_firmware
    if args.command == "record":
        return record(args)
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
