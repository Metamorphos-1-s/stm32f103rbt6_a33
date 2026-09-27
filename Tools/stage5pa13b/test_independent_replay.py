"""A13B frozen rule tests; A9 is OPENED DEVELOPMENT, never the new holdout."""
import csv
import json
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
from independent_replay import (POLICY, RULES, adjudicate, effect_status,
                                frozen_contract, score_after_replay, sha)
from sample_clock_review import replay, samples

A9 = ROOT / 'Results/stage5pa9/20260927T060806Z_development_capture'
A9_HASH = 'E683609BA0DB7EA090D101CE38569A432EC319982E7D41A9D2176A62B623128C'
ENV = json.loads((A9 / 'environment.json').read_text())
SUMMARY = json.loads((A9 / 'summary.json').read_text())


def test_frozen_rules():
    policy = frozen_contract()
    assert policy['boosted_ug_per_sample'] == 35 and policy['boost_samples'] == 3000
    assert policy['auto_static'] and RULES['display_division_d_g'] == 0.01
    assert RULES['legal_verification_division_e_g'] == 1
    assert RULES['maximum_10s_offset_change_ug'] == 3500
    assert RULES['maximum_offset_ug'] == 500000
    assert RULES['maximum_span_loss_ug'] == 10000
    assert effect_status(.039999, 0) == 'INCONCLUSIVE'
    assert effect_status(.04, .020) == 'PASS'
    assert effect_status(.04, .020001) == 'FAIL'
    assert effect_status(.08, .021) == 'FAIL'
    assert effect_status(.08, .019) == 'PASS'
    assert effect_status(None, .001) == 'INVALID/INCOMPLETE'


def test_old_development_smoke_and_extra_gate():
    outcome = adjudicate(A9 / 'samples.csv', A9_HASH, ENV, SUMMARY, '0x051C', A9 / 'events.jsonl')
    assert outcome['decision'] == 'PASS' and len(outcome['effect_stages']) == 4
    assert all(s['effect_status'] == 'PASS' for s in outcome['effect_stages'])
    assert len(outcome['model_gates']) == 4 and not outcome['safety_failures']
    assert outcome['operator_event_log_sha256'] == sha(A9 / 'events.jsonl')
    data = list(samples(A9 / 'samples.csv'))
    trace, meta = replay(data, POLICY, ())
    meta['gates'].append(dict(meta['gates'][-1]))
    invalid = score_after_replay(A9 / 'samples.csv', data, trace, meta)
    assert invalid['decision'] == 'FAIL' and any('trigger count' in e for e in invalid['safety_failures'])


def test_sha_and_configuration_fail_closed():
    wrong = adjudicate(A9 / 'samples.csv', '0' * 64, ENV, SUMMARY, '0x051C')
    assert wrong['decision'] == 'INVALID/INCOMPLETE' and 'SHA' in wrong['reason']
    mismatch = dict(ENV, filter_mode=3)
    result = adjudicate(A9 / 'samples.csv', A9_HASH, mismatch, SUMMARY, '0x051C')
    assert result['decision'] == 'INVALID/INCOMPLETE' and 'filt1' in result['reason']


def test_sequence_clock_fault_and_host_gap_fail_closed():
    with (A9 / 'samples.csv').open(newline='', encoding='utf-8') as stream:
        iterator = csv.DictReader(stream)
        original = [next(iterator) for _ in range(4)]
        fields = list(iterator.fieldnames)
    with tempfile.TemporaryDirectory() as folder:
        path = Path(folder) / 'small.csv'
        def check(modified, reason, decision='INVALID/INCOMPLETE'):
            with path.open('w', newline='', encoding='utf-8') as stream:
                writer = csv.DictWriter(stream, fields)
                writer.writeheader()
                writer.writerows(modified)
            summary = dict(SUMMARY, status='COMPLETE', records=len(modified),
                           read_errors=0, unobserved_sample_sequences=0,
                           maximum_host_gap_s=.15)
            out = adjudicate(path, sha(path), ENV, summary, '0x051C')
            assert out['decision'] == decision and reason in out['reason'], out
        check([original[0], original[2]], 'sequence')
        changed = [dict(x) for x in original[:3]]
        changed[1]['timestamp_ms'] = str(int(changed[0]['timestamp_ms']) + 500)
        check(changed, 'timestamp')
        changed = [dict(x) for x in original[:3]]
        changed[1]['host_monotonic_ns'] = str(int(changed[0]['host_monotonic_ns']) + 700_000_000)
        changed[2]['host_monotonic_ns'] = str(int(changed[1]['host_monotonic_ns']) + 100_000_000)
        check(changed, 'host monotonic')
        changed = [dict(x) for x in original[:3]]
        changed[1]['fault_mask'] = '1'
        check(changed, 'fault_mask', 'FAIL')
        changed = [dict(x) for x in original[:3]]
        changed[1]['primary_sample_sequence'] = str(int(changed[1]['sample_sequence']) + 3)
        check(changed, 'skew')


if __name__ == '__main__':
    for test in (test_frozen_rules, test_old_development_smoke_and_extra_gate,
                 test_sha_and_configuration_fail_closed,
                 test_sequence_clock_fault_and_host_gap_fail_closed):
        test()
    print('A13B FROZEN INDEPENDENT GATES 4/4 PASS; A9 SMOKE DEVELOPMENT ONLY')
