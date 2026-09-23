#!/usr/bin/env python3
"""Frozen Stage 5M-R5 reference-lock engineering-beta model."""
from __future__ import annotations

import csv
import statistics
from dataclasses import dataclass
from enum import IntEnum
from pathlib import Path


class Mode(IntEnum):
    OFF = 0
    DOSING_NO_COMPENSATION = 1
    STATIC_COMPENSATION = 2


class State(IntEnum):
    OFF = 0
    DOSING = 1
    HOLDOFF = 2
    REFERENCE_FILL = 3
    OBSERVATION_FILL = 4
    TRACKING = 5
    LIMITED = 6


class Reason(IntEnum):
    NONE = 0
    MODE_CHANGE = 1
    AUTOMATIC_STEP = 2
    ZERO = 3
    CALIBRATION = 4
    PROFILE = 5
    INVALID_CALIBRATION = 6
    FAULT = 7
    OVERLOAD = 8
    NEAR_RAIL = 9
    SEQUENCE = 10
    TIMESTAMP = 11
    NUMERIC = 12
    OFFSET_LIMIT = 13


@dataclass(frozen=True)
class Config:
    reference_window_s: int = 300
    observation_window_s: int = 600
    evaluation_period_s: int = 60
    holdoff_s: int = 15
    deadband_ug: int = 10_000
    time_constant_s: int = 900
    max_rate_ug_per_s: int = 50
    max_offset_ug: int = 500_000
    step_block_s: int = 3
    step_threshold_ug: int = 100_000
    step_confirmations: int = 2
    robust_block_s: int = 10


CONFIG = Config()


def median_int(values):
    ordered = sorted(values)
    size = len(ordered)
    middle = size // 2
    if size & 1:
        return ordered[middle]
    total_value = ordered[middle - 1] + ordered[middle]
    return total_value // 2 if total_value >= 0 else -((-total_value) // 2)


def trunc_div(numerator, denominator):
    if denominator <= 0:
        raise ValueError("positive denominator required")
    return numerator // denominator if numerator >= 0 else -((-numerator) // denominator)


class ReferenceLock:
    def __init__(self, config=CONFIG):
        self.c = config
        self.mode = Mode.OFF
        self.state = State.OFF
        self.offset_ug = 0
        self.offset_milli_ug = 0
        self.reference_ug = 0
        self.current_window_ug = 0
        self.reference_error_ug = 0
        self.correction_rate_ug_per_s = 0
        self.holdoff_remaining = 0
        self.reference_values = []
        self.observation_values = []
        self.robust_block_values = []
        self.step_values = []
        self.step_sign = 0
        self.step_count = 0
        self.step_armed = True
        self.automatic_rebase_count = 0
        self.last_rebase_reason = Reason.NONE
        self.limited = False
        self.evaluation_count = 0
        self.last_evaluation_second = None
        self.have_sample = False
        self.last_sample_sequence = 0
        self.last_timestamp_ms = 0
        self.second_bucket_ms = 0
        self.logical_second = 0
        self.second_samples = []
        self.second_slot = None

    def snapshot(self, uncompensated_gross_ug):
        applied = 0 if self.mode == Mode.OFF else self.offset_ug
        reference_blocks = self.c.reference_window_s // self.c.robust_block_s
        reference_fill = len(self.reference_values) * self.c.robust_block_s
        if len(self.reference_values) < reference_blocks:
            reference_fill += len(self.robust_block_values)
        observation_partial = 0 if len(self.reference_values) < reference_blocks else len(self.robust_block_values)
        return {
            "mode": int(self.mode),
            "state": int(self.state),
            "uncompensated_gross_ug": uncompensated_gross_ug,
            "corrected_gross_ug": uncompensated_gross_ug - applied,
            "offset_ug": self.offset_ug,
            "reference_ug": self.reference_ug,
            "current_window_ug": self.current_window_ug,
            "reference_error_ug": self.reference_error_ug,
            "correction_rate_ug_per_s": self.correction_rate_ug_per_s,
            "holdoff_remaining": self.holdoff_remaining,
            "reference_fill": reference_fill,
            "observation_fill": min(self.c.observation_window_s,
                len(self.observation_values) * self.c.robust_block_s + observation_partial),
            "automatic_rebase_count": self.automatic_rebase_count,
            "last_rebase_reason": int(self.last_rebase_reason),
            "limited": int(self.limited),
            "evaluation_count": self.evaluation_count,
        }

    def clear_learning(self, reason, holdoff=True):
        self.correction_rate_ug_per_s = 0
        self.reference_values.clear()
        self.observation_values.clear()
        self.robust_block_values.clear()
        self.step_values.clear()
        self.step_sign = 0
        self.step_count = 0
        self.reference_ug = 0
        self.current_window_ug = 0
        self.reference_error_ug = 0
        self.last_evaluation_second = None
        self.last_rebase_reason = reason
        self.holdoff_remaining = self.c.holdoff_s if holdoff else 0
        self.state = State.HOLDOFF if holdoff else State.REFERENCE_FILL

    def reset(self, reason=Reason.ZERO):
        self.offset_ug = 0
        self.offset_milli_ug = 0
        self.clear_learning(reason, self.mode == Mode.STATIC_COMPENSATION)
        if self.mode == Mode.OFF:
            self.state = State.OFF
        elif self.mode == Mode.DOSING_NO_COMPENSATION:
            self.state = State.DOSING

    def set_mode(self, mode):
        mode = Mode(mode)
        if mode == self.mode:
            return
        self.mode = mode
        if mode == Mode.OFF:
            self.offset_ug = 0
            self.offset_milli_ug = 0
            self.clear_learning(Reason.MODE_CHANGE, False)
            self.state = State.OFF
        elif mode == Mode.DOSING_NO_COMPENSATION:
            self.correction_rate_ug_per_s = 0
            self.reference_values.clear()
            self.observation_values.clear()
            self.robust_block_values.clear()
            self.step_values.clear()
            self.state = State.DOSING
            self.last_rebase_reason = Reason.MODE_CHANGE
        else:
            self.clear_learning(Reason.MODE_CHANGE, True)
        self.have_sample = False
        self.second_samples.clear()
        self.second_slot = None

    def limit(self, reason):
        self.limited = True
        self.correction_rate_ug_per_s = 0
        self.state = State.LIMITED
        self.last_rebase_reason = reason

    def profile_change(self):
        self.limited = False
        self.have_sample = False
        self.second_samples.clear()
        self.second_slot = None
        self.clear_learning(Reason.PROFILE,
            self.mode == Mode.STATIC_COMPENSATION)
        if self.mode == Mode.OFF:
            self.state = State.OFF
        elif self.mode == Mode.DOSING_NO_COMPENSATION:
            self.state = State.DOSING

    def _step(self, value):
        self.step_values.append(value)
        block = self.c.step_block_s
        self.step_values = self.step_values[-2 * block :]
        if len(self.step_values) < 2 * block:
            return False
        delta = median_int(self.step_values[block:]) - median_int(self.step_values[:block])
        sign = 1 if delta >= self.c.step_threshold_ug else -1 if delta <= -self.c.step_threshold_ug else 0
        if sign == 0:
            self.step_count = 0
            self.step_sign = 0
            self.step_armed = True
            return False
        if not self.step_armed:
            return False
        if sign == self.step_sign:
            self.step_count += 1
        else:
            self.step_sign = sign
            self.step_count = 1
        if self.step_count >= self.c.step_confirmations:
            self.step_armed = False
            return True
        return False

    def process_second(self, second, uncompensated_gross_ug, *, valid=True,
                       fault=False, overload=False, near_rail=False):
        if self.mode == Mode.OFF:
            return self.snapshot(uncompensated_gross_ug)
        if not valid or fault or overload or near_rail:
            reason = Reason.INVALID_CALIBRATION if not valid else Reason.FAULT if fault else Reason.OVERLOAD if overload else Reason.NEAR_RAIL
            self.limit(reason)
            return self.snapshot(uncompensated_gross_ug)
        if self.limited:
            return self.snapshot(uncompensated_gross_ug)
        if self.mode == Mode.DOSING_NO_COMPENSATION:
            self.state = State.DOSING
            self.correction_rate_ug_per_s = 0
            return self.snapshot(uncompensated_gross_ug)

        if self._step(uncompensated_gross_ug):
            self.automatic_rebase_count += 1
            self.clear_learning(Reason.AUTOMATIC_STEP, True)

        if self.holdoff_remaining:
            self.holdoff_remaining -= 1
            self.state = State.HOLDOFF
            return self.snapshot(uncompensated_gross_ug)

        corrected = uncompensated_gross_ug - self.offset_ug
        reference_blocks = self.c.reference_window_s // self.c.robust_block_s
        observation_blocks = self.c.observation_window_s // self.c.robust_block_s
        if len(self.reference_values) < reference_blocks:
            self.robust_block_values.append(corrected)
            self.state = State.REFERENCE_FILL
            if len(self.robust_block_values) == self.c.robust_block_s:
                self.reference_values.append(median_int(self.robust_block_values))
                self.robust_block_values.clear()
            if len(self.reference_values) == reference_blocks:
                self.reference_ug = median_int(self.reference_values)
                self.observation_values.clear()
                self.state = State.OBSERVATION_FILL
            return self.snapshot(uncompensated_gross_ug)

        self.robust_block_values.append(uncompensated_gross_ug)
        if len(self.robust_block_values) == self.c.robust_block_s:
            self.observation_values.append(median_int(self.robust_block_values))
            self.observation_values = self.observation_values[-observation_blocks:]
            self.robust_block_values.clear()
        if len(self.observation_values) < observation_blocks:
            self.state = State.OBSERVATION_FILL
            return self.snapshot(uncompensated_gross_ug)

        self.state = State.TRACKING
        if self.last_evaluation_second is None or second - self.last_evaluation_second >= self.c.evaluation_period_s:
            self.current_window_ug = median_int(self.observation_values) - self.offset_ug
            self.reference_error_ug = self.current_window_ug - self.reference_ug
            magnitude = abs(self.reference_error_ug)
            if magnitude <= self.c.deadband_ug:
                self.correction_rate_ug_per_s = 0
            else:
                excess = magnitude - self.c.deadband_ug
                rate_milli_ug = trunc_div(excess * 1000, self.c.time_constant_s)
                rate_milli_ug = min(rate_milli_ug, self.c.max_rate_ug_per_s * 1000)
                self.correction_rate_ug_per_s = (rate_milli_ug if self.reference_error_ug > 0 else -rate_milli_ug) / 1000
            self.last_evaluation_second = second
            self.evaluation_count += 1

        update_milli_ug = round(self.correction_rate_ug_per_s * 1000)
        target = self.offset_milli_ug + update_milli_ug
        if target > self.c.max_offset_ug * 1000:
            target = self.c.max_offset_ug * 1000
            self.last_rebase_reason = Reason.OFFSET_LIMIT
        elif target < -self.c.max_offset_ug * 1000:
            target = -self.c.max_offset_ug * 1000
            self.last_rebase_reason = Reason.OFFSET_LIMIT
        self.offset_milli_ug = target
        self.offset_ug = trunc_div(self.offset_milli_ug, 1000)
        return self.snapshot(uncompensated_gross_ug)

    def process_sample(self, sequence, timestamp_ms, uncompensated_gross_ug,
                       *, valid=True, fault=False, overload=False,
                       near_rail=False):
        sequence &= 0xFFFFFFFF
        timestamp_ms &= 0xFFFFFFFF
        if self.have_sample:
            if ((sequence - self.last_sample_sequence) & 0xFFFFFFFF) != 1:
                self.limit(Reason.SEQUENCE)
            sample_elapsed = ((timestamp_ms - self.last_timestamp_ms) &
                              0xFFFFFFFF)
            if sample_elapsed == 0 or sample_elapsed > 250:
                self.limit(Reason.TIMESTAMP)
        else:
            self.second_bucket_ms = timestamp_ms
            self.have_sample = True
        elapsed = ((timestamp_ms - self.second_bucket_ms) & 0xFFFFFFFF)
        if elapsed >= 1000 and self.second_samples:
            self.logical_second = (self.logical_second + 1) & 0xFFFFFFFF
            self.process_second(self.logical_second,
                median_int(self.second_samples), valid=valid, fault=fault,
                overload=overload, near_rail=near_rail)
            self.second_bucket_ms = timestamp_ms
            self.second_samples.clear()
            self.second_slot = None
            elapsed = 0
        slot = elapsed // 100
        if self.second_slot != slot:
            if len(self.second_samples) >= 16:
                self.limit(Reason.TIMESTAMP)
            else:
                self.second_samples.append(uncompensated_gross_ug)
                self.second_slot = slot
        elif self.second_samples:
            self.second_samples[-1] = uncompensated_gross_ug
        self.last_timestamp_ms = timestamp_ms
        self.last_sample_sequence = sequence
        return self.snapshot(uncompensated_gross_ug)


def csv_seconds(path, start_utc=None):
    with Path(path).open(encoding="utf-8", newline="") as stream:
        rows = list(csv.DictReader(stream))
    if start_utc is not None:
        rows = [row for row in rows if row["utc"] >= start_utc]
    origin = int(rows[0]["mcu_uptime_ms"])
    buckets = {}
    for row in rows:
        second = (int(row["mcu_uptime_ms"]) - origin) // 1000
        buckets.setdefault(second, []).append(int(row["uncompensated_gross_ug"]))
    return [(second, median_int(values)) for second, values in sorted(buckets.items())]


def ols_g_per_h(rows):
    if len(rows) < 2:
        return 0.0
    xm = statistics.fmean(row[0] for row in rows)
    ym = statistics.fmean(row[1] for row in rows)
    denominator = sum((row[0] - xm) ** 2 for row in rows)
    return sum((row[0] - xm) * (row[1] - ym) for row in rows) / denominator * 3600 / 1e6
