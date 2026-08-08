#!/usr/bin/env python3
"""Stage 5C-C BLE command client with explicit write/Flash interlocks."""

import argparse
import asyncio
import json
import struct
import time


FFE1_UUID = "0000ffe1-0000-1000-8000-00805f9b34fb"
FFE2_UUID = "0000ffe2-0000-1000-8000-00805f9b34fb"
SYNC = b"\xA5\x5A"
VERSION = 1
REQUEST = 0x80
RESPONSE = 0x81
HEADER_SIZE = 12
CRC_SIZE = 2
MAX_PAYLOAD = 240

OPERATIONS = {
    "device-info": 0x01,
    "get-config": 0x02,
    "tare": 0x10,
    "clear-tare": 0x11,
    "zero": 0x12,
    "reset-zero": 0x13,
    "set-view": 0x14,
    "set-unit": 0x15,
    "begin-config": 0x20,
    "set-mass": 0x21,
    "set-unit-display": 0x22,
    "set-profile": 0x23,
    "validate-config": 0x24,
    "apply-config": 0x25,
    "discard-config": 0x26,
    "save": 0x27,
}

RESULTS = {
    0: "OK", 1: "INVALID_COMMAND", 2: "INVALID_ARGUMENT",
    3: "INVALID_STATE", 4: "NOT_STABLE", 5: "OUT_OF_RANGE",
    6: "TARE_ACTIVE", 7: "ZERO_DISABLED", 8: "CALIBRATION_INVALID",
    9: "OVERLOAD", 10: "BUSY", 11: "PERSISTENCE_FAILED",
    12: "POWER_UNSAFE", 13: "UNSUPPORTED", 14: "INTERNAL_ERROR",
    15: "TRANSACTION_CONFLICT",
}

WRITE_COMMANDS = set(OPERATIONS) - {"device-info", "get-config"}


def crc16(data):
    state = 0xFFFF
    for value in data:
        state ^= value
        for _ in range(8):
            state = ((state >> 1) ^ 0xA001) if state & 1 else state >> 1
    return state


def encode_request(transaction_id, operation, data=b"", sequence=0,
                   timestamp_ms=0):
    payload = struct.pack("<HBBH", transaction_id, operation, 0, len(data)) + data
    if len(payload) > 128:
        raise ValueError("request payload exceeds 128 bytes")
    frame = struct.pack("<2sBBHHI", SYNC, VERSION, REQUEST, len(payload),
                        sequence, timestamp_ms) + payload
    return frame + struct.pack("<H", crc16(frame))


class FrameParser:
    def __init__(self):
        self.buffer = bytearray()
        self.crc_errors = 0
        self.resync_bytes = 0

    def feed(self, data):
        self.buffer.extend(data)
        frames = []
        while True:
            sync_at = self.buffer.find(SYNC)
            if sync_at < 0:
                keep = 1 if self.buffer and self.buffer[-1] == SYNC[0] else 0
                self.resync_bytes += len(self.buffer) - keep
                self.buffer[:] = self.buffer[-keep:] if keep else b""
                break
            if sync_at:
                self.resync_bytes += sync_at
                del self.buffer[:sync_at]
            if len(self.buffer) < HEADER_SIZE:
                break
            version, message_type = self.buffer[2], self.buffer[3]
            payload_length = struct.unpack_from("<H", self.buffer, 4)[0]
            if version != VERSION or payload_length > MAX_PAYLOAD:
                del self.buffer[0]
                continue
            total = HEADER_SIZE + payload_length + CRC_SIZE
            if len(self.buffer) < total:
                break
            received = struct.unpack_from("<H", self.buffer, total - 2)[0]
            if received != crc16(self.buffer[:total - 2]):
                self.crc_errors += 1
                del self.buffer[0]
                continue
            raw = bytes(self.buffer[:total])
            del self.buffer[:total]
            frames.append((message_type, raw[HEADER_SIZE:-CRC_SIZE], raw))
        return frames


def decode_response(payload):
    if len(payload) < 8:
        raise ValueError("short response payload")
    transaction_id, operation, result, detail, length = struct.unpack_from(
        "<HBBHH", payload)
    if length != len(payload) - 8:
        raise ValueError("response data_length mismatch")
    return {
        "transaction_id": transaction_id,
        "operation": operation,
        "result": result,
        "result_name": RESULTS.get(result, "UNKNOWN"),
        "detail_code": detail,
        "data": payload[8:],
    }


def decode_operation_data(operation, data):
    if operation == OPERATIONS["device-info"] and len(data) == 12:
        protocol, fw_major, fw_minor, _, schema, register_map, caps = struct.unpack(
            "<BBBBHHI", data)
        return {"protocol_version": protocol,
                "firmware_version": (fw_major << 8) | fw_minor,
                "firmware_major": fw_major, "firmware_minor": fw_minor,
                "schema_version": schema,
                "register_map_version": register_map, "capabilities": caps}
    if operation == OPERATIONS["get-config"] and len(data) == 55:
        values = struct.unpack("<BBBBqqqBBBBBqqIBB", data)
        names = ("unit", "decimal_places", "division", "active_profile",
                 "capacity_ug", "overload_threshold_ug", "zero_range_ug",
                 "filter_mode", "filter_strength", "stability_window",
                 "sample_rate", "gain", "stability_enter_ug",
                 "stability_exit_ug", "stability_hold_ms", "persistent_dirty",
                 "config_edit_state")
        return dict(zip(names, values))
    return {"hex": data.hex()}


def command_data(args):
    if args.command == "set-view":
        return bytes(({"net": 0, "gross": 1}[args.view],))
    if args.command == "set-unit":
        return bytes(({"kg": 0, "g": 1, "lb": 2}[args.unit],))
    if args.command == "set-mass":
        field = {"capacity": 0, "zero-range": 1, "overload": 2}[args.field]
        return struct.pack("<Bq", field, args.value_ug)
    if args.command == "set-unit-display":
        return struct.pack("<BBB", {"kg": 0, "g": 1, "lb": 2}[args.unit],
                           args.dp, args.division)
    if args.command == "set-profile":
        profile = {"precision": 0, "speed": 1}[args.profile]
        field = {"filter-mode": 2, "filter-strength": 3,
                 "stability-window": 4, "stability-enter": 5,
                 "stability-exit": 6, "stability-hold-ms": 7}[args.field]
        return struct.pack("<BBq", profile, field, args.value)
    return b""


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--address")
    parser.add_argument("--device-name", default="W02_008324")
    parser.add_argument("--timeout-s", type=float, default=3.0)
    parser.add_argument("--retries", type=int, default=2)
    parser.add_argument("--transaction-id", type=lambda value: int(value, 0))
    parser.add_argument("--allow-write", action="store_true")
    parser.add_argument("--allow-flash", action="store_true")
    subparsers = parser.add_subparsers(dest="command", required=True)
    def add_interlocks(command_parser):
        command_parser.add_argument("--allow-write", action="store_true",
                                    default=argparse.SUPPRESS)
        command_parser.add_argument("--allow-flash", action="store_true",
                                    default=argparse.SUPPRESS)
    for name in ("device-info", "get-config", "tare", "clear-tare", "zero",
                 "reset-zero", "begin-config", "validate-config",
                 "apply-config", "discard-config", "save"):
        add_interlocks(subparsers.add_parser(name))
    view = subparsers.add_parser("set-view")
    add_interlocks(view)
    view.add_argument("--view", choices=("net", "gross"), required=True)
    unit = subparsers.add_parser("set-unit")
    add_interlocks(unit)
    unit.add_argument("--unit", choices=("kg", "g", "lb"), required=True)
    mass = subparsers.add_parser("set-mass")
    add_interlocks(mass)
    mass.add_argument("--field", choices=("capacity", "zero-range", "overload"),
                      required=True)
    mass.add_argument("--value-ug", type=int, required=True)
    display = subparsers.add_parser("set-unit-display")
    add_interlocks(display)
    display.add_argument("--unit", choices=("kg", "g", "lb"), required=True)
    display.add_argument("--dp", type=int, choices=range(6), required=True)
    display.add_argument("--division", type=int, choices=(1, 2, 5), required=True)
    profile = subparsers.add_parser("set-profile")
    add_interlocks(profile)
    profile.add_argument("--profile", choices=("precision", "speed"), required=True)
    profile.add_argument("--field", choices=("filter-mode", "filter-strength",
        "stability-window", "stability-enter", "stability-exit",
        "stability-hold-ms"), required=True)
    profile.add_argument("--value", type=int, required=True)
    return parser.parse_args()


async def run(args):
    from bleak import BleakClient, BleakScanner

    if args.command in WRITE_COMMANDS and not args.allow_write:
        raise RuntimeError("state-changing command requires --allow-write")
    if args.command == "save" and not args.allow_flash:
        raise RuntimeError("SAVE requires --allow-flash")
    operation = OPERATIONS[args.command]
    transaction_id = args.transaction_id
    if transaction_id is None:
        transaction_id = int(time.monotonic_ns() // 1_000_000) & 0xFFFF
        transaction_id = transaction_id or 1
    data = command_data(args)
    request = encode_request(transaction_id, operation, data,
                             transaction_id, int(time.monotonic() * 1000) & 0xFFFFFFFF)
    parser = FrameParser()
    responses = asyncio.Queue()

    def on_notify(_characteristic, notification):
        for message_type, payload, _raw in parser.feed(bytes(notification)):
            if message_type == RESPONSE:
                try:
                    response = decode_response(payload)
                except ValueError:
                    continue
                responses.put_nowait(response)

    device = None
    if args.address:
        device = await BleakScanner.find_device_by_address(args.address, timeout=15.0)
    else:
        devices = await BleakScanner.discover(timeout=15.0)
        device = next((item for item in devices if item.name == args.device_name), None)
    if device is None:
        raise RuntimeError("W02 was not found")
    async with BleakClient(device, timeout=20.0) as client:
        await client.start_notify(FFE1_UUID, on_notify)
        try:
            for attempt in range(args.retries + 1):
                await client.write_gatt_char(FFE2_UUID, request, response=False)
                deadline = time.monotonic() + args.timeout_s
                while True:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        break
                    try:
                        response = await asyncio.wait_for(responses.get(), remaining)
                    except asyncio.TimeoutError:
                        break
                    if (response["transaction_id"] == transaction_id and
                            response["operation"] == operation):
                        decoded = decode_operation_data(operation, response.pop("data"))
                        response["response_data"] = decoded
                        response["attempt"] = attempt + 1
                        print(json.dumps(response, indent=2), flush=True)
                        return 0 if response["result"] == 0 else 1
        finally:
            if client.is_connected:
                await client.stop_notify(FFE1_UUID)
    raise RuntimeError("matching command response timed out")


def main():
    try:
        return asyncio.run(run(parse_args()))
    except Exception as error:
        print(json.dumps({"fatal": str(error)}), flush=True)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
