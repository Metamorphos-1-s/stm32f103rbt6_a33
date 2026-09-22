#!/usr/bin/env python3
"""Apply the documented volatile Stage 5N-B hardware alarm test config."""

import argparse
import json
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools" / "stage5b_hw"))
from hw_common import HardwareTestError, ModbusClient, execute_command
from serial_transport import SerialTransport


def words_i64(value):
    bits = value & ((1 << 64) - 1)
    return [(bits >> shift) & 0xFFFF for shift in (48, 32, 16, 0)]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--allow-control", action="store_true")
    args = parser.parse_args()
    if not args.allow_control:
        raise HardwareTestError("volatile setup requires --allow-control")
    values = [1, 0] + words_i64(100000000) + words_i64(400000000) + \
        words_i64(20000) + [1, 1, 1]
    with SerialTransport(args.port, 115200, "N", 1, 300) as transport:
        client = ModbusClient(transport, 1)
        before, _ = client.read(0x0220, 28)
        client.write_multiple(0x0240, values)
        token = int(time.time() * 1000) & 0xFFFF or 1
        validation = execute_command(client, token, 10)
        if validation["result_name"] != "OK":
            raise HardwareTestError("alarm validation failed")
        application = execute_command(client, (token + 1) & 0xFFFF or 1, 11)
        if application["result_name"] != "OK":
            raise HardwareTestError("alarm RAM application failed")
        after, _ = client.read(0x0220, 28)
    result = {"before": before, "staging": values,
        "validation": validation, "application": application, "after": after,
        "persistent_write": False, "save_sent": False, "flash_operations": 0}
    args.output.write_bytes((json.dumps(result, indent=2) + "\n").encode("utf-8"))
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
