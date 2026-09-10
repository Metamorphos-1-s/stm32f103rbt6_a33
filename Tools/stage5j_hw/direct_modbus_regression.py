#!/usr/bin/env python3
"""Run a bounded, non-persistent Modbus RTU regression and restore staging."""

import argparse
import json
import sys
import time
from pathlib import Path


STAGE5B = Path(__file__).resolve().parents[1] / "stage5b_hw"
sys.path.insert(0, str(STAGE5B))

import modbus_frame as frame  # noqa: E402
import register_map as registers  # noqa: E402
from hw_common import ModbusClient, probe_device  # noqa: E402
from serial_transport import SerialTransport  # noqa: E402


def _expect_exception(client, request, function, code):
    exchange = client.raw(request)
    parsed = frame.parse_response(exchange.rx, client.slave, function)
    if parsed.exception != code:
        raise RuntimeError("expected exception %02X, got %r" %
                           (code, parsed.exception))
    return exchange.rx.hex().upper()


def _release_staging_owner(client):
    before = client.read(0x004C, 4)[0]
    token = (before[0] + 1) & 0xFFFF
    if token == 0:
        token = 1
    client.write_multiple(0x0040,
                          [token, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xA55A])
    after = client.read(0x004C, 4)[0]
    if after[0] != token or after[2] != 0 or after[3] != 12:
        raise RuntimeError("CONFIG_CANCEL cleanup response mismatch")
    return {"token": token, "result": after[1], "state": after[2],
            "last_command": after[3]}


def run(args):
    if not args.allow_staging_writes:
        raise RuntimeError("--allow-staging-writes is required")
    result = {"schema": 1, "interface": args.interface, "port": args.port,
              "firmware": "0x050F", "register_map": "0x0104",
              "persistent_commands": 0, "apply_commands": 0,
              "passed": False, "checks": {}, "restored": False}
    transport = SerialTransport(args.port, 115200, "N", 1, args.timeout_ms)
    client = ModbusClient(transport, 1)
    original_single = None
    original_multiple = None
    try:
        transport.reset()
        identity = probe_device(client)
        if identity["firmware_version"] != 0x050F or \
                identity["register_map_version"] != 0x0104 or \
                identity["schema_version"] != 2:
            raise RuntimeError("target identity does not match 0x050F/0x0104/2")
        result["identity"] = identity
        for _ in range(args.fc03_count):
            client.read(0, 2)
        result["checks"]["fc03"] = {"passed": True,
                                           "count": args.fc03_count}

        original_single = client.read(0x017C, 1)[0][0]
        test_single = 0 if original_single else 1
        client.write_single(0x017C, test_single)
        if client.read(0x017C, 1)[0][0] != test_single:
            raise RuntimeError("FC06 staging readback mismatch")
        client.write_single(0x017C, original_single)
        if client.read(0x017C, 1)[0][0] != original_single:
            raise RuntimeError("FC06 staging restore mismatch")
        result["checks"]["fc06"] = {"passed": True, "address": "0x017C",
                                           "original": original_single,
                                           "test": test_single}

        original_multiple = client.read(0x0140, 2)[0]
        test_multiple = [1, 2] if original_multiple != [1, 2] else [2, 3]
        client.write_multiple(0x0140, test_multiple)
        if client.read(0x0140, 2)[0] != test_multiple:
            raise RuntimeError("FC16 staging readback mismatch")
        client.write_multiple(0x0140, original_multiple)
        if client.read(0x0140, 2)[0] != original_multiple:
            raise RuntimeError("FC16 staging restore mismatch")
        result["checks"]["fc16"] = {"passed": True, "address": "0x0140",
                                           "original": original_multiple,
                                           "test": test_multiple}

        result["checks"]["exception_01"] = _expect_exception(
            client, frame.build_request(1, 4, b"\x00\x00\x00\x01"), 4, 1)
        result["checks"]["exception_02"] = _expect_exception(
            client, frame.build_read(1, 0xFFFF, 1), 3, 2)
        result["checks"]["exception_03"] = _expect_exception(
            client, frame.build_write_single(1, registers.COMM_ADDRESS, 0), 6, 3)

        bad_crc = bytearray(frame.build_read(1, 0, 1))
        bad_crc[-1] ^= 1
        if client.raw(bad_crc, expect_response=False).rx:
            raise RuntimeError("bad CRC unexpectedly produced a response")
        client.read(0, 1)
        result["checks"]["bad_crc_silence_and_recovery"] = True
        result["restored"] = True
        result["passed"] = True
    except Exception as exc:
        result["error"] = "%s: %s" % (type(exc).__name__, exc)
    finally:
        restore_errors = []
        try:
            if original_single is not None and \
                    client.read(0x017C, 1)[0][0] != original_single:
                client.write_single(0x017C, original_single)
        except Exception as exc:
            restore_errors.append("FC06: %s" % exc)
        try:
            if original_multiple is not None and \
                    client.read(0x0140, 2)[0] != original_multiple:
                client.write_multiple(0x0140, original_multiple)
        except Exception as exc:
            restore_errors.append("FC16: %s" % exc)
        try:
            result["owner_cleanup"] = _release_staging_owner(client)
        except Exception as exc:
            restore_errors.append("owner cleanup: %s" % exc)
        transport.close()
        if restore_errors:
            result["restored"] = False
            result["passed"] = False
            result["restore_errors"] = restore_errors
    result["completed_at_epoch"] = int(time.time())
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="ascii")
    print(json.dumps(result, indent=2))
    return 0 if result["passed"] and result["restored"] else 1


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--interface", choices=("rs232", "rs485"), required=True)
    parser.add_argument("--fc03-count", type=int, default=100)
    parser.add_argument("--timeout-ms", type=int, default=800)
    parser.add_argument("--output", required=True)
    parser.add_argument("--allow-staging-writes", action="store_true")
    args = parser.parse_args()
    if args.fc03_count < 1:
        parser.error("--fc03-count must be positive")
    return run(args)


if __name__ == "__main__":
    raise SystemExit(main())
