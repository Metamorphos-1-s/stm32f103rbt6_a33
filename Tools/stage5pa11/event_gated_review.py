#!/usr/bin/env python3
"""A11: causal feedback enabled by contemporaneous DOSING completion.

Research-only one-second replay. A9 physical edges are never control inputs;
they are loaded only after replay for scoring. This script never accesses hardware.
"""
from __future__ import annotations

import argparse
import json
import statistics
import sys
from collections import deque
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools/stage5pa10"))
from causal_review import (A9, OLD, SHA, Controller, Policy, clip, concise_scores,
                           edges, operator_intervals, rows, sec_series, sha)  # noqa: E402


@dataclass(frozen=True)
class GatePolicy:
    name: str
    boosted_rate_ug_s: int
    boost_s: int
    prefill: bool
    recent_event_s: int = 120
    baseline_rate_ug_s: int = 50


CASES = (
    GatePolicy("confirmed_250_5m_prefill", 250, 300, True),
    GatePolicy("confirmed_350_5m_prefill", 350, 300, True),
    GatePolicy("confirmed_350_10m_prefill", 350, 600, True),
    GatePolicy("confirmed_500_5m_prefill", 500, 300, True),
    GatePolicy("confirmed_350_10m_no_prefill", 350, 600, False),
)


class GatedController(Controller):
    """A10 position feedback; temporary higher rate requires operator and step evidence.

The controller must first observe both a robust step and an obvious single-second
change during explicit DOSING, then receive a timely DOSING -> STATIC confirmation.
Gross samples collected during DOSING may fill a provisional reference, but
offset cannot change before the confirmation. Auto steps in STATIC get only
the baseline rate.
"""

    def __init__(self, gate: GatePolicy):
        super().__init__(Policy(gate.name, gate.baseline_rate_ug_s))
        self.gate = gate
        self.event_s = None
        self.last_obvious_step_s = None
        self.event_reference: list[int] = []
        self.event_observation: list[int] = []
        self.boost_until = 0
        self.gate_events: list[dict] = []
        self.boost_seconds = 0

    def clear_step_history(self):
        self.step_values.clear()
        self.step_sign = 0
        self.step_confirm_count = 0
        self.step_armed = True

    def feed(self, second: int, mass: int, dosing: bool = False) -> tuple:
        if self.last_s is None:
            self.reset_reference(second, "INITIAL")
        elif second != self.last_s + 1:
            self.reset_reference(second, "TIME_GAP")
            self.clear_step_history()
            self.event_s = None
            self.last_obvious_step_s = None
            self.event_reference.clear()
            self.event_observation.clear()
            self.boost_until = 0
            self.freeze_until = second

        if dosing and not self.dosing:
            self.reference = None
            self.ref_samples.clear()
            self.obs.clear()
            self.clear_step_history()
            self.event_s = None
            self.last_obvious_step_s = None
            self.event_reference.clear()
            self.event_observation.clear()
            self.boost_until = 0
        elif not dosing and self.dosing:
            event_age = second - self.event_s if self.event_s is not None else None
            qualifies = event_age is not None and 0 <= event_age <= self.gate.recent_event_s
            self.reset_reference(second, "CONFIRMED_EVENT" if qualifies else "NO_RECENT_EVENT")
            self.clear_step_history()
            self.freeze_until = second
            if qualifies:
                self.boost_until = self.event_s + self.gate.boost_s
                if self.gate.prefill:
                    self.hold_until = self.event_s + self.p.holdoff_s
                    self.ref_samples = list(self.event_reference)
                    if len(self.ref_samples) == self.p.reference_s:
                        self.reference = statistics.median(self.ref_samples)
                        self.obs = deque(self.event_observation, maxlen=self.p.observation_s)
                self.gate_events.append({"event_detected_s": self.event_s,
                                         "mode_confirmed_s": second,
                                         "age_at_confirmation_s": event_age,
                                         "prefill_reference_count": len(self.ref_samples),
                                         "prefill_observation_count": len(self.obs),
                                         "boost_until_s": self.boost_until,
                                         "offset_ug": self.offset})
            else:
                self.boost_until = 0
            self.event_s = None
            self.last_obvious_step_s = None
            self.event_reference.clear()
            self.event_observation.clear()
        self.dosing = dosing

        if dosing:
            if (self.last_mass is not None
                    and abs(mass - self.last_mass) >= self.p.immediate_freeze_ug):
                self.last_obvious_step_s = second
            if self.fast_step(mass):
                if (self.last_obvious_step_s is not None
                        and 0 <= second - self.last_obvious_step_s <= 5):
                    self.event_s = second
                    self.event_reference.clear()
                    self.event_observation.clear()
            if self.event_s is not None and self.gate.prefill:
                age = second - self.event_s
                if 15 <= age < 45:
                    self.event_reference.append(mass - self.offset)
                elif 45 <= age < 65 and len(self.event_reference) == self.p.reference_s:
                    self.event_observation.append(mass)
            state = "DOSING"
        else:
            if (self.last_mass is not None
                    and abs(mass - self.last_mass) >= self.p.immediate_freeze_ug):
                self.freeze_until = max(self.freeze_until, second + 5)
            if self.fast_step(mass):
                self.reset_reference(second, "FAST_STEP_BASELINE_ONLY")
                self.boost_until = 0
            if second < self.freeze_until:
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
                    boost = second < self.boost_until
                    rate = self.gate.boosted_rate_ug_s if boost else self.gate.baseline_rate_ug_s
                    movement = int(min(excess, rate)) * (1 if error > 0 else -1)
                    self.offset = clip(self.offset + movement, self.p.offset_cap_ug)
                    if boost:
                        self.boost_seconds += 1
        if abs(self.offset) == self.p.offset_cap_ug:
            self.cap_samples += 1
        self.last_s, self.last_mass = second, mass
        return (second, mass - self.offset, self.offset, state)


def replay(series, gate: GatePolicy, intervals=()):
    controller = GatedController(gate)
    trace = []
    for second, gross_ug in series:
        dosing = any(start <= second < end for start, end in intervals)
        trace.append(controller.feed(second, gross_ug, dosing))
    max10 = max((abs(row[2] - trace[max(0, i - 10)][2])
                 for i, row in enumerate(trace)), default=0)
    return trace, {"max_10s_offset_ug": max10, "gates": controller.gate_events,
                   "boost_seconds": controller.boost_seconds,
                   "rebases": controller.rebases, "at_cap_seconds": controller.cap_samples,
                   "final_offset_ug": controller.offset}


def short_scores(trace, event_list):
    out = concise_scores(trace, event_list)
    for s in out:
        s["relative_change_g"] = {k: s["relative_change_g"].get(k)
                                  for k in ("1", "2", "5", "10", "30", "45")}
    return out


def a9_review():
    source = A9 / "samples.csv"
    if sha(source) != SHA["a9"]:
        raise ValueError("A9 CSV hash mismatch")
    samples = rows(source)
    series = sec_series(samples)
    interval = operator_intervals(A9 / "events.jsonl", samples[0]["t"])
    event_list = edges(samples)
    cases = {}
    for gate in CASES:
        trace, detail = replay(series, gate, interval)
        cases[gate.name] = {**detail, "scores": short_scores(trace, event_list)}
    return {"sha256": SHA["a9"], "rows": len(samples), "seconds": len(series),
            "operator_intervals_are_log_only": interval,
            "inferred_physical_edges_scoring_only": event_list, "cases": cases}


def old_no_operator_review():
    """No event log exists in this opened file; no gate can legitimately arm."""
    if sha(OLD) != SHA["older"]:
        raise ValueError("Older CSV hash mismatch")
    samples = rows(OLD)
    series = sec_series(samples)
    trace, detail = replay(series, CASES[2], ())
    event_list = edges(samples)
    scores = short_scores(trace, event_list)
    index = max((i for i, s in enumerate(scores) if s["kind"] == "load"),
                key=lambda i: scores[i]["duration_s"])
    return {"sha256": SHA["older"], "manual_markers_available": False,
            "gates": detail["gates"], "max_10s_offset_ug": detail["max_10s_offset_ug"],
            "longest_load": scores[index], "following_unload": scores[index + 1]}


def synthetic_adversarial():
    """Misdeclared STATIC after an abrupt load followed by unannounced feeding."""
    out = []
    for rate in (100, 500, 2_000):
        for pause in (2, 5, 10):
            gross = 0
            series = []
            for second in range(850):
                if second == 130:
                    gross += 500_000_000
                # User claims complete at 160 but material keeps entering for 10m.
                if 160 <= second < 760 and (second - 160) % 60 < 60 - pause:
                    gross += rate
                series.append((second, gross))
            for gate in (CASES[1], CASES[2]):
                trace, detail = replay(series, gate, ((100, 160),))
                held_dosing, _ = replay(series, gate, ((100, 760),))
                assert len(detail["gates"]) == 1
                absorbed = trace[759][2] - trace[159][2]
                actual = series[759][1] - series[159][1]
                out.append({"feed_rate_ug_s": rate, "pause_s": pause,
                            "case": gate.name, "actual_added_ug": actual,
                            "misdeclared_static_absorbed_ug": absorbed,
                            "continued_dosing_absorbed_ug": held_dosing[759][2] - held_dosing[159][2],
                            "absorbed_fraction": absorbed / actual})
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path,
                        default=ROOT / "Results/stage5pa11/event_gated_review.json")
    args = parser.parse_args()
    a9 = a9_review()
    old = old_no_operator_review()
    out = {"classification": "OPENED_DEVELOPMENT_ONLY_NOT_HOLDOUT",
           "product_r5_modified": False, "device_accessed": False,
           "controller_input": "one-second host gross medians and real-time operator log hypotheticals",
           "historical_operator_logs_were_not_device_mode_commands": True,
           "feedback_rate_over_50ug_s_is_not_qualified": True,
           "a9": a9, "older_opened_record": old,
           "misdeclared_static_synthetic": synthetic_adversarial()}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(out, ensure_ascii=False, indent=2) + "\n",
                           encoding="utf-8")
    print(json.dumps({"a9_rows": a9["rows"], "cases": list(a9["cases"]),
                      "old_gates": len(old["gates"]), "output": str(args.output)}, indent=2))


if __name__ == "__main__":
    main()
