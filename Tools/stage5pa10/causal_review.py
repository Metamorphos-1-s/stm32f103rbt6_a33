#!/usr/bin/env python3
"""Opened-data, causal drift-controller comparison. Never touches a device.

The controller receives only the current and earlier one-second gross medians.
Operator mode changes, when used, come from the contemporaneous A9 event log.
No recorded physical edge is supplied to the controller; those edges are used
only for scoring after replay.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import statistics
import sys
from collections import deque
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools/stage5pa9"))
from a9_model_review import edges, rows, sec_series, trace_scores  # noqa: E402

A9 = ROOT / "Results/stage5pa9/20260927T060806Z_development_capture"
OLD = ROOT / "Results/stage5pa4/realtime_runs/20260925T121451Z_0x051C/samples.csv"
SHA = {
    "a9": "E683609BA0DB7EA090D101CE38569A432EC319982E7D41A9D2176A62B623128C",
    "older": "10545968A92A23688162A68D82DCFCF20C10FDD0ABDF3EC58203F6E549D0C3D2",
}


@dataclass(frozen=True)
class Policy:
    name: str
    early_rate_ug_s: int
    late_rate_ug_s: int = 50
    boost_s: int = 600
    holdoff_s: int = 15
    reference_s: int = 30
    observation_s: int = 20
    deadband_ug: int = 10_000
    offset_cap_ug: int = 500_000
    step_ug: int = 100_000  # Frozen R5 0.100 g; research does not change it.
    step_block_s: int = 3
    step_confirmations: int = 2
    immediate_freeze_ug: int = 2_000_000  # Provisional guard for obvious 500g transitions


POLICIES = (
    Policy("auto_50", 50),
    Policy("auto_100_first10m", 100),
    Policy("auto_250_first10m", 250),
    Policy("auto_350_first10m", 350),
    Policy("operator_250_first10m", 250),
)


def clip(value: int, bound: int) -> int:
    return max(-bound, min(bound, value))


class Controller:
    """Reference-error feedback with a causal fast-step or explicit DOSING gate."""

    def __init__(self, policy: Policy):
        self.p = policy
        self.offset = 0
        self.reference = None
        self.hold_until = policy.holdoff_s
        self.started_s = 0
        self.ref_samples: list[int] = []
        self.obs: deque[int] = deque(maxlen=policy.observation_s)
        self.last_s = None
        self.last_mass = None
        self.dosing = False
        self.rebases: list[dict] = []
        self.cap_samples = 0
        self.step_values: deque[int] = deque(maxlen=2 * policy.step_block_s)
        self.step_sign = 0
        self.step_confirm_count = 0
        self.step_armed = True
        self.freeze_until = 0

    def fast_step(self, mass: int) -> bool:
        """Keep frozen R5's two 3-second medians and two confirmations."""
        self.step_values.append(mass)
        n = self.p.step_block_s
        if len(self.step_values) < 2 * n:
            return False
        block = list(self.step_values)
        change = statistics.median(block[n:]) - statistics.median(block[:n])
        sign = 1 if change >= self.p.step_ug else -1 if change <= -self.p.step_ug else 0
        if not sign:
            self.step_sign = self.step_confirm_count = 0
            self.step_armed = True
            return False
        if not self.step_armed:
            return False
        if sign == self.step_sign:
            self.step_confirm_count += 1
        else:
            self.step_sign, self.step_confirm_count = sign, 1
        if self.step_confirm_count >= self.p.step_confirmations:
            self.step_armed = False
            return True
        return False

    def reset_reference(self, second: int, reason: str) -> None:
        self.reference = None
        self.started_s = second
        self.hold_until = second + self.p.holdoff_s
        self.ref_samples.clear()
        self.obs.clear()
        if reason != "INITIAL":
            self.rebases.append({"s": second, "reason": reason, "offset_ug": self.offset})

    def feed(self, second: int, mass: int, dosing: bool = False) -> tuple:
        if self.last_s is None:
            self.reset_reference(second, "INITIAL")
        elif second != self.last_s + 1:
            self.reset_reference(second, "TIME_GAP")
            self.step_values.clear()
            self.freeze_until = second

        if dosing and not self.dosing:
            self.reference = None
            self.ref_samples.clear()
            self.obs.clear()
            self.step_values.clear()
        if not dosing and self.dosing:
            self.reset_reference(second, "DOSING_EXIT")
            self.step_values.clear()
            self.freeze_until = second
        self.dosing = dosing

        # Use uncompensated mass and frozen R5's robust 0.1 g step rule.
        # A slow ramp can still be mistaken for drift: DOSING is the real guard.
        if (not dosing and self.last_mass is not None
                and abs(mass - self.last_mass) >= self.p.immediate_freeze_ug):
            self.freeze_until = max(self.freeze_until, second + 5)
        if not dosing and self.fast_step(mass):
            self.reset_reference(second, "FAST_STEP")

        if dosing:
            state = "DOSING"
        elif second < self.freeze_until:
            state = "STEP_PENDING"
        elif second < self.hold_until:
            state = "HOLDOFF"
        elif len(self.ref_samples) < self.p.reference_s:
            self.ref_samples.append(mass - self.offset)
            state = "REFERENCE_FILL"
            if len(self.ref_samples) == self.p.reference_s:
                self.reference = statistics.median(self.ref_samples)
        else:
            self.obs.append(mass)
            state = "OBSERVATION_FILL"
            if len(self.obs) == self.p.observation_s:
                state = "TRACKING"
                error = statistics.median(self.obs) - self.offset - self.reference
                excess = max(0, abs(error) - self.p.deadband_ug)
                rate = (self.p.early_rate_ug_s if second - self.started_s < self.p.boost_s
                        else self.p.late_rate_ug_s)
                movement = int(min(excess, rate)) * (1 if error > 0 else -1)
                self.offset = clip(self.offset + movement, self.p.offset_cap_ug)
        if abs(self.offset) == self.p.offset_cap_ug:
            self.cap_samples += 1
        self.last_s, self.last_mass = second, mass
        return (second, mass - self.offset, self.offset, state)


def replay(series, policy: Policy, intervals=()):
    model = Controller(policy)
    trace = []
    for second, mass in series:
        is_dosing = any(begin <= second < end for begin, end in intervals)
        trace.append(model.feed(second, mass, is_dosing))
    max10 = max((abs(row[2] - trace[max(0, i - 10)][2])
                 for i, row in enumerate(trace)), default=0)
    return trace, {"max_10s_offset_ug": max10, "rebases": model.rebases,
                   "final_offset_ug": model.offset, "at_cap_seconds": model.cap_samples}


def operator_intervals(path: Path, first_ns: int):
    records = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()]
    pairs = []
    for prefix in ("LOAD_1", "UNLOAD_1", "LOAD_2", "UNLOAD_2"):
        begin = next(r for r in records if r["event"] == prefix + "_START")
        end = next(r for r in records if r["event"] == prefix + "_COMPLETE")
        start_s = round((begin["host_monotonic_ns"] - first_ns) / 1e9)
        end_s = round((end["host_monotonic_ns"] - first_ns) / 1e9)
        if end_s <= start_s:
            raise ValueError("Invalid operator interval: " + prefix)
        pairs.append((start_s, end_s))
    return pairs


def sha(path: Path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest().upper()


def concise_scores(trace, event_list):
    scores = trace_scores(trace, event_list)
    return [{"kind": x["kind"], "duration_s": x["duration_s"],
             "early_corrected_g": x["early_corrected_g"],
             "end_corrected_g": x["phase_end5_corrected_g"],
             "span_early_g": x["span_early_g"],
             "relative_change_g": {m: (v.get("change_from_early_g") if v.get("status") is None else None)
                                    for m, v in x["checkpoints"].items()}}
            for x in scores]


def edge_alignment(rebases, event_list):
    detections = [x["s"] for x in rebases if x["reason"] == "FAST_STEP"]
    inferred = [round(x["center_s"]) for x in event_list]
    return {"inferred_edges_s": inferred, "detected_steps_s": detections,
            "signed_delay_s": [a - b for a, b in zip(detections, inferred)]
            if len(detections) == len(inferred) else None}


def one_dataset(path: Path, expected: str, manual_events: Path | None = None):
    actual = sha(path)
    if actual != expected:
        raise ValueError(f"CSV SHA mismatch: {path}: {actual} != {expected}")
    source = rows(path)
    series, event_list = sec_series(source), edges(source)
    intervals = operator_intervals(manual_events, source[0]["t"]) if manual_events else []
    out = {"csv_sha256": actual, "rows": len(source), "one_second_points": len(series),
           "physical_edges_for_scoring_only": event_list,
           "operator_intervals_for_mode_control": intervals, "policies": {}}
    for policy in POLICIES:
        if policy.name.startswith("operator_") and not intervals:
            continue
        trace, detail = replay(series, policy, intervals if policy.name.startswith("operator_") else ())
        out["policies"][policy.name] = {
            **detail, "scoring_only_edge_alignment": edge_alignment(detail["rebases"], event_list),
            "scores": concise_scores(trace, event_list)}
    return out


def rate_bound(phase):
    # A9 check at minute 5 is a 5–6 minute median, relative to the 15–45 s median.
    # 300 s is a representative midpoint interval; 345 s spans the full windows.
    g = phase["checkpoints"]["5"]["delta_from_early"]["gross_ug"]
    if g is None:
        return None
    return {"observed_first5m_change_ug": g,
            "max_50ug_s_300s_ug": 15_000,
            "max_50ug_s_345s_ug": 17_250,
            "nominal_ug_s_to_10mg_residual_over_300s": max(0, g - 10_000) / 300,
            "generous_ug_s_to_10mg_residual_over_345s": max(0, g - 10_000) / 345}


def slow_dosing_sensitivity():
    """Synthetic identical gross ramps: controller cannot infer intent from gross."""
    out = []
    for feed_ug_s in (100, 500, 2_000):
        for pause_s in (2, 5, 10):
            mass = 0
            series = []
            for second in range(840):
                if 120 <= second < 720 and (second - 120) % 60 < 60 - pause_s:
                    mass += feed_ug_s
                series.append((second, mass))
            for policy in (POLICIES[0], POLICIES[2]):
                auto, info = replay(series, policy)
                protected, pinf = replay(series, policy, ((120, 720),))
                actual = series[719][1] - series[119][1]
                auto_loss = auto[719][2] - auto[119][2]
                protected_loss = protected[719][2] - protected[119][2]
                out.append({"rate_ug_s": feed_ug_s, "pause_s": pause_s,
                            "policy": policy.name, "actual_added_ug": actual,
                            "unprotected_absorbed_ug": auto_loss,
                            "protected_absorbed_ug": protected_loss,
                            "unprotected_rebases": len(info["rebases"]),
                            "protected_rebases": len(pinf["rebases"])})
    return out


def main():
    arg = argparse.ArgumentParser()
    arg.add_argument("--output", type=Path, default=ROOT / "Results/stage5pa10/causal_review.json")
    opts = arg.parse_args()
    new = one_dataset(A9 / "samples.csv", SHA["a9"], A9 / "events.jsonl")
    old = one_dataset(OLD, SHA["older"])
    source = json.loads((A9 / "attribution_analysis.json").read_text(encoding="utf-8"))
    out = {"classification": "OPENED_DEVELOPMENT_ONLY_NOT_HOLDOUT",
           "firmware_modified": False, "device_accessed": False,
           "rate_50_ug_s_is_existing_R5_10s_500ug_limit": True,
           "rates_over_50_ug_s_are_sensitivity_only": True,
           "recording_and_replay_limit": "A9/A8 sampled 1-second host medians; no 10 Hz C parity or on-device proof",
           "a9": new, "older_opened_data": old,
           "a9_5min_rate_bound": [rate_bound(p) for p in source["phases"] if p["event"]["kind"] == "load"],
           "synthetic_slow_dosing": slow_dosing_sensitivity()}
    opts.output.parent.mkdir(parents=True, exist_ok=True)
    opts.output.write_text(json.dumps(out, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"a9_sha": new["csv_sha256"], "old_sha": old["csv_sha256"],
                      "a9_edges": len(new["physical_edges_for_scoring_only"]),
                      "scenarios": list(new["policies"]), "output": str(opts.output)}, indent=2))


if __name__ == "__main__":
    main()
