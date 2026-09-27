#!/usr/bin/env python3
"""A13 research-only 10 Hz STATIC auto-step boost; no target device access."""
from __future__ import annotations

import argparse
import json
import sys
from dataclasses import asdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools/stage5pa12"))
from sample_clock_review import (A9, SHA, SamplePolicy, edges, replay, rows, samples,
                                 sha, short_scores)  # noqa: E402


class AutoStaticPolicy(SamplePolicy):
    """Opt in to A13 only; base A12 policy and its archived result stay identical."""
    auto_static = True


POLICIES = (
    AutoStaticPolicy("auto_static_250_5m", 25, 3000),
    AutoStaticPolicy("auto_static_350_5m", 35, 3000),
    AutoStaticPolicy("auto_static_350_10m", 35, 6000),
)


def a9_review():
    source = A9 / "samples.csv"
    if sha(source) != SHA["a9"]:
        raise ValueError("A9 CSV hash mismatch")
    data = list(samples(source))
    # Human event markers do not enter this controller. Physical edges score only.
    event_list = edges(rows(source))
    cases = {}
    for policy in POLICIES:
        trace, meta = replay(data, policy, ())
        cases[policy.name] = {**meta, "scores": short_scores(trace, event_list)}
    return {"source_sha256": SHA["a9"], "records": len(data),
            "operator_mode_switches_supplied": 0,
            "physical_edges_scoring_only": event_list,
            "policies": {p.name: {**asdict(p), "auto_static": p.auto_static} for p in POLICIES},
            "cases": cases}


def synthetic_continuous_feed():
    """User keeps STATIC after a 500g step vs explicit DOSING for all feeding."""
    result = []
    for rate_ug_s in (500, 2000, 10_000):
        data = []
        mass = 0
        for tick in range(8500):
            if tick == 1300:
                mass += 500_000_000
            if 1600 <= tick < 7600 and ((tick - 1600) // 10) % 60 < 55:
                mass += rate_ug_s // 10
            data.append((tick, tick * 100, tick * 100_000_000, mass))
        for policy in POLICIES[1:]:
            static, meta = replay(data, policy, ())
            protected, guard = replay(data, policy, ((100_000_000_000, 760_000_000_000),))
            actual = data[7599][3] - data[1599][3]
            result.append({"rate_ug_s": rate_ug_s, "pause_s_per_minute": 5,
                           "policy": policy.name, "actual_added_ug": actual,
                           "static_absorbed_ug": static[7599][2] - static[1599][2],
                           "dosing_absorbed_ug": protected[7599][2] - protected[1599][2],
                           "static_gate_sources": [g["source"] for g in meta["gates"]],
                           "dosing_return_gate_sources": [g["source"] for g in guard["gates"]]})
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path,
                        default=ROOT / "Results/stage5pa13/auto_static_review.json")
    args = parser.parse_args()
    a9 = a9_review()
    result = {"classification": "OPENED_DEVELOPMENT_NOT_HOLDOUT",
              "device_accessed": False, "product_r5_modified": False,
              "controller_input": "continuous 10 Hz gross and actual sample clock; no operator markers",
              "r5_off_at_capture": True, "a9": a9,
              "synthetic_continuous_feed": synthetic_continuous_feed()}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n",
                           encoding="utf-8")
    print(json.dumps({"a9_records": a9["records"],
                      "gates": {name: len(case["gates"]) for name, case in a9["cases"].items()},
                      "output": str(args.output)}, indent=2))


if __name__ == "__main__":
    main()
