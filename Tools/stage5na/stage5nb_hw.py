#!/usr/bin/env python3
"""Stage 5N-B engineering-only guarded ACTIVE monitor and controls."""

import argparse
import json
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools" / "stage5b_hw"))
from hw_common import HardwareTestError, ModbusClient, execute_command
from serial_transport import SerialTransport

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
    args = parser.parse_args()
    with SerialTransport(args.port, args.baud, "N", 1, 300) as transport:
        client = ModbusClient(transport, 1)
        before = read_state(client)
        if args.command == "probe":
            result = {"state": before, "writes": 0, "flash_operations": 0}
        else:
            if not args.allow_control:
                raise HardwareTestError("set-mode requires --allow-control")
            token = int(time.time() * 1000) & 0xFFFF or 1
            response = execute_command(client, token, COMMAND_SET_MODE,
                arg0=MODES[args.mode], arg1=before["generation"], flags=1)
            result = {"before": before, "response": response,
                      "after": read_state(client), "writes": 1,
                      "flash_operations": 0, "save_operations": 0}
        args.output.write_bytes((json.dumps(result, indent=2) + "\n").encode("utf-8"))
        print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
