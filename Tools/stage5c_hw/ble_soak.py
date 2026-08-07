#!/usr/bin/env python3
"""Run the PC side of the Stage 5C BLE bidirectional soak test."""

import argparse
import asyncio
import json
import time
from pathlib import Path

from bleak import BleakClient, BleakScanner


FFE1_UUID = "0000ffe1-0000-1000-8000-00805f9b34fb"
FFE2_UUID = "0000ffe2-0000-1000-8000-00805f9b34fb"
RX_FRAME = b"Hello W02"
TX_FRAME = b"ABC123"


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--address", required=True)
    parser.add_argument("--duration-s", type=float, default=600.0)
    parser.add_argument("--interval-s", type=float, default=1.0)
    parser.add_argument("--start-delay-s", type=float, default=3.0)
    parser.add_argument("--output-dir", required=True)
    return parser.parse_args()


async def run(args):
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    events_path = output_dir / "ble_events.jsonl"
    summary_path = output_dir / "ble_summary.json"
    started_wall = time.strftime("%Y-%m-%dT%H:%M:%S%z")
    started_mono = time.monotonic()
    rx_buffer = bytearray()
    intentional_disconnect = False
    stats = {
        "address": args.address,
        "duration_s": args.duration_s,
        "interval_s": args.interval_s,
        "started": started_wall,
        "connected": False,
        "disconnect_count": 0,
        "write_attempts": 0,
        "write_success": 0,
        "write_errors": 0,
        "notify_callbacks": 0,
        "notify_bytes": 0,
        "notify_frames": 0,
        "notify_mismatch_bytes": 0,
        "notify_partial_bytes": 0,
    }

    def log_event(kind, **values):
        event = {
            "elapsed_s": round(time.monotonic() - started_mono, 6),
            "kind": kind,
        }
        event.update(values)
        with events_path.open("a", encoding="ascii") as stream:
            stream.write(json.dumps(event, separators=(",", ":")) + "\n")

    def parse_notifications():
        while len(rx_buffer) >= len(RX_FRAME):
            if rx_buffer.startswith(RX_FRAME):
                del rx_buffer[:len(RX_FRAME)]
                stats["notify_frames"] += 1
            else:
                del rx_buffer[0]
                stats["notify_mismatch_bytes"] += 1

    def on_notify(_characteristic, data):
        payload = bytes(data)
        stats["notify_callbacks"] += 1
        stats["notify_bytes"] += len(payload)
        rx_buffer.extend(payload)
        parse_notifications()
        log_event("notify", hex=payload.hex(), length=len(payload))

    def on_disconnect(_client):
        if not intentional_disconnect:
            stats["disconnect_count"] += 1
            log_event("disconnect")
            print("BLE_DISCONNECTED", flush=True)

    device = await BleakScanner.find_device_by_address(args.address, timeout=15.0)
    if device is None:
        raise RuntimeError("W02 was not found during the pre-connect scan")

    async with BleakClient(device, disconnected_callback=on_disconnect,
                           timeout=20.0) as client:
        stats["connected"] = bool(client.is_connected)
        await client.start_notify(FFE1_UUID, on_notify)
        log_event("ready")
        print("BLE_READY", flush=True)
        await asyncio.sleep(args.start_delay_s)

        test_start = time.monotonic()
        deadline = test_start + args.duration_s
        next_write = test_start
        while time.monotonic() < deadline:
            delay = next_write - time.monotonic()
            if delay > 0.0:
                await asyncio.sleep(delay)
            if time.monotonic() >= deadline:
                break
            stats["write_attempts"] += 1
            try:
                await client.write_gatt_char(FFE2_UUID, TX_FRAME, response=False)
                stats["write_success"] += 1
                log_event("write", hex=TX_FRAME.hex())
            except Exception as error:
                stats["write_errors"] += 1
                log_event("write_error", error=repr(error))
                raise
            next_write += args.interval_s

        await asyncio.sleep(1.5)
        if client.is_connected:
            await client.stop_notify(FFE1_UUID)
        stats["connected_at_end"] = bool(client.is_connected)
        intentional_disconnect = True

    parse_notifications()
    stats["notify_partial_bytes"] = len(rx_buffer)
    stats["minimum_notify_frames"] = max(1, stats["write_success"] - 2)
    stats["elapsed_s"] = round(time.monotonic() - started_mono, 6)
    stats["passed_transport"] = (
        stats["connected"] and stats.get("connected_at_end", False) and
        stats["disconnect_count"] == 0 and stats["write_errors"] == 0 and
        stats["notify_frames"] >= stats["minimum_notify_frames"] and
        stats["notify_mismatch_bytes"] == 0 and
        stats["notify_partial_bytes"] == 0
    )
    summary_path.write_text(json.dumps(stats, indent=2) + "\n", encoding="ascii")
    print("BLE_SUMMARY " + json.dumps(stats, separators=(",", ":")), flush=True)
    return 0 if stats["passed_transport"] else 1


def main():
    args = parse_args()
    try:
        return asyncio.run(run(args))
    except Exception as error:
        print(f"BLE_FATAL {error!r}", flush=True)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
