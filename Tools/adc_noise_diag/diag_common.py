"""Shared diagnostic identity, wrap tracking, and CSV helpers."""

import csv
from dataclasses import dataclass
from pathlib import Path


MODE_IDS = {
    "internal_short": 1,
    "channel_a": 2,
    "channel_a_display_off": 3,
}

MODE_NAMES = {value: key for key, value in MODE_IDS.items()}
CS1237_STATE_RUNNING = 4


def decode_cs1237_config(value):
    if not 0 <= value <= 0x7F:
        raise ValueError("invalid CS1237 configuration register")
    channel_bits = value & 0x03
    channels = {0: "channel_a", 2: "temperature", 3: "internal_short"}
    if channel_bits not in channels:
        raise ValueError("reserved CS1237 channel encoding")
    return {
        "register": value,
        "rate_hz": (10, 40, 640, 1280)[(value >> 4) & 0x03],
        "gain": (1, 2, 64, 128)[(value >> 2) & 0x03],
        "channel": channels[channel_bits],
        "reference_output_enabled": (value & 0x40) == 0,
    }


def validate_diagnostic_identity(requested_mode, reported_mode, config_register):
    expected_mode = MODE_IDS[requested_mode]
    if reported_mode != expected_mode:
        actual = MODE_NAMES.get(reported_mode, "product_or_unknown")
        raise ValueError(
            "diagnostic image mismatch: requested %s, device reports %s (%d)" %
            (requested_mode, actual, reported_mode))
    decoded = decode_cs1237_config(config_register)
    expected_channel = "internal_short" if expected_mode == 1 else "channel_a"
    if decoded["channel"] != expected_channel or decoded["rate_hz"] != 10 or \
            decoded["gain"] != 128 or not decoded["reference_output_enabled"]:
        raise ValueError("CS1237 readback mismatch: %r" % decoded)
    return decoded


@dataclass
class SequenceUpdate:
    accepted: bool
    delta: int = 0
    lost: int = 0


class U32SequenceTracker:
    def __init__(self):
        self.previous = None
        self.total_lost = 0

    def update(self, value):
        value &= 0xFFFFFFFF
        if self.previous is None:
            self.previous = value
            return SequenceUpdate(True, 1, 0)
        delta = (value - self.previous) & 0xFFFFFFFF
        if delta == 0:
            return SequenceUpdate(False, 0, 0)
        if delta > 0x7FFFFFFF:
            raise ValueError("sequence moved backwards without a valid wrap")
        lost = delta - 1
        self.total_lost += lost
        self.previous = value
        return SequenceUpdate(True, delta, lost)


class U32TimeExtender:
    def __init__(self):
        self.previous = None
        self.extended = 0
        self.anomalies = 0

    def update(self, value):
        value &= 0xFFFFFFFF
        if self.previous is None:
            self.previous = value
            self.extended = value
            return self.extended
        delta = (value - self.previous) & 0xFFFFFFFF
        if delta == 0 or delta > 0x7FFFFFFF:
            self.anomalies += 1
            raise ValueError("device timestamp did not advance monotonically")
        self.extended += delta
        self.previous = value
        return self.extended


def load_csv_rows(path, include_excluded=False):
    with Path(path).open("r", newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    if not include_excluded:
        rows = [row for row in rows
                if row.get("excluded", "false").strip().lower() not in
                ("1", "true", "yes")]
    return rows
