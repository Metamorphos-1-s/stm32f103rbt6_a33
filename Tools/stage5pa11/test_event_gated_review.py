"""A11 safety and causality checks, independent of the A9 physical edge list."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from event_gated_review import GatePolicy, replay, synthetic_adversarial


def example():
    seq = []
    for s in range(460):
        mass = s * 2000 if s < 130 else 500_260_000 + (s - 130) * 500
        seq.append((s, mass))
    return seq


def test_causality_nonzero_offset_and_dosing_freeze():
    seq = example()
    policy = GatePolicy("prefill", 350, 600, True)
    full, detail = replay(seq, policy, ((100, 190),))
    for cutoff in (90, 131, 145, 169, 185, 210, 330):
        prefix, _ = replay(seq[:cutoff], policy, ((100, 190),))
        assert full[:cutoff] == prefix
    assert full[99][2] > 0
    assert len({row[2] for row in full[100:190]}) == 1
    assert all(row[3] == "DOSING" for row in full[100:190])
    assert full[130][1] - full[129][1] == seq[130][1] - seq[129][1]
    assert len(detail["gates"]) == 1
    gate = detail["gates"][0]
    assert gate["mode_confirmed_s"] == 190
    assert 0 <= gate["age_at_confirmation_s"] <= 120
    assert gate["prefill_reference_count"] == 30
    assert gate["prefill_observation_count"] > 0
    assert detail["max_10s_offset_ug"] <= 3500


def test_no_step_no_boost_and_stale_step_no_boost():
    policy = GatePolicy("stale", 500, 600, True)
    seq = [(s, 0 if s < 130 else 500_000_000 + (s - 130) * 1000)
           for s in range(850)]
    no_step, detail = replay(seq, policy, ((25, 100),))
    assert not detail["gates"] and detail["boost_seconds"] == 0
    assert detail["max_10s_offset_ug"] <= 500
    stale, detail = replay(seq, policy, ((100, 450),))
    assert not detail["gates"] and detail["boost_seconds"] == 0
    assert detail["max_10s_offset_ug"] <= 500
    assert all(row[2] == stale[100][2] for row in stale[100:450])


def test_unexpected_static_step_stays_at_baseline_rate():
    policy = GatePolicy("fallback", 500, 600, True)
    trace, detail = replay(example(), policy)
    assert not detail["gates"] and detail["boost_seconds"] == 0
    assert any(e["reason"] == "FAST_STEP_BASELINE_ONLY" for e in detail["rebases"])
    assert detail["max_10s_offset_ug"] <= 500


def test_operator_without_prefill_does_not_read_future():
    seq = example()
    policy = GatePolicy("no-prefill", 350, 600, False)
    full, detail = replay(seq, policy, ((100, 170),))
    prefix, _ = replay(seq[:180], policy, ((100, 170),))
    assert prefix == full[:180]
    assert len(detail["gates"]) == 1
    assert detail["gates"][0]["prefill_reference_count"] == 0
    assert detail["gates"][0]["prefill_observation_count"] == 0
    assert detail["max_10s_offset_ug"] <= 3500


def test_false_static_confirmation_changes_actual_slow_weight():
    scenarios = synthetic_adversarial()
    assert all(s["continued_dosing_absorbed_ug"] == 0 for s in scenarios)
    assert all(s["misdeclared_static_absorbed_ug"] > 0 for s in scenarios)
    assert len(scenarios) == 18


def test_missing_second_cancels_provisional_event():
    seq = [(s, 0 if s < 130 else 500_000_000)
           for s in range(260) if not 155 <= s < 162]
    trace, detail = replay(seq, GatePolicy("gap", 350, 600, True), ((100, 190),))
    assert len(trace) == len(seq)
    assert not detail["gates"] and detail["boost_seconds"] == 0
    assert any(e["reason"] == "TIME_GAP" for e in detail["rebases"])
    assert all(row[2] == trace[100][2] for row in trace if 100 <= row[0] < 190)


def test_small_step_during_dosing_cannot_authorize_fast_rate():
    # The legacy 0.1 g rebase detector must not alone authorize a 7x rate.
    seq = [(s, 0 if s < 130 else 150_000 + (s - 130) * 150)
           for s in range(850)]
    trace, detail = replay(seq, GatePolicy("small-step", 350, 600, True), ((100, 190),))
    assert len(trace) == len(seq)
    assert not detail["gates"] and detail["boost_seconds"] == 0
    assert detail["max_10s_offset_ug"] <= 500
    assert len({row[2] for row in trace[100:190]}) == 1


if __name__ == "__main__":
    for test in (test_causality_nonzero_offset_and_dosing_freeze,
                 test_no_step_no_boost_and_stale_step_no_boost,
                 test_unexpected_static_step_stays_at_baseline_rate,
                 test_operator_without_prefill_does_not_read_future,
                 test_false_static_confirmation_changes_actual_slow_weight,
                 test_missing_second_cancels_provisional_event,
                 test_small_step_during_dosing_cannot_authorize_fast_rate):
        test()
    print("A11 EVENT-GATED INVARIANTS 7/7 PASS")
