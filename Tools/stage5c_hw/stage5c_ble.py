#!/usr/bin/env python3
"""Stage 5C-C BLE command client with explicit write/Flash interlocks."""

import argparse
import asyncio
import json
import struct
import time
from decimal import Decimal, InvalidOperation


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
    "cal-status": 0x30,
    "cal-begin": 0x31,
    "cal-set-mass": 0x32,
    "cal-zero": 0x33,
    "cal-load": 0x34,
    "cal-apply": 0x35,
    "cal-cancel": 0x36,
}

RESULTS = {
    0: "OK", 1: "INVALID_COMMAND", 2: "INVALID_ARGUMENT",
    3: "INVALID_STATE", 4: "NOT_STABLE", 5: "OUT_OF_RANGE",
    6: "TARE_ACTIVE", 7: "ZERO_DISABLED", 8: "CALIBRATION_INVALID",
    9: "OVERLOAD", 10: "BUSY", 11: "PERSISTENCE_FAILED",
    12: "POWER_UNSAFE", 13: "UNSUPPORTED", 14: "INTERNAL_ERROR",
    15: "TRANSACTION_CONFLICT",
}

WRITE_COMMANDS = set(OPERATIONS) - {"device-info", "get-config", "cal-status"}
CALIBRATION_COMMANDS = {
    "cal-begin", "cal-set-mass", "cal-zero", "cal-load", "cal-apply",
    "cal-cancel", "calibrate",
}

CALIBRATION_STATES = {
    0: "IDLE", 1: "WAIT_ZERO_STABLE", 2: "ZERO_READY",
    3: "ZERO_CAPTURED", 4: "WAIT_LOAD_STABLE", 5: "LOAD_READY",
    6: "RESULT_READY", 7: "APPLIED", 8: "FAILED",
}
CALIBRATION_OWNERS = {0: "NONE", 1: "LOCAL_UI", 2: "MODBUS", 3: "BLE"}


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
    if (operation in range(OPERATIONS["cal-status"],
                          OPERATIONS["cal-cancel"] + 1) and
            len(data) == 44):
        values = struct.unpack("<BBHBBBBqqiiiIBBH", data)
        (state, owner, session_id, unit, dp, division, flags,
         mass_ug, capacity_ug, zero_raw, load_raw, span_raw,
         sample_sequence, sample_count, last_result, _) = values
        return {
            "state": state,
            "state_name": CALIBRATION_STATES.get(state, "UNKNOWN"),
            "owner": owner,
            "owner_name": CALIBRATION_OWNERS.get(owner, "UNKNOWN"),
            "session_id": session_id,
            "locked_unit": unit,
            "decimal_places": dp,
            "division": division,
            "active": bool(flags & (1 << 0)),
            "stable": bool(flags & (1 << 1)),
            "zero_captured": bool(flags & (1 << 2)),
            "load_captured": bool(flags & (1 << 3)),
            "candidate_valid": bool(flags & (1 << 4)),
            "result_valid": bool(flags & (1 << 5)),
            "persistent_dirty": bool(flags & (1 << 6)),
            "active_calibration_valid": bool(flags & (1 << 7)),
            "calibration_mass_ug": mass_ug,
            "capacity_ug": capacity_ug,
            "zero_raw": zero_raw,
            "load_raw": load_raw,
            "span_raw": span_raw,
            "sample_sequence": sample_sequence,
            "sample_count": sample_count,
            "last_result": last_result,
        }
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
    if args.command == "cal-set-mass":
        return struct.pack("<Hq", args.session_id, args.value_ug)
    if args.command in ("cal-zero", "cal-load", "cal-apply", "cal-cancel"):
        return struct.pack("<H", args.session_id)
    return b""


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--address")
    parser.add_argument("--device-name", default="W02_008324")
    parser.add_argument("--timeout-s", type=float, default=3.0)
    parser.add_argument("--retries", type=int, default=2)
    parser.add_argument("--transaction-id", type=lambda value: int(value, 0))
    parser.add_argument("--allow-write", action="store_true")
    parser.add_argument("--allow-calibration", action="store_true")
    parser.add_argument("--allow-flash", action="store_true")
    subparsers = parser.add_subparsers(dest="command", required=True)
    def add_interlocks(command_parser):
        command_parser.add_argument("--allow-write", action="store_true",
                                    default=argparse.SUPPRESS)
        command_parser.add_argument("--allow-calibration", action="store_true",
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
    for name in ("cal-status", "cal-begin"):
        add_interlocks(subparsers.add_parser(name))
    cal_mass = subparsers.add_parser("cal-set-mass")
    add_interlocks(cal_mass)
    cal_mass.add_argument("--session-id", type=lambda value: int(value, 0),
                          required=True)
    cal_mass.add_argument("--value-ug", type=int, required=True)
    for name in ("cal-zero", "cal-load", "cal-apply", "cal-cancel"):
        action = subparsers.add_parser(name)
        add_interlocks(action)
        action.add_argument("--session-id", type=lambda value: int(value, 0),
                            required=True)
    wizard = subparsers.add_parser("calibrate")
    add_interlocks(wizard)
    wizard.add_argument("--mass-g")
    return parser.parse_args()


async def run(args):
    from bleak import BleakClient, BleakScanner

    if args.command in WRITE_COMMANDS and not args.allow_write:
        raise RuntimeError("state-changing command requires --allow-write")
    if args.command in CALIBRATION_COMMANDS and not args.allow_calibration:
        raise RuntimeError("calibration command requires --allow-calibration")
    if args.command == "save" and not args.allow_flash:
        raise RuntimeError("SAVE requires --allow-flash")
    parser = FrameParser()
    responses = asyncio.Queue()
    next_transaction = args.transaction_id
    if next_transaction is None:
        next_transaction = int(time.monotonic_ns() // 1_000_000) & 0xFFFF
    next_transaction = next_transaction or 1

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

        async def send(operation, data=b""):
            nonlocal next_transaction
            transaction_id = next_transaction
            next_transaction = (next_transaction + 1) & 0xFFFF
            next_transaction = next_transaction or 1
            request = encode_request(transaction_id, operation, data,
                transaction_id, int(time.monotonic() * 1000) & 0xFFFFFFFF)
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
                        response["response_data"] = decode_operation_data(
                            operation, response.pop("data"))
                        response["attempt"] = attempt + 1
                        return response
            raise RuntimeError("matching command response timed out")

        async def require_ok(operation, data=b""):
            response = await send(operation, data)
            print(json.dumps(response, indent=2), flush=True)
            if response["result"] != 0:
                raise RuntimeError(response["result_name"])
            return response

        async def wait_for_state(expected, timeout_s=120.0):
            deadline = time.monotonic() + timeout_s
            while time.monotonic() < deadline:
                response = await send(OPERATIONS["cal-status"])
                state = response["response_data"].get("state_name")
                if response["result"] != 0:
                    raise RuntimeError(response["result_name"])
                if state == expected:
                    print(json.dumps(response, indent=2), flush=True)
                    return response["response_data"]
                await asyncio.sleep(0.5)
            raise RuntimeError("calibration state wait timed out")

        try:
            if args.command != "calibrate":
                response = await send(OPERATIONS[args.command], command_data(args))
                print(json.dumps(response, indent=2), flush=True)
                return 0 if response["result"] == 0 else 1

            state_response = await require_ok(OPERATIONS["cal-status"])
            state = state_response["response_data"]
            if state["active"]:
                if state["owner_name"] != "BLE":
                    raise RuntimeError("calibration session is owned by " +
                                       state["owner_name"])
                session_id = state["session_id"]
            else:
                begin = await require_ok(OPERATIONS["cal-begin"])
                state = begin["response_data"]
                session_id = state["session_id"]
            workflow_state = state["state_name"]
            if workflow_state != "RESULT_READY":
                if args.mass_g is not None:
                    mass_text = args.mass_g
                elif state.get("calibration_mass_ug", 0) > 0:
                    mass_text = str(Decimal(state["calibration_mass_ug"]) /
                                    Decimal(1000000))
                else:
                    mass_text = input("Known calibration mass in g: ").strip()
                try:
                    mass_ug_decimal = Decimal(mass_text) * Decimal(1000000)
                except InvalidOperation as error:
                    raise RuntimeError("invalid calibration mass") from error
                if mass_ug_decimal != mass_ug_decimal.to_integral_value():
                    raise RuntimeError(
                        "mass cannot be represented as whole micrograms")
                mass_ug = int(mass_ug_decimal)
                if mass_ug != state.get("calibration_mass_ug", 0):
                    mass_response = await require_ok(
                        OPERATIONS["cal-set-mass"],
                        struct.pack("<Hq", session_id, mass_ug))
                    workflow_state = mass_response["response_data"]["state_name"]
                if workflow_state in ("WAIT_ZERO_STABLE", "ZERO_READY"):
                    input("Clear the scale, then press Enter to wait for zero stability: ")
                    await wait_for_state("ZERO_READY")
                    zero_response = await require_ok(OPERATIONS["cal-zero"],
                        struct.pack("<H", session_id))
                    workflow_state = zero_response["response_data"]["state_name"]
                if workflow_state == "ZERO_CAPTURED":
                    mass_response = await require_ok(
                        OPERATIONS["cal-set-mass"],
                        struct.pack("<Hq", session_id, mass_ug))
                    workflow_state = mass_response["response_data"]["state_name"]
                if workflow_state not in ("WAIT_LOAD_STABLE", "LOAD_READY"):
                    raise RuntimeError("cannot resume calibration from " +
                                       workflow_state)
                input("Place the known mass, then press Enter to wait for load stability: ")
                await wait_for_state("LOAD_READY")
                result = await require_ok(OPERATIONS["cal-load"],
                                          struct.pack("<H", session_id))
                if result["response_data"].get("state_name") != "RESULT_READY":
                    raise RuntimeError("calibration candidate is not ready")
            decision = input("Type APPLY to apply the candidate to RAM, or CANCEL: ").strip()
            if decision != "APPLY":
                if decision == "CANCEL":
                    await require_ok(OPERATIONS["cal-cancel"],
                                     struct.pack("<H", session_id))
                return 1
            applied = await require_ok(OPERATIONS["cal-apply"],
                                       struct.pack("<H", session_id))
            if not applied["response_data"].get("persistent_dirty"):
                raise RuntimeError("APPLY did not mark configuration dirty")
            if args.allow_flash:
                if input("Type SAVE to write the calibration to Flash: ").strip() == "SAVE":
                    await require_ok(OPERATIONS["save"])
            else:
                print("Calibration is active in RAM only; use SAVE with --allow-flash after verification.")
            return 0
        finally:
            if client.is_connected:
                await client.stop_notify(FFE1_UUID)


def main():
    try:
        return asyncio.run(run(parse_args()))
    except Exception as error:
        print(json.dumps({"fatal": str(error)}), flush=True)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
