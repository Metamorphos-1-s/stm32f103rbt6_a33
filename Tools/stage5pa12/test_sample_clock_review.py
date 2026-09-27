"""Synthetic 10 Hz controller invariants; A9 remains opened development data."""
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from sample_clock_review import POLICIES, replay, samples, synthetic_misdeclared_static


def sequence(step_ug=500_000_000, skip=()):
    first = 1_500_000
    return [(first + tick, 200_000 + tick * 100,
             2_000_000_000_000 + tick * 100_000_000,
             tick * 200 + (step_ug if tick >= 1300 else 0))
            for tick in range(2600) if tick not in skip]


def dosing(start=1000, end=1900):
    return ((2_000_000_000_000 + start * 100_000_000,
             2_000_000_000_000 + end * 100_000_000),)


def test_prefix_causality_and_dosing_freeze():
    data = sequence()
    policy = POLICIES[2]
    full, detail = replay(data, policy, dosing())
    for cutoff in (700, 1310, 1500, 1800, 1950, 2300):
        prefix, _ = replay(data[:cutoff], policy, dosing())
        assert full[:cutoff] == prefix
    assert len(detail["gates"]) == 1
    assert detail["gates"][0]["prefill_reference_count"] == 300
    assert all(row[3] == "DOSING" for row in full[1000:1900])
    assert len({row[2] for row in full[1000:1900]}) == 1
    assert full[1300][1] - full[1299][1] == data[1300][3] - data[1299][3]
    assert detail["max_10s_offset_ug"] <= 3500


def test_small_step_and_no_operator_stay_at_baseline():
    data = sequence(150_000)
    _, detail = replay(data, POLICIES[2], dosing())
    assert not detail["gates"] and detail["boost_samples"] == 0
    assert detail["max_10s_offset_ug"] <= 500
    _, detail = replay(sequence(), POLICIES[2], ())
    assert not detail["gates"] and detail["boost_samples"] == 0
    assert detail["max_10s_offset_ug"] <= 500


def test_gap_cancels_event_and_prefill():
    data = sequence(skip=range(1550, 1620))
    _, detail = replay(data, POLICIES[2], dosing())
    assert not detail["gates"] and detail["boost_samples"] == 0
    assert any(item["reason"] == "TIME_GAP" for item in detail["rebases"])


def test_no_prefill_and_stale_confirmation():
    full, detail = replay(sequence(), POLICIES[3], dosing())
    assert len(detail["gates"]) == 1 and detail["gates"][0]["prefill_reference_count"] == 0
    assert detail["max_10s_offset_ug"] <= 3500
    _, detail = replay(sequence(), POLICIES[2], dosing(1000, 2550))
    assert not detail["gates"] and detail["boost_samples"] == 0


def test_source_clock_rejects_sequence_or_timestamp_gap():
    with tempfile.TemporaryDirectory() as root:
        path = Path(root) / "input.csv"
        path.write_text("sample_sequence,timestamp_ms,host_monotonic_ns,gross_ug\n"
                        "100,1000,1000000000,0\n102,1100,1100000000,0\n")
        try:
            list(samples(path))
        except ValueError as exc:
            assert "sequence" in str(exc)
        else:
            raise AssertionError("sequence gap admitted")
        path.write_text("sample_sequence,timestamp_ms,host_monotonic_ns,gross_ug\n"
                        "100,1000,1000000000,0\n101,1400,1100000000,0\n")
        try:
            list(samples(path))
        except ValueError as exc:
            assert "timestamp" in str(exc)
        else:
            raise AssertionError("clock gap admitted")


def test_premature_static_can_absorb_real_feed():
    cases = synthetic_misdeclared_static()
    assert len(cases) == 6
    assert all(x["continued_dosing_absorbed_ug"] == 0 for x in cases)
    assert all(x["premature_static_absorbed_ug"] > 0 for x in cases)
    for x in cases:
        assert x["premature_static_absorbed_ug"] < x["actual_added_ug"]


if __name__ == "__main__":
    checks = (test_prefix_causality_and_dosing_freeze,
              test_small_step_and_no_operator_stay_at_baseline,
              test_gap_cancels_event_and_prefill,
              test_no_prefill_and_stale_confirmation,
              test_source_clock_rejects_sequence_or_timestamp_gap,
              test_premature_static_can_absorb_real_feed)
    for check in checks:
        check()
    print(f"A12 SAMPLE CLOCK INVARIANTS {len(checks)}/{len(checks)} PASS")
