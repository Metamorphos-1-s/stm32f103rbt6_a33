#!/usr/bin/env python3
"""BLE telemetry and non-persistent command concurrency soak test."""

import argparse
import asyncio
import csv
import json
import struct
import time
from pathlib import Path

from bleak import BleakClient, BleakScanner

from ble_telemetry import (
    CHECKWEIGH_TYPE, FAST_TYPE, SLOW_TYPE, TelemetryParser,
)
from stage5c_ble import (
    FFE1_UUID, FFE2_UUID, OPERATIONS, RESPONSE, FrameParser,
    decode_operation_data, decode_response, encode_request,
)


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--address", required=True)
    parser.add_argument("--duration-s", type=float, default=600.0)
    parser.add_argument("--command-interval-s", type=float, default=25.0)
    parser.add_argument("--response-timeout-s", type=float, default=3.0)
    parser.add_argument("--retries", type=int, default=2)
    parser.add_argument(
        "--include-calibration-sessions", action="store_true",
        help="periodically run BEGIN followed immediately by CANCEL; never captures, applies, or saves")
    parser.add_argument("--output-dir", required=True)
    return parser.parse_args()


def write_csv(path, rows):
    if not rows:
        return
    with path.open("w", newline="", encoding="ascii") as stream:
        writer = csv.DictWriter(stream, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)


async def run(args):
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    command_log = output_dir / "commands.jsonl"
    stream_parser = FrameParser()
    telemetry = TelemetryParser()
    responses = asyncio.Queue()
    started = time.monotonic()
    intentional_disconnect = False
    stats = {
        "address": args.address,
        "duration_s": args.duration_s,
        "command_interval_s": args.command_interval_s,
        "connected": False,
        "disconnects": 0,
        "command_requests": 0,
        "command_responses": 0,
        "device_info_responses": 0,
        "get_config_responses": 0,
        "cal_status_responses": 0,
        "cal_begin_responses": 0,
        "cal_cancel_responses": 0,
        "calibration_sessions_completed": 0,
        "command_timeouts": 0,
        "command_retries": 0,
        "command_result_errors": 0,
        "transaction_mismatches": 0,
        "unknown_frames": 0,
    }

    def log_command(entry):
        with command_log.open("a", encoding="ascii") as stream:
            stream.write(json.dumps(entry, separators=(",", ":")) + "\n")

    def on_disconnect(_client):
        if not intentional_disconnect:
            stats["disconnects"] += 1

    def on_notify(_characteristic, notification):
        for message_type, payload, raw in stream_parser.feed(bytes(notification)):
            if message_type in (FAST_TYPE, SLOW_TYPE, CHECKWEIGH_TYPE):
                telemetry.feed(raw)
            elif message_type == RESPONSE:
                try:
                    responses.put_nowait(decode_response(payload))
                except ValueError:
                    stats["unknown_frames"] += 1
            else:
                stats["unknown_frames"] += 1

    operation_names = {
        OPERATIONS["device-info"]: "device-info",
        OPERATIONS["get-config"]: "get-config",
        OPERATIONS["cal-status"]: "cal-status",
        OPERATIONS["cal-begin"]: "cal-begin",
        OPERATIONS["cal-cancel"]: "cal-cancel",
    }

    async def execute_command(client, transaction_id, operation, data=b""):
        request = encode_request(
            transaction_id, operation, data, transaction_id,
            int(time.monotonic() * 1000) & 0xFFFFFFFF)
        stats["command_requests"] += 1
        for attempt in range(args.retries + 1):
            if attempt:
                stats["command_retries"] += 1
            await client.write_gatt_char(FFE2_UUID, request, response=False)
            deadline = time.monotonic() + args.response_timeout_s
            while time.monotonic() < deadline:
                remaining = deadline - time.monotonic()
                try:
                    response = await asyncio.wait_for(responses.get(), remaining)
                except asyncio.TimeoutError:
                    break
                if (response["transaction_id"] != transaction_id or
                        response["operation"] != operation):
                    stats["transaction_mismatches"] += 1
                    continue
                stats["command_responses"] += 1
                if response["result"] != 0:
                    stats["command_result_errors"] += 1
                name = operation_names[operation]
                if operation == OPERATIONS["device-info"]:
                    stats["device_info_responses"] += 1
                elif operation == OPERATIONS["get-config"]:
                    stats["get_config_responses"] += 1
                elif operation == OPERATIONS["cal-status"]:
                    stats["cal_status_responses"] += 1
                elif operation == OPERATIONS["cal-begin"]:
                    stats["cal_begin_responses"] += 1
                elif operation == OPERATIONS["cal-cancel"]:
                    stats["cal_cancel_responses"] += 1
                response_data = decode_operation_data(operation, response["data"])
                entry = {
                    "elapsed_s": round(time.monotonic() - started, 6),
                    "transaction_id": transaction_id,
                    "operation": name,
                    "attempt": attempt + 1,
                    "result": response["result"],
                    "detail_code": response["detail_code"],
                    "response_data": response_data,
                }
                log_command(entry)
                print("COMMAND " + json.dumps(entry, separators=(",", ":")), flush=True)
                response["response_data"] = response_data
                return response
        stats["command_timeouts"] += 1
        log_command({
            "elapsed_s": round(time.monotonic() - started, 6),
            "transaction_id": transaction_id,
            "operation": operation,
            "timeout": True,
        })
        return None

    device = await BleakScanner.find_device_by_address(args.address, timeout=15.0)
    if device is None:
        raise RuntimeError("W02 was not found")
    async with BleakClient(device, disconnected_callback=on_disconnect,
                           timeout=20.0) as client:
        if not client.is_connected:
            raise RuntimeError("BLE connection failed")
        stats["connected"] = True
        await client.start_notify(FFE1_UUID, on_notify)
        await asyncio.sleep(1.0)
        deadline = time.monotonic() + args.duration_s
        transaction_id = 0x6000
        command_index = 0
        while time.monotonic() < deadline:
            run_calibration_session = (
                args.include_calibration_sessions and
                (command_index % 6 == 3))
            if run_calibration_session:
                begin = await execute_command(
                    client, transaction_id, OPERATIONS["cal-begin"])
                transaction_id = (transaction_id + 1) & 0xFFFF
                if begin is not None and begin["result"] == 0:
                    session_id = begin["response_data"]["session_id"]
                    cancel = await execute_command(
                        client, transaction_id, OPERATIONS["cal-cancel"],
                        struct.pack("<H", session_id))
                    transaction_id = (transaction_id + 1) & 0xFFFF
                    if cancel is not None and cancel["result"] == 0:
                        stats["calibration_sessions_completed"] += 1
            else:
                read_operations = (
                    OPERATIONS["device-info"], OPERATIONS["get-config"],
                    OPERATIONS["cal-status"])
                operation = read_operations[command_index % len(read_operations)]
                await execute_command(client, transaction_id, operation)
                transaction_id = (transaction_id + 1) & 0xFFFF
            command_index += 1
            delay = min(args.command_interval_s, deadline - time.monotonic())
            if delay > 0:
                await asyncio.sleep(delay)
        await asyncio.sleep(1.0)
        stats["connected_at_end"] = bool(client.is_connected)
        intentional_disconnect = True
        if client.is_connected:
            await client.stop_notify(FFE1_UUID)

    fast = [frame for frame in telemetry.frames if frame["message_type"] == FAST_TYPE]
    slow = [frame for frame in telemetry.frames if frame["message_type"] == SLOW_TYPE]
    checkweigh = [frame for frame in telemetry.frames
                  if frame["message_type"] == CHECKWEIGH_TYPE]
    write_csv(output_dir / "fast.csv", fast)
    write_csv(output_dir / "slow.csv", slow)
    write_csv(output_dir / "checkweigh.csv", checkweigh)
    stats.update({
        "elapsed_s": round(time.monotonic() - started, 6),
        "frames_received": telemetry.frames_received,
        "fast_frames": len(fast),
        "slow_frames": len(slow),
        "checkweigh_frames": len(checkweigh),
        "telemetry_crc_errors": telemetry.crc_errors,
        "telemetry_sequence_gaps": telemetry.sequence_gaps,
        "telemetry_duplicates": telemetry.duplicates,
        "telemetry_timestamp_anomalies": telemetry.timestamp_anomalies,
        "stream_crc_errors": stream_parser.crc_errors,
        "stream_resync_bytes": stream_parser.resync_bytes,
        "partial_bytes": len(stream_parser.buffer),
    })
    stats["passed"] = (
        stats["connected"] and stats.get("connected_at_end", False) and
        stats["disconnects"] == 0 and
        stats["command_requests"] == stats["command_responses"] and
        stats["command_timeouts"] == 0 and
        stats["command_result_errors"] == 0 and
        stats["transaction_mismatches"] == 0 and
        stats["telemetry_crc_errors"] == 0 and
        stats["telemetry_sequence_gaps"] == 0 and
        stats["telemetry_duplicates"] == 0 and
        stats["stream_crc_errors"] == 0 and
        stats["unknown_frames"] == 0 and
        stats["partial_bytes"] == 0 and
        (not args.include_calibration_sessions or
         stats["calibration_sessions_completed"] > 0) and
        len(fast) >= int(args.duration_s * 4.5) and
        len(slow) >= int(args.duration_s * 0.8) and
        len(checkweigh) >= int(args.duration_s * 0.8)
    )
    (output_dir / "summary.json").write_text(
        json.dumps(stats, indent=2) + "\n", encoding="ascii")
    print("COMMAND_SOAK_SUMMARY " + json.dumps(stats, separators=(",", ":")), flush=True)
    return 0 if stats["passed"] else 1


def main():
    try:
        return asyncio.run(run(parse_args()))
    except Exception as error:
        print("COMMAND_SOAK_FATAL " + repr(error), flush=True)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
