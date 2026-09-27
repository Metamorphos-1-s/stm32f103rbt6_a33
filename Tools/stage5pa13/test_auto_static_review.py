"""A13 synthetic invariants; independent A9 data are not claimed."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from auto_static_review import POLICIES, replay, synthetic_continuous_feed


def sample_path(steps=(), count=4000, slope_ug_sample=0, skip=()):
    out = []
    for tick in range(count):
        if tick in skip:
            continue
        mass = tick * slope_ug_sample + sum(delta for at, delta in steps if tick >= at)
        out.append((tick + 1_000_000, tick * 100, 2_000_000_000_000 + tick * 100_000_000, mass))
    return out


def mode_interval(start, end):
    return ((2_000_000_000_000 + start * 100_000_000,
             2_000_000_000_000 + end * 100_000_000),)


def test_auto_static_no_operator_marker_and_prefix_causality():
    data = sample_path(((1300, 500_000_000),), slope_ug_sample=200)
    full, meta = replay(data, POLICIES[1], ())
    assert len(meta["gates"]) == 1
    assert meta["gates"][0]["source"] == "STATIC_OBVIOUS_STEP"
    assert meta["gates"][0]["settled_seq"] - meta["gates"][0]["obvious_step_seq"] >= 5 * 10
    assert meta["gates"][0]["settled_seq"] - meta["gates"][0]["event_detected_seq"] <= 30 * 10
    assert meta["boost_samples"] > 0
    assert meta["max_10s_offset_ug"] <= 3500
    for cutoff in (1000, 1301, 1370, 1600, 3000):
        prefix, _ = replay(data[:cutoff], POLICIES[1], ())
        assert full[:cutoff] == prefix
    assert full[1300][1] - full[1299][1] == data[1300][3] - data[1299][3]


def test_repeated_static_load_unload_retains_offset_and_span():
    steps = ((1300, 500_000_000), (1900, -500_000_000),
             (2500, 500_000_000), (3100, -500_000_000))
    data = sample_path(steps, slope_ug_sample=200)
    trace, meta = replay(data, POLICIES[1], ())
    assert len(meta["gates"]) == 4
    assert all(e["source"] == "STATIC_OBVIOUS_STEP" for e in meta["gates"])
    for tick, _ in steps:
        assert trace[tick][2] == trace[tick - 1][2]
        assert trace[tick][1] - trace[tick - 1][1] == data[tick][3] - data[tick - 1][3]
    assert meta["max_10s_offset_ug"] <= 3500


def test_dosing_slow_feed_freezes_then_return_arms_fast():
    data = sample_path(count=3500, slope_ug_sample=200)
    data = [(seq, ms, host_ns, gross + min(max(tick - 1300, 0), 500) * 200)
            for tick, (seq, ms, host_ns, gross) in enumerate(data)]
    trace, meta = replay(data, POLICIES[1], mode_interval(1000, 1800))
    assert trace[999][2] > 0
    assert len({r[2] for r in trace[1000:1800]}) == 1
    assert len(meta["gates"]) == 1 and meta["gates"][0]["source"] == "DOSING_EXIT"
    assert meta["boost_samples"] > 0
    assert meta["max_10s_offset_ug"] <= 3500


def test_small_change_and_short_mechanical_pulse_fall_back():
    _, small = replay(sample_path(((1300, 150_000),)), POLICIES[1], ())
    assert not small["gates"] and small["boost_samples"] == 0
    pulse = sample_path(((1300, 3_000_000), (1350, -3_000_000)))
    trace, meta = replay(pulse, POLICIES[1], ())
    assert not meta["gates"] and meta["boost_samples"] == 0
    assert trace[-1][2] == 0
    assert any(r["reason"] == "STATIC_STEP_RETURNED" for r in meta["rebases"])


def test_continuing_large_motion_and_clock_gap_do_not_arm():
    ramp = sample_path(count=2200)
    ramp = [(s, ms, h, 0 if i < 1300 else (i - 1299) * 2_100_000)
            for i, (s, ms, h, _) in enumerate(ramp)]
    _, motion = replay(ramp, POLICIES[1], ())
    assert not motion["gates"] and motion["boost_samples"] == 0
    assert any(r["reason"] == "STATIC_STEP_UNSETTLED" for r in motion["rebases"])
    gap = sample_path(((1300, 500_000_000),), skip=range(1340, 1420))
    _, broken = replay(gap, POLICIES[1], ())
    assert not broken["gates"] and broken["boost_samples"] == 0
    assert any(r["reason"] == "TIME_GAP" for r in broken["rebases"])


def test_continuous_feed_requires_dosing():
    cases = synthetic_continuous_feed()
    assert len(cases) == 6
    assert all(x["static_absorbed_ug"] > 0 for x in cases)
    assert all(x["dosing_absorbed_ug"] == 0 for x in cases)
    assert all(x["static_absorbed_ug"] < x["actual_added_ug"] for x in cases)
    assert all(x["dosing_return_gate_sources"] == ["DOSING_EXIT"] for x in cases)


if __name__ == "__main__":
    checks = (test_auto_static_no_operator_marker_and_prefix_causality,
              test_repeated_static_load_unload_retains_offset_and_span,
              test_dosing_slow_feed_freezes_then_return_arms_fast,
              test_small_change_and_short_mechanical_pulse_fall_back,
              test_continuing_large_motion_and_clock_gap_do_not_arm,
              test_continuous_feed_requires_dosing)
    for check in checks:
        check()
    print(f"A13 AUTO STATIC INVARIANTS {len(checks)}/{len(checks)} PASS")
