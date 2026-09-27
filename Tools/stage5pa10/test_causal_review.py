"""Focused invariants for the A10 offline research controller."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from causal_review import Controller, Policy, replay, slow_dosing_sensitivity


def test_prefix_causality_and_no_pre_event_reference():
    series = [(s, s * 2000 if s < 100 else 500_200_000 + (s - 100) * 1000)
              for s in range(400)]
    policy = Policy("test", 50)
    full, info = replay(series, policy)
    for cutoff in (99, 101, 130, 160, 250):
        prefix, _ = replay(series[:cutoff], policy)
        assert full[:cutoff] == prefix  # no future samples enter control
    assert len([e for e in info["rebases"] if e["reason"] == "FAST_STEP"]) == 1
    # At the 500 g physical step, previously learned offset is preserved.
    assert full[99][2] > 0
    assert full[100][2] == full[99][2]
    assert full[100][1] - full[99][1] == series[100][1] - series[99][1]
    assert full[129][3] == "REFERENCE_FILL"
    assert full[164][3] == "OBSERVATION_FILL" or full[164][3] == "TRACKING"


def test_dosing_freeze_and_rebuild():
    series = [(s, s * 200 if s < 150 else 500_000_000 + (s - 150) * 500)
              for s in range(400)]
    trace, detail = replay(series, Policy("test", 250), ((120, 230),))
    assert len({r[2] for r in trace[120:230]}) == 1
    assert all(r[3] == "DOSING" for r in trace[120:230])
    assert any(r["reason"] == "DOSING_EXIT" and r["s"] == 230 for r in detail["rebases"])
    assert not any(r["reason"] == "FAST_STEP" and 120 <= r["s"] < 230
                   for r in detail["rebases"])
    assert trace[150][1] - trace[149][1] == series[150][1] - series[149][1]


def test_rate_cap_and_position_feedback():
    series = [(s, 0 if s < 100 else 500_000_000 + min(s - 100, 600) * 1000)
              for s in range(2000)]
    for rate in (50, 100, 250, 350):
        trace, detail = replay(series, Policy("rate", rate))
        assert detail["max_10s_offset_ug"] <= 10 * rate
        assert max(abs(r[2]) for r in trace) <= 500_000
        assert trace[-1][2] > trace[130][2]


def test_identical_gross_cannot_distinguish_slow_load_from_drift():
    series = [(s, s * 100) for s in range(840)]
    load, _ = replay(series, Policy("test", 50))
    thermal, _ = replay(series, Policy("test", 50))
    assert load == thermal
    rows = slow_dosing_sensitivity()
    assert all(r["protected_absorbed_ug"] == 0 for r in rows)
    assert any(r["unprotected_absorbed_ug"] > 0 for r in rows)


if __name__ == "__main__":
    for test in (test_prefix_causality_and_no_pre_event_reference,
                 test_dosing_freeze_and_rebuild,
                 test_rate_cap_and_position_feedback,
                 test_identical_gross_cannot_distinguish_slow_load_from_drift):
        test()
    print("A10 CAUSAL CONTROLLER INVARIANTS 4/4 PASS")
