#!/usr/bin/env python3
"""SWD-only controller for the Stage 5N-A3 diagnostic RAM block."""

import argparse
import hashlib
import json
import struct
import subprocess
import time
from pathlib import Path

MAGIC = 0x354E4133
VERSION = 1
COMMAND_MAGIC = 0x534E4133
CONTROL_WORDS = 23
CONTROL_SIZE = CONTROL_WORDS * 4
SYMBOL = "g_stage5na3_fault_control"
COMMANDS = {"invalid": 1, "abort": 2}
CONTROL_NAMES = (
    "magic", "version", "length", "request_sequence", "applied_sequence",
    "command_magic", "command", "requested_duration_ms", "status",
    "injection_type", "active", "start_ms", "remaining_ms",
    "completion_reason", "accepted_count", "rejected_count",
    "injected_sample_count", "natural_invalid_count", "last_input_valid",
    "last_static_class", "last_dynamic_class", "last_sample_sequence",
    "last_timestamp_ms")


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def write_json(path, value):
    Path(path).write_bytes((json.dumps(value, indent=2) + "\n").encode("utf-8"))


def symbol_from_elf(elf, nm):
    output = subprocess.check_output(
        [nm, "-S", "--defined-only", str(elf)], text=True)
    for line in output.splitlines():
        parts = line.split()
        if len(parts) >= 4 and parts[3] == SYMBOL:
            value = {"address": int(parts[0], 16),
                     "size": int(parts[1], 16)}
            if value["size"] != CONTROL_SIZE:
                raise ValueError("control symbol size mismatch")
            return value
    raise ValueError("Stage 5N-A3 control symbol missing")


def decode_control(data):
    if len(data) != CONTROL_SIZE:
        raise ValueError("truncated control block")
    value = dict(zip(CONTROL_NAMES,
                     struct.unpack("<%dI" % CONTROL_WORDS, data)))
    if (value["magic"] != MAGIC or value["version"] != VERSION or
            value["length"] != CONTROL_SIZE):
        raise ValueError("control magic/version/length mismatch")
    return value


def programmer_call(programmer, serial_number, frequency_khz, arguments):
    command = [programmer, "-c", "port=SWD", "mode=HotPlug",
               "freq=%d" % frequency_khz, "sn=%s" % serial_number] + arguments
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    if result.returncode != 0:
        raise RuntimeError("STM32CubeProgrammer failed:\n" + result.stdout)
    return {"command": command, "output": result.stdout}


def upload(args, address, output):
    return programmer_call(args.programmer, args.sn, args.swd_khz,
        ["-u", "0x%08X" % address, str(CONTROL_SIZE), str(Path(output).resolve())])


def write32(args, address, value):
    return programmer_call(args.programmer, args.sn, args.swd_khz,
        ["-w32", "0x%08X" % address, "0x%08X" % value, "-v"])


def inspect(args):
    symbol = symbol_from_elf(args.elf, args.nm)
    value = {"elf": {"path": str(args.elf), "length": args.elf.stat().st_size,
                      "sha256": sha256(args.elf)},
             "map": {"path": str(args.map), "length": args.map.stat().st_size,
                      "sha256": sha256(args.map)},
             "symbol": symbol, "control_magic": "0x%08X" % MAGIC,
             "control_version": VERSION, "control_size": CONTROL_SIZE}
    write_json(args.output, value)
    print(json.dumps(value, indent=2))
    return 0


def control(args):
    output = args.output
    output.mkdir(parents=True, exist_ok=False)
    symbol = symbol_from_elf(args.elf, args.nm)
    before_path = output / "control_before.bin"
    log = [upload(args, symbol["address"], before_path)]
    before = decode_control(before_path.read_bytes())
    sequence = (before["request_sequence"] + 1) & 0xFFFFFFFF
    if sequence == 0:
        sequence = 1
    duration = args.duration_ms if args.action == "invalid" else 0
    for offset, value in ((28, duration), (24, COMMANDS[args.action]),
                          (20, COMMAND_MAGIC), (12, sequence)):
        log.append(write32(args, symbol["address"] + offset, value))
    if args.action == "invalid" and args.autonomous_restore:
        time.sleep(duration / 1000.0 + args.autonomous_margin_s)
    deadline = time.monotonic() + args.timeout_s
    poll_path = output / "control_poll.bin"
    observed = []
    while time.monotonic() < deadline:
        time.sleep(args.poll_interval_s)
        log.append(upload(args, symbol["address"], poll_path))
        current = decode_control(poll_path.read_bytes())
        observed.append(current)
        if current["applied_sequence"] == sequence:
            if args.action == "abort" or current["status"] in (2, 3, 4):
                break
            if args.action == "invalid" and current["active"] == 1:
                continue
    else:
        raise TimeoutError("diagnostic command did not reach a terminal state")
    after_path = output / "control_after.bin"
    after_path.write_bytes(poll_path.read_bytes())
    result = {"action": args.action, "duration_ms": duration,
              "sequence": sequence, "symbol": symbol, "before": before,
              "observations": observed, "after": observed[-1],
              "operations": log}
    write_json(output / "result.json", result)
    print(json.dumps(result["after"], indent=2))
    return 0


def parser():
    result = argparse.ArgumentParser()
    sub = result.add_subparsers(dest="command", required=True)
    inspect_parser = sub.add_parser("inspect-elf")
    inspect_parser.add_argument("--elf", type=Path, required=True)
    inspect_parser.add_argument("--map", type=Path, required=True)
    inspect_parser.add_argument("--output", type=Path, required=True)
    inspect_parser.add_argument("--nm", default="arm-none-eabi-nm")
    decode = sub.add_parser("decode")
    decode.add_argument("--input", type=Path, required=True)
    decode.add_argument("--output", type=Path, required=True)
    for action in ("invalid", "abort"):
        command = sub.add_parser(action)
        command.add_argument("--elf", type=Path, required=True)
        command.add_argument("--output", type=Path, required=True)
        command.add_argument("--sn", required=True)
        command.add_argument("--programmer", required=True)
        command.add_argument("--swd-khz", type=int, default=1800)
        command.add_argument("--duration-ms", type=int, default=1000)
        command.add_argument("--poll-interval-s", type=float, default=0.1)
        command.add_argument("--timeout-s", type=float, default=10.0)
        command.add_argument("--autonomous-restore", action="store_true")
        command.add_argument("--autonomous-margin-s", type=float, default=0.5)
        command.add_argument("--nm", default="arm-none-eabi-nm")
        command.set_defaults(action=action)
    return result


def main():
    args = parser().parse_args()
    if args.command == "inspect-elf":
        return inspect(args)
    if args.command == "decode":
        value = decode_control(args.input.read_bytes())
        write_json(args.output, value)
        print(json.dumps(value, indent=2))
        return 0
    return control(args)


if __name__ == "__main__":
    raise SystemExit(main())
