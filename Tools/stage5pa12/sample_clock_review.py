#!/usr/bin/env python3
"""A12 opened-data 10 Hz replay; neither a product implementation nor holdout.

Device sample sequence advances the causal state machine. Host monotonic times
only map contemporaneous operator log markers and score the completed replay.
"""
from __future__ import annotations

import argparse
import csv
import json
import statistics
import sys
from collections import deque
from dataclasses import asdict, dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools/stage5pa11"))
from event_gated_review import A9, SHA, GatePolicy, clip, edges, rows, sha, short_scores  # noqa: E402


@dataclass(frozen=True)
class SamplePolicy:
    name: str
    boosted_ug_per_sample: int
    boost_samples: int
    prefill: bool = True
    baseline_ug_per_sample: int = 5
    hz: int = 10
    step_ug: int = 100_000
    obvious_step_ug: int = 2_000_000
    deadband_ug: int = 10_000
    offset_cap_ug: int = 500_000


POLICIES = (
    SamplePolicy("baseline_50", 5, 0, False),
    SamplePolicy("confirmed_350_5m_prefill", 35, 3000),
    SamplePolicy("confirmed_350_10m_prefill", 35, 6000),
    SamplePolicy("confirmed_350_10m_no_prefill", 35, 6000, False),
)


def samples(path: Path):
    """Read every captured sequence once; fail closed on clock or sequence gaps."""
    with path.open(encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream)
        previous = None
        for row in reader:
            current = (int(row["sample_sequence"]), int(row["timestamp_ms"]),
                       int(row["host_monotonic_ns"]), int(row["gross_ug"]))
            if previous is not None:
                if current[0] != previous[0] + 1:
                    raise ValueError("nonconsecutive device sample sequence")
                if not 0 < current[1] - previous[1] <= 250:
                    raise ValueError("device timestamp discontinuity")
                if current[2] <= previous[2]:
                    raise ValueError("host monotonic timestamp discontinuity")
            previous = current
            yield current


def operator_ns(path: Path):
    records = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()]
    pairs = []
    for prefix in ("LOAD_1", "UNLOAD_1", "LOAD_2", "UNLOAD_2"):
        begin = next(r for r in records if r["event"] == prefix + "_START")
        end = next(r for r in records if r["event"] == prefix + "_COMPLETE")
        start, finish = int(begin["host_monotonic_ns"]), int(end["host_monotonic_ns"])
        if start >= finish or (pairs and start < pairs[-1][1]):
            raise ValueError("operator markers overlap or are out of order")
        pairs.append((start, finish))
    return pairs


class SampleController:
    """Reference-error feedback; A13 may enable auto STATIC on a subclass policy."""

    def __init__(self, policy: SamplePolicy):
        self.p = policy
        self.offset = 0
        self.reference = None
        self.ref_samples: list[int] = []
        self.obs: deque[int] = deque(maxlen=20 * policy.hz)
        self.step_values: deque[int] = deque(maxlen=6 * policy.hz)
        self.step_sign = 0
        self.step_count = 0
        self.step_armed = True
        self.dosing = False
        self.last_seq = self.last_ms = self.last_mass = None
        self.hold_until = self.freeze_until = 0
        self.event_seq = self.obvious_seq = None
        self.pending_auto_seq = None
        self.pre_step_mass = None
        self.event_reference: list[int] = []
        self.event_observation: list[int] = []
        self.boost_until = 0
        self.gates: list[dict] = []
        self.rebases: list[dict] = []
        self.boost_samples = 0

    def reset_reference(self, seq: int, reason: str):
        self.reference = None
        self.ref_samples.clear()
        self.obs.clear()
        self.hold_until = seq + 15 * self.p.hz
        if reason != "INITIAL":
            self.rebases.append({"seq": seq, "reason": reason, "offset_ug": self.offset})

    def clear_step(self):
        self.step_values.clear()
        self.step_sign = self.step_count = 0
        self.step_armed = True

    def robust_step(self, mass: int) -> bool:
        self.step_values.append(mass)
        n = 3 * self.p.hz
        if len(self.step_values) < 2 * n:
            return False
        block = list(self.step_values)
        change = statistics.median(block[n:]) - statistics.median(block[:n])
        sign = 1 if change >= self.p.step_ug else -1 if change <= -self.p.step_ug else 0
        if sign == 0:
            self.step_sign = self.step_count = 0
            self.step_armed = True
            return False
        if not self.step_armed:
            return False
        if sign == self.step_sign:
            self.step_count += 1
        else:
            self.step_sign, self.step_count = sign, 1
        if self.step_count == 2:
            self.step_armed = False
            return True
        return False

    def feed(self, seq: int, ms: int, mass: int, dosing: bool):
        p = self.p
        auto_static = getattr(p, "auto_static", False)
        if self.last_seq is None:
            self.reset_reference(seq, "INITIAL")
        elif seq != self.last_seq + 1 or not 0 < ms - self.last_ms <= 250:
            self.reset_reference(seq, "TIME_GAP")
            self.clear_step()
            self.event_seq = self.obvious_seq = None
            self.pending_auto_seq = None
            self.pre_step_mass = None
            self.event_reference.clear()
            self.event_observation.clear()
            self.boost_until = 0
            self.freeze_until = seq

        if dosing and not self.dosing:
            self.reset_reference(seq, "DOSING_ENTRY")
            self.clear_step()
            self.event_seq = self.obvious_seq = None
            self.pending_auto_seq = None
            self.pre_step_mass = None
            self.event_reference.clear()
            self.event_observation.clear()
            self.boost_until = 0
        elif not dosing and self.dosing:
            age = seq - self.event_seq if self.event_seq is not None else None
            recent = age is not None and 0 <= age <= 120 * p.hz
            qualifies = recent or auto_static
            reason = ("CONFIRMED_EVENT" if recent else
                      "DOSING_EXIT_STATIC" if auto_static else "NO_RECENT_EVENT")
            self.reset_reference(seq, reason)
            self.clear_step()
            self.freeze_until = seq
            if qualifies:
                self.boost_until = (self.event_seq if recent else seq) + p.boost_samples
                if recent and p.prefill:
                    self.hold_until = self.event_seq + 15 * p.hz
                    self.ref_samples = list(self.event_reference)
                    if len(self.ref_samples) == 30 * p.hz:
                        self.reference = statistics.median(self.ref_samples)
                        self.obs = deque(self.event_observation, maxlen=20 * p.hz)
                record = {"event_detected_seq": self.event_seq,
                          "mode_confirmed_seq": seq, "age_samples": age,
                          "prefill_reference_count": len(self.ref_samples),
                          "offset_at_confirmation_ug": self.offset}
                if auto_static:
                    record["source"] = "DOSING_RECENT_STEP" if recent else "DOSING_EXIT"
                self.gates.append(record)
            else:
                self.boost_until = 0
            self.event_seq = self.obvious_seq = None
            self.pending_auto_seq = None
            self.pre_step_mass = None
            self.event_reference.clear()
            self.event_observation.clear()
        self.dosing = dosing

        if dosing:
            if self.last_mass is not None and abs(mass - self.last_mass) >= p.obvious_step_ug:
                self.obvious_seq = seq
            if self.robust_step(mass):
                if self.obvious_seq is not None and seq - self.obvious_seq <= 5 * p.hz:
                    self.event_seq = seq
                    self.event_reference.clear()
                    self.event_observation.clear()
            if self.event_seq is not None and p.prefill:
                age = seq - self.event_seq
                if 15 * p.hz <= age < 45 * p.hz:
                    self.event_reference.append(mass - self.offset)
                elif 45 * p.hz <= age < 65 * p.hz and len(self.event_reference) == 30 * p.hz:
                    self.event_observation.append(mass)
            state = "DOSING"
        else:
            if self.last_mass is not None and abs(mass - self.last_mass) >= p.obvious_step_ug:
                self.freeze_until = max(self.freeze_until, seq + 5 * p.hz)
                if auto_static:
                    if (self.pending_auto_seq is None
                            and (self.obvious_seq is None
                                 or seq - self.obvious_seq > 5 * p.hz)):
                        self.pre_step_mass = self.last_mass
                    self.obvious_seq = seq
                    self.boost_until = 0
            if self.robust_step(mass):
                recent_obvious = (self.obvious_seq is not None
                                  and 0 <= seq - self.obvious_seq <= 5 * p.hz)
                fast = auto_static and recent_obvious
                if fast:
                    self.reset_reference(seq, "STATIC_STEP_PENDING")
                    self.pending_auto_seq = seq
                else:
                    self.reset_reference(seq, "STATIC_STEP_BASELINE_ONLY")
                    self.pending_auto_seq = None
                    self.obvious_seq = None
                    self.pre_step_mass = None
                self.boost_until = 0
            if self.pending_auto_seq is not None:
                quiet = (self.obvious_seq is not None
                         and seq - self.obvious_seq >= 5 * p.hz)
                recent = list(self.step_values)[-3 * p.hz:]
                settled = (len(recent) == 3 * p.hz
                           and max(recent) - min(recent) <= 50_000)
                if quiet and settled:
                    net_change = abs(statistics.median(recent) - self.pre_step_mass)
                    if net_change >= p.obvious_step_ug:
                        self.reset_reference(seq, "STATIC_STEP_FAST")
                        self.hold_until = max(seq, self.pending_auto_seq + 15 * p.hz)
                        self.boost_until = self.pending_auto_seq + p.boost_samples
                        self.gates.append({"source": "STATIC_OBVIOUS_STEP",
                                           "event_detected_seq": self.pending_auto_seq,
                                           "settled_seq": seq,
                                           "obvious_step_seq": self.obvious_seq,
                                           "net_step_ug": net_change,
                                           "offset_at_detection_ug": self.offset})
                    else:
                        self.reset_reference(seq, "STATIC_STEP_RETURNED")
                    self.pending_auto_seq = self.obvious_seq = None
                    self.pre_step_mass = None
                elif seq - self.pending_auto_seq >= 30 * p.hz:
                    self.reset_reference(seq, "STATIC_STEP_UNSETTLED")
                    self.pending_auto_seq = self.obvious_seq = None
                    self.pre_step_mass = None
            if self.pending_auto_seq is not None:
                state = "STEP_SETTLING"
            elif seq < self.freeze_until:
                state = "STEP_PENDING"
            elif seq < self.hold_until:
                state = "HOLDOFF"
            elif len(self.ref_samples) < 30 * p.hz:
                self.ref_samples.append(mass - self.offset)
                state = "REFERENCE_FILL"
                if len(self.ref_samples) == 30 * p.hz:
                    self.reference = statistics.median(self.ref_samples)
            else:
                self.obs.append(mass)
                state = "OBSERVATION_FILL"
                if len(self.obs) == 20 * p.hz:
                    state = "TRACKING"
                    error = statistics.median(self.obs) - self.offset - self.reference
                    excess = max(0, abs(error) - p.deadband_ug)
                    boosted = seq < self.boost_until
                    rate = p.boosted_ug_per_sample if boosted else p.baseline_ug_per_sample
                    movement = int(min(excess, rate)) * (1 if error > 0 else -1)
                    self.offset = clip(self.offset + movement, p.offset_cap_ug)
                    if boosted:
                        self.boost_samples += 1
        self.last_seq, self.last_ms, self.last_mass = seq, ms, mass
        return mass - self.offset, self.offset, state


def replay(data, policy: SamplePolicy, intervals):
    controller = SampleController(policy)
    trace = []
    window: deque[tuple[int, int]] = deque()
    max10 = 0
    first_ns = None
    for seq, ms, host_ns, mass in data:
        if first_ns is None:
            first_ns = host_ns
        dosing = any(start <= host_ns < end for start, end in intervals)
        corrected, offset, state = controller.feed(seq, ms, mass, dosing)
        while window and ms - window[0][0] > 10_000:
            window.popleft()
        max10 = max(max10, (max(abs(offset - old) for _, old in window) if window else 0))
        window.append((ms, offset))
        trace.append(((host_ns - first_ns) / 1e9, corrected, offset, state))
    return trace, {"max_10s_offset_ug": max10, "gates": controller.gates,
                   "boost_samples": controller.boost_samples, "rebases": controller.rebases,
                   "final_offset_ug": controller.offset}


def synthetic_misdeclared_static():
    """Unannounced post-confirmation feed; still an invented safety scenario."""
    result = []
    for rate_ug_s in (100, 500, 2000):
        data = []
        mass = 0
        for tick in range(8500):
            if tick == 1300:
                mass += 500_000_000
            if 1600 <= tick < 7600 and ((tick - 1600) // 10) % 60 < 55:
                mass += rate_ug_s // 10
            data.append((tick, tick * 100, tick * 100_000_000, mass))
        for policy in POLICIES[1:3]:
            early, meta = replay(data, policy, ((100_000_000_000, 160_000_000_000),))
            held, _ = replay(data, policy, ((100_000_000_000, 760_000_000_000),))
            if len(meta["gates"]) != 1:
                raise AssertionError("synthetic marked load did not arm gate")
            actual = data[7599][3] - data[1599][3]
            result.append({"feed_rate_ug_s": rate_ug_s, "pause_s_per_minute": 5,
                           "policy": policy.name, "actual_added_ug": actual,
                           "premature_static_absorbed_ug": early[7599][2] - early[1599][2],
                           "continued_dosing_absorbed_ug": held[7599][2] - held[1599][2]})
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path,
                        default=ROOT / "Results/stage5pa12/sample_clock_review.json")
    args = parser.parse_args()
    source = A9 / "samples.csv"
    if sha(source) != SHA["a9"]:
        raise ValueError("A9 source hash mismatch")
    data = list(samples(source))
    marker_path = A9 / "events.jsonl"
    marks = operator_ns(marker_path)
    recorded = rows(source)
    physical_edges_for_scoring = edges(recorded)
    cases = {}
    for policy in POLICIES:
        trace, meta = replay(data, policy, marks)
        cases[policy.name] = {**meta, "scores": short_scores(trace, physical_edges_for_scoring)}
    output = {"classification": "OPENED_DEVELOPMENT_ONLY_NOT_HOLDOUT",
              "source_sha256": SHA["a9"], "operator_log_sha256": sha(marker_path),
              "samples": len(data),
              "clock": "device sample_sequence/timestamp_ms; operator markers aligned by host_monotonic_ns",
              "operator_logs_were_not_device_mode_commands": True,
              "physical_edges_scoring_only": True, "model": "new Python 10 Hz research controller, not frozen R5 or product C",
              "policies": {p.name: asdict(p) for p in POLICIES}, "cases": cases,
              "synthetic_misdeclared_static": synthetic_misdeclared_static()}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"rows": len(data), "gates": {n: len(c["gates"]) for n, c in cases.items()},
                      "output": str(args.output)}, indent=2))


if __name__ == "__main__":
    main()
