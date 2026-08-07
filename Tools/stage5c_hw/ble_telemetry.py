#!/usr/bin/env python3
"""Read-only Stage 5C-B BLE telemetry stream and write CSV/JSON results."""

import argparse
import asyncio
import csv
import json
import struct
import time
from pathlib import Path


FFE0_UUID = "0000ffe0-0000-1000-8000-00805f9b34fb"
FFE1_UUID = "0000ffe1-0000-1000-8000-00805f9b34fb"
SYNC = b"\xA5\x5A"
HEADER_SIZE = 12
CRC_SIZE = 2
FAST_TYPE = 0x01
SLOW_TYPE = 0x02
FAST_PAYLOAD_SIZE = 42
SLOW_PAYLOAD_SIZE = 59


def crc16(data):
    state = 0xFFFF
    for value in data:
        state ^= value
        for _ in range(8):
            state = ((state >> 1) ^ 0xA001) if state & 1 else state >> 1
    return state


class TelemetryParser:
    """Incremental parser; Notify boundaries are intentionally ignored."""

    def __init__(self):
        self.buffer = bytearray()
        self.frames = []
        self.frames_received = 0
        self.crc_errors = 0
        self.length_errors = 0
        self.version_errors = 0
        self.type_errors = 0
        self.parser_resync = 0
        self.sequence_gaps = 0
        self.duplicates = 0
        self.timestamp_anomalies = 0
        self._last_sequence = None
        self._last_timestamp = None

    def feed(self, data):
        self.buffer.extend(data)
        parsed = []
        while True:
            sync_at = self.buffer.find(SYNC)
            if sync_at < 0:
                keep = 1 if self.buffer and self.buffer[-1] == SYNC[0] else 0
                self.parser_resync += len(self.buffer) - keep
                if keep:
                    self.buffer[:] = self.buffer[-1:]
                else:
                    self.buffer.clear()
                break
            if sync_at:
                self.parser_resync += sync_at
                del self.buffer[:sync_at]
            if len(self.buffer) < HEADER_SIZE:
                break
            version = self.buffer[2]
            message_type = self.buffer[3]
            payload_length = struct.unpack_from("<H", self.buffer, 4)[0]
            expected = {FAST_TYPE: FAST_PAYLOAD_SIZE, SLOW_TYPE: SLOW_PAYLOAD_SIZE}.get(message_type)
            if version != 1:
                self.version_errors += 1
                del self.buffer[0]
                continue
            if expected is None:
                self.type_errors += 1
                del self.buffer[0]
                continue
            if payload_length != expected:
                self.length_errors += 1
                del self.buffer[0]
                continue
            total = HEADER_SIZE + payload_length + CRC_SIZE
            if len(self.buffer) < total:
                break
            received_crc = struct.unpack_from("<H", self.buffer, total - 2)[0]
            if crc16(self.buffer[: total - 2]) != received_crc:
                self.crc_errors += 1
                del self.buffer[0]
                continue
            raw = bytes(self.buffer[:total])
            del self.buffer[:total]
            frame = self._decode(raw)
            self._check_order(frame)
            self.frames_received += 1
            self.frames.append(frame)
            parsed.append(frame)
        return parsed

    def _decode(self, raw):
        message_type = raw[3]
        sequence = struct.unpack_from("<H", raw, 6)[0]
        timestamp = struct.unpack_from("<I", raw, 8)[0]
        payload = raw[HEADER_SIZE:-CRC_SIZE]
        frame = {"frame_sequence": sequence, "device_timestamp_ms": timestamp,
                 "message_type": message_type}
        if message_type == FAST_TYPE:
            fields = struct.unpack_from("<IqqqqBBBBBB", payload)
            (frame["measurement_sequence"], frame["display_mass_ug"],
             frame["net_mass_ug"], frame["gross_mass_ug"], frame["tare_mass_ug"],
             frame["stable"], frame["display_locked"], frame["overload"],
             frame["unit"], frame["dp"], frame["division"]) = fields
            frame["display_mass_g"] = frame["display_mass_ug"] / 1000000.0
        else:
            fields = struct.unpack_from("<iiqqqB B B qqBBB B I", payload)
            (frame["raw_count"], frame["filtered_raw"],
             frame["uncompensated_gross_ug"], frame["compensated_gross_ug"],
             frame["runtime_drift_offset_ug"], frame["runtime_drift_enabled"],
             frame["runtime_drift_state"], frame["persistent_dirty"],
             frame["capacity_ug"], frame["overload_threshold_ug"],
             frame["filter_mode"], frame["filter_strength"],
             frame["active_profile"], frame["app_state"], frame["fault_mask"]) = fields
        frame["host_timestamp"] = time.time()
        return frame

    def _check_order(self, frame):
        sequence = frame["frame_sequence"]
        timestamp = frame["device_timestamp_ms"]
        if self._last_sequence is not None:
            previous = self._last_sequence
            if sequence == previous:
                self.duplicates += 1
            else:
                gap = (sequence - previous - 1) & 0xFFFF
                if gap < 0x8000:
                    self.sequence_gaps += gap
        if self._last_timestamp is not None:
            delta = (timestamp - self._last_timestamp) & 0xFFFFFFFF
            if delta == 0 or delta >= 0x80000000:
                self.timestamp_anomalies += 1
        self._last_sequence = sequence
        self._last_timestamp = timestamp


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--address")
    parser.add_argument("--device-name", default="W02_008324")
    parser.add_argument("--duration-s", type=float, default=60.0)
    parser.add_argument("--output-dir", required=True)
    return parser.parse_args()


async def run(args):
    from bleak import BleakClient, BleakScanner

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    parser = TelemetryParser()
    started = time.monotonic()
    disconnected = 0
    intentional_disconnect = False
    device = None
    if args.address:
        device = await BleakScanner.find_device_by_address(args.address, timeout=15.0)
    else:
        devices = await BleakScanner.discover(timeout=15.0)
        device = next((item for item in devices if item.name == args.device_name), None)
    if device is None:
        raise RuntimeError("W02 was not found")

    def on_disconnect(_client):
        nonlocal disconnected
        if not intentional_disconnect:
            disconnected += 1

    def on_notify(_characteristic, data):
        frames = parser.feed(bytes(data))
        for frame in frames[-1:]:
            if frame["message_type"] == FAST_TYPE:
                print(f"{frame['display_mass_g']:.6f} g stable={frame['stable']} "
                      f"locked={frame['display_locked']}", flush=True)

    async with BleakClient(device, disconnected_callback=on_disconnect,
                           timeout=20.0) as client:
        if not client.is_connected:
            raise RuntimeError("BLE connection failed")
        await client.start_notify(FFE1_UUID, on_notify)
        try:
            await asyncio.sleep(args.duration_s)
        finally:
            intentional_disconnect = True
            if client.is_connected:
                await client.stop_notify(FFE1_UUID)
    fast = [frame for frame in parser.frames if frame["message_type"] == FAST_TYPE]
    slow = [frame for frame in parser.frames if frame["message_type"] == SLOW_TYPE]
    for name, rows in (("fast.csv", fast), ("slow.csv", slow)):
        if rows:
            with (output_dir / name).open("w", newline="", encoding="ascii") as stream:
                writer = csv.DictWriter(stream, fieldnames=rows[0].keys())
                writer.writeheader()
                writer.writerows(rows)
    summary = {"duration_s": time.monotonic() - started,
               "frames_received": parser.frames_received,
               "fast_frames": len(fast), "slow_frames": len(slow),
               "crc_errors": parser.crc_errors, "sequence_gaps": parser.sequence_gaps,
               "duplicates": parser.duplicates, "parser_resync": parser.parser_resync,
               "timestamp_anomalies": parser.timestamp_anomalies,
               "disconnects": disconnected, "partial_bytes": len(parser.buffer)}
    (output_dir / "telemetry_summary.json").write_text(
        json.dumps(summary, indent=2) + "\n", encoding="ascii")
    print("TELEMETRY_SUMMARY " + json.dumps(summary, separators=(",", ":")), flush=True)
    return 0 if (summary["crc_errors"] == 0 and summary["sequence_gaps"] == 0 and
                 summary["disconnects"] == 0 and not parser.buffer) else 1


def main():
    try:
        return asyncio.run(run(parse_args()))
    except Exception as error:
        print(f"TELEMETRY_FATAL {error!r}", flush=True)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
