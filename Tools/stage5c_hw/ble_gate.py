#!/usr/bin/env python3
"""Run a bounded BLE telemetry window with interleaved read-only commands."""

import argparse
import asyncio
import json
import time
from pathlib import Path

from bleak import BleakClient, BleakScanner

from ble_telemetry import (CHECKWEIGH_TYPE, FAST_TYPE, FFE1_UUID,
                           SLOW_TYPE, TelemetryParser)
from stage5c_ble import (FFE2_UUID, FrameParser, OPERATIONS, RESPONSE,
                         decode_response, encode_request)


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--address", required=True)
    parser.add_argument("--duration-s", type=float, default=600.0)
    parser.add_argument("--command-rounds", type=int, default=24)
    parser.add_argument("--output-dir", required=True)
    return parser.parse_args()


async def run(args):
    output = Path(args.output_dir)
    output.mkdir(parents=True, exist_ok=True)
    events_path = output / "notifications.jsonl"
    telemetry = TelemetryParser()
    protocol = FrameParser()
    responses = asyncio.Queue()
    first_frame = asyncio.Event()
    disconnected = 0
    intentional_disconnect = False
    notification_count = 0
    notification_bytes = 0
    transaction = int(time.monotonic_ns() // 1_000_000) & 0xFFFF or 1

    def on_disconnect(_client):
        nonlocal disconnected
        if not intentional_disconnect:
            disconnected += 1

    def on_notify(_characteristic, data):
        nonlocal notification_count, notification_bytes
        payload = bytes(data)
        notification_count += 1
        notification_bytes += len(payload)
        with events_path.open("a", encoding="ascii") as stream:
            stream.write(json.dumps({"t": round(time.monotonic(), 6),
                "length": len(payload), "hex": payload.hex()},
                separators=(",", ":")) + "\n")
        for message_type, response_payload, raw in protocol.feed(payload):
            if message_type == RESPONSE:
                try:
                    responses.put_nowait(decode_response(response_payload))
                except ValueError:
                    pass
            elif message_type in (FAST_TYPE, SLOW_TYPE, CHECKWEIGH_TYPE):
                if telemetry.feed(raw):
                    first_frame.set()

    device = await BleakScanner.find_device_by_address(args.address,
                                                        timeout=15.0)
    if device is None:
        raise RuntimeError("W02 was not found")
    command_results = []
    started = time.monotonic()
    async with BleakClient(device, disconnected_callback=on_disconnect,
                           timeout=20.0) as client:
        await client.start_notify(FFE1_UUID, on_notify)
        await asyncio.wait_for(first_frame.wait(), timeout=5.0)
        window_start = time.monotonic()
        deadline = window_start + args.duration_s
        interval = args.duration_s / max(1, args.command_rounds)
        for index in range(args.command_rounds):
            target = window_start + index * interval
            await asyncio.sleep(max(0.0, target - time.monotonic()))
            name = ("device-info", "get-config", "cal-status")[index % 3]
            operation = OPERATIONS[name]
            current = transaction
            transaction = (transaction + 1) & 0xFFFF or 1
            request = encode_request(current, operation, b"", current,
                int(time.monotonic() * 1000) & 0xFFFFFFFF)
            await client.write_gatt_char(FFE2_UUID, request, response=False)
            response = None
            response_deadline = time.monotonic() + 3.0
            while time.monotonic() < response_deadline:
                try:
                    candidate = await asyncio.wait_for(responses.get(),
                        response_deadline - time.monotonic())
                except asyncio.TimeoutError:
                    break
                if (candidate["transaction_id"] == current and
                    candidate["operation"] == operation):
                    response = candidate
                    break
            command_results.append({"index": index + 1, "name": name,
                "transaction_id": current, "ok": response is not None and
                response["result"] == 0,
                "result": None if response is None else response["result"]})
        await asyncio.sleep(max(0.0, deadline - time.monotonic()))
        intentional_disconnect = True
        if client.is_connected:
            await client.stop_notify(FFE1_UUID)

    fast = sum(item["message_type"] == FAST_TYPE for item in telemetry.frames)
    slow = sum(item["message_type"] == SLOW_TYPE for item in telemetry.frames)
    checkweigh = sum(item["message_type"] == CHECKWEIGH_TYPE
                     for item in telemetry.frames)
    summary = {"address": args.address,
        "duration_s": round(time.monotonic() - window_start, 6),
        "elapsed_s": round(time.monotonic() - started, 6),
        "notifications": notification_count,
        "notification_bytes": notification_bytes,
        "frames_received": telemetry.frames_received,
        "fast_frames": fast, "slow_frames": slow,
        "checkweigh_frames": checkweigh,
        "crc_errors": protocol.crc_errors,
        "sequence_gaps": telemetry.sequence_gaps,
        "duplicates": telemetry.duplicates,
        "parser_resync": protocol.resync_bytes,
        "timestamp_anomalies": telemetry.timestamp_anomalies,
        "disconnects": disconnected, "partial_bytes": len(telemetry.buffer),
        "command_rounds": args.command_rounds,
        "command_success": sum(item["ok"] for item in command_results),
        "commands": command_results}
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n",
                                          encoding="ascii")
    print("BLE_GATE_SUMMARY " + json.dumps(summary, separators=(",", ":")),
          flush=True)
    passed = (summary["sequence_gaps"] == 0 and
        summary["parser_resync"] == 0 and summary["crc_errors"] == 0 and
        summary["duplicates"] == 0 and summary["partial_bytes"] == 0 and
        summary["disconnects"] == 0 and
        summary["command_success"] == args.command_rounds)
    return 0 if passed else 1


def main():
    args = parse_args()
    try:
        return asyncio.run(run(args))
    except Exception as error:
        print("BLE_GATE_FATAL " + repr(error), flush=True)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
