#!/usr/bin/env python3
"""Frozen A13 10 Hz independent replay and pre-registered adjudication; offline only."""
from __future__ import annotations

import argparse
import csv
import json
import statistics
import subprocess
import sys
from dataclasses import asdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Tools' / 'stage5pa13'))
sys.path.insert(0, str(ROOT / 'Tools' / 'stage5pa12'))
from auto_static_review import POLICIES  # noqa: E402
from sample_clock_review import replay, samples  # noqa: E402
sys.path.insert(0, str(ROOT / 'Tools' / 'stage5pa11'))
from event_gated_review import edges, rows, sha, short_scores  # noqa: E402

POLICY = POLICIES[1]
A13_BLOB = '148fa72aa1438291945ba6eb367d5bce971773d8'
A12_BLOB = 'f81953a141706b3118f5637cdfd53e5d26dd3846'
RULES = {
    'display_division_d_g': 0.01, 'legal_verification_division_e_g': 1,
    'sample_hz': 10, 'max_device_timestamp_step_ms': 250,
    'max_host_gap_s': 0.5, 'minimum_empty_before_first_load_s': 600,
    'minimum_load_s': 1800, 'maximum_load_s': 3600,
    'minimum_unloaded_recovery_s': 1800, 'effect_stimulus_ug': 40000,
    'effect_minimum_improvement_fraction': 0.5,
    'effect_max_corrected_change_ug': 20000,
    'maximum_offset_ug': 500000, 'maximum_10s_offset_change_ug': 3500,
    'maximum_span_loss_ug': 10000,  # one display division; step sample itself must be exact
    'early_reference_s': [15, 45], 'checkpoint_5min_s': [300, 360],
}
REQUIRED = ('firmware', 'map', 'sample_rate', 'gain', 'sample_sequence',
            'primary_sample_sequence', 'timestamp_ms', 'host_monotonic_ns',
            'gross_ug', 'raw_adc', 'filtered_raw', 'display_count',
            'conditioned_display_ug', 'display_anchor_ug', 'official_stable',
            'official_overload', 'calibration_valid',
            'overrun_count', 'fault_mask', 'dirty', 'revision',
            'saved_revision', 'application', 'mode', 'offset_ug',
            'save_request_count_low')


def frozen_contract():
    for relative, expected in (('Tools/stage5pa13/auto_static_review.py', A13_BLOB),
                               ('Tools/stage5pa12/sample_clock_review.py', A12_BLOB)):
        actual = subprocess.check_output(['git', 'hash-object', relative], cwd=ROOT, text=True).strip()
        if actual != expected:
            raise ValueError('candidate Git blob changed: ' + relative)
    assert POLICY.name == 'auto_static_350_5m'
    assert POLICY.auto_static and POLICY.boosted_ug_per_sample == 35
    assert POLICY.boost_samples == 3000 and POLICY.hz == 10
    assert POLICY.baseline_ug_per_sample == 5
    assert POLICY.obvious_step_ug == 2_000_000
    assert POLICY.step_ug == 100_000 and POLICY.deadband_ug == 10_000
    assert POLICY.offset_cap_ug == 500_000
    return {**asdict(POLICY), 'auto_static': True,
            'a13_source_git_blob': A13_BLOB, 'a12_source_git_blob': A12_BLOB}


def as_int(row, key):
    return int(row[key])


class SafetyGateError(ValueError):
    """A real device or candidate safety invariant failed, not a missing input."""


def validate_recorded_rows(path, data, environment, summary, firmware):
    """Fail closed on acquisition and safety prerequisites before a replay."""
    with path.open(encoding='utf-8', newline='') as stream:
        reader = csv.DictReader(stream)
        if not set(REQUIRED).issubset(reader.fieldnames or ()):
            raise ValueError('required measured channels are missing')
        result = list(reader)
    if not result or len(result) != len(data):
        raise ValueError('device sample count differs from CSV rows')
    if summary.get('status') != 'COMPLETE' or summary.get('records') != len(result):
        raise ValueError('capture did not complete or recorded count differs')
    if summary.get('read_errors') or summary.get('unobserved_sample_sequences'):
        raise ValueError('capture reported read errors or missing device samples')
    if summary.get('maximum_host_gap_s', 0) > RULES['max_host_gap_s']:
        raise ValueError('capture reported a long host polling gap')
    if environment.get('filter_mode') != 1 or environment.get('filter_strength') != 3:
        raise ValueError('expected actual filt1 strength3 capture')
    if environment.get('sample_rate') != 0 or environment.get('calibration', {}).get('span_mass_ug') != 500000000:
        raise ValueError('expected actual 10 Hz and 500 g calibrated endpoint')
    first = result[0]
    original = (as_int(first, 'revision'), as_int(first, 'saved_revision'))
    if original[0] != original[1] or as_int(first, 'dirty'):
        raise SafetyGateError('initial unsaved or dirty configuration')
    previous = None
    for i, row in enumerate(result):
        if row['firmware'] != firmware or row['map'] != '0x0104':
            raise ValueError('firmware or register Map changed at row %d' % i)
        if as_int(row, 'sample_rate') != 0 or as_int(row, 'gain') != 3:
            raise SafetyGateError('sample profile changed at row %d' % i)
        if (as_int(row, 'revision'), as_int(row, 'saved_revision')) != original:
            raise SafetyGateError('revision changed at row %d' % i)
        for key, wanted in (('dirty', 0), ('fault_mask', 0),
                            ('overrun_count', 0), ('official_overload', 0),
                            ('calibration_valid', 1), ('application', 0),
                            ('mode', 0), ('offset_ug', 0)):
            if as_int(row, key) != wanted:
                raise SafetyGateError('%s invalid at row %d' % (key, i))
        if as_int(row, 'save_request_count_low') != as_int(first, 'save_request_count_low'):
            raise SafetyGateError('SAVE request count changed at row %d' % i)
        if abs(as_int(row, 'primary_sample_sequence') - as_int(row, 'sample_sequence')) > 1:
            raise ValueError('ADC primary/shadow sequence skew exceeds one at row %d' % i)
        if previous is not None and (as_int(row, 'host_monotonic_ns') - as_int(previous, 'host_monotonic_ns')) > RULES['max_host_gap_s'] * 1e9:
            raise ValueError('host monotonic polling gap at row %d' % i)
        previous = row
    return {'records': len(result), 'revision_saved': list(original),
            'first_sequence': data[0][0], 'last_sequence': data[-1][0],
            'first_device_ms': data[0][1], 'last_device_ms': data[-1][1],
            'first_host_ns': data[0][2], 'last_host_ns': data[-1][2],
            'save_request_count_low': as_int(first, 'save_request_count_low'),
            'read_errors': summary['read_errors'], 'unobserved_samples': summary['unobserved_sample_sequences'],
            'maximum_host_gap_s': summary['maximum_host_gap_s'],
            'primary_shadow_nonatomic': True}


def effect_status(raw_change, corrected_change):
    """Frozen per-phase 5-6 min result, not a device-level zero claim."""
    if raw_change is None or corrected_change is None:
        return 'INVALID/INCOMPLETE'
    if abs(raw_change) < RULES['effect_stimulus_ug'] / 1e6:
        return 'INCONCLUSIVE'
    if (abs(corrected_change) <= abs(raw_change) * (1 - RULES['effect_minimum_improvement_fraction'])
            and abs(corrected_change) <= RULES['effect_max_corrected_change_ug'] / 1e6):
        return 'PASS'
    return 'FAIL'

def score_after_replay(path, data, trace, meta):
    """Only this function reads inferred edges; never feeds them to controller."""
    event_list = edges(rows(path))
    end_s = (data[-1][2] - data[0][2]) / 1e9
    labels = [event['kind'] for event in event_list]
    if labels != ['load', 'unload', 'load', 'unload']:
        return {'decision': 'INVALID/INCOMPLETE', 'reason': 'not exactly two load/unload cycles', 'edges': event_list}
    first_edge = event_list[0]['center_s']
    durations = [event_list[i + 1]['center_s'] - event_list[i]['center_s']
                 for i in range(len(event_list) - 1)] + [end_s - event_list[-1]['center_s']]
    if first_edge < RULES['minimum_empty_before_first_load_s']:
        return {'decision': 'INVALID/INCOMPLETE', 'reason': 'empty baseline under 10 minutes', 'edges': event_list}
    if (not RULES['minimum_load_s'] <= durations[0] <= RULES['maximum_load_s']
            or durations[1] < RULES['minimum_unloaded_recovery_s']
            or not RULES['minimum_load_s'] <= durations[2] <= RULES['maximum_load_s']
            or durations[3] < RULES['minimum_unloaded_recovery_s']):
        return {'decision': 'INVALID/INCOMPLETE', 'reason': 'physical stage durations outside preregistered bounds',
                'edges': event_list, 'phase_durations_s': durations}
    first_host = data[0][2]
    sequence_index = {sample[0]: index for index, sample in enumerate(data)}
    def milestone(seq):
        if seq is None:
            return None
        index = sequence_index.get(seq)
        if index is None:
            return None
        sample = data[index]
        return {'sequence': seq, 'device_ms': sample[1],
                'device_relative_s': (sample[1] - data[0][1]) / 1000,
                'host_monotonic_ns': sample[2],
                'host_relative_s': (sample[2] - first_host) / 1e9}
    gross_trace = [((row[2] - first_host) / 1e9, row[3], 0, 'MEASURED') for row in data]
    gross_scores = short_scores(gross_trace, event_list)
    corrected_scores = short_scores(trace, event_list)
    if len(meta['gates']) != 4:
        safety = ['unexpected static boost trigger count %d' % len(meta['gates'])]
    else:
        safety = []
    if len(meta['gates']) == 4 and any(g.get('source') != 'STATIC_OBVIOUS_STEP' for g in meta['gates']):
        safety.append('unexpected boost trigger source')
    for i, event in enumerate(event_list):
        row_index = event['index']
        if trace[row_index][2] != trace[row_index - 1][2]:
            safety.append('offset changed at physical step %d' % (i + 1))
        if trace[row_index][1] - trace[row_index - 1][1] != data[row_index][3] - data[row_index - 1][3]:
            safety.append('physical step loss at event %d' % (i + 1))
        if abs(gross_scores[i]['span_early_g'] - corrected_scores[i]['span_early_g']) * 1e6 > RULES['maximum_span_loss_ug']:
            safety.append('load/unload span loss at event %d' % (i + 1))
    for i, gate in enumerate(meta['gates'][:4]):
        edge_host = first_host + round(event_list[i]['center_s'] * 1e9)
        event_seq = min(data, key=lambda r: abs(r[2] - edge_host))[0]
        if not 0 <= gate['event_detected_seq'] - event_seq <= 30 * POLICY.hz:
            safety.append('boost trigger not aligned to event %d' % (i + 1))
    maximum_offset = max(abs(row[2]) for row in trace)
    if maximum_offset > RULES['maximum_offset_ug']:
        safety.append('absolute offset exceeded 0.5 g')
    if meta['max_10s_offset_ug'] > RULES['maximum_10s_offset_change_ug']:
        safety.append('10-second offset speed exceeded 0.0035 g')
    start_empty = [row[3] for row in data if first_edge - 300 <= (row[2] - first_host) / 1e9 < first_edge]
    initial_empty_g = statistics.median(start_empty) / 1e6 if start_empty else None
    stages = []
    for i, (raw, corrected) in enumerate(zip(gross_scores, corrected_scores)):
        raw_change = raw['relative_change_g']['5']
        corrected_change = corrected['relative_change_g']['5']
        status = effect_status(raw_change, corrected_change)
        event = event_list[i]
        relevant = meta['gates'][i] if i < len(meta['gates']) else None
        lock_seq = None
        first_correction_seq = None
        if relevant:
            start = next((k for k, row in enumerate(data) if row[0] >= relevant['event_detected_seq']), len(data))
            end = next((k for k, row in enumerate(data) if row[0] >= (meta['gates'][i + 1]['event_detected_seq'] if i + 1 < len(meta['gates']) else data[-1][0])), len(data))
            lock_seq = next((data[k][0] for k in range(start, end) if trace[k][3] == 'OBSERVATION_FILL'
                             and trace[k - 1][3] == 'REFERENCE_FILL'), None)
            first_correction_seq = next((data[k][0] for k in range(max(1, start), end) if trace[k][2] != trace[k - 1][2]), None)
        stages.append({'kind': event['kind'], 'edge_bracket_host_s': [event['lo_s'], event['hi_s']],
                       'duration_s': durations[i], 'raw': raw, 'candidate': corrected,
                       'stimulus_g': raw_change, 'corrected_change_g': corrected_change,
                       'effect_status': status, 'gate': relevant, 'reference_locked_seq': lock_seq,
                       'first_correction_seq': first_correction_seq,
                       'obvious_step_time': milestone(relevant.get('obvious_step_seq')) if relevant else None,
                       'robust_step_time': milestone(relevant.get('event_detected_seq')) if relevant else None,
                       'quiet_confirmed_time': milestone(relevant.get('settled_seq')) if relevant else None,
                       'reference_locked_time': milestone(lock_seq),
                       'first_correction_time': milestone(first_correction_seq),
                       'preload_empty_baseline_g': initial_empty_g if i == 0 else None,
                       'absolute_unloaded_zero_end_g': corrected['end_corrected_g'] if event['kind'] == 'unload' else None,
                       'measured_unloaded_zero_end_g': raw['end_corrected_g'] if event['kind'] == 'unload' else None,
                       'step_offset_ug': [trace[event['index'] - 1][2], trace[event['index']][2]]})
    decision = ('FAIL' if safety or any(p['effect_status'] == 'FAIL' for p in stages)
                else 'INVALID/INCOMPLETE' if any(p['effect_status'] == 'INVALID/INCOMPLETE' for p in stages)
                else 'PASS' if all(p['effect_status'] == 'PASS' for p in stages)
                else 'SAFETY PASS; EFFICACY INCONCLUSIVE')
    return {'decision': decision, 'safety_failures': safety, 'effect_stages': stages,
            'inferred_edges_scoring_only': event_list, 'phase_durations_s': durations,
            'initial_empty_baseline_g': initial_empty_g, 'max_abs_offset_ug': maximum_offset,
            'max_10s_offset_change_ug': meta['max_10s_offset_ug'],
            'all_rebuild_and_cancellation_reasons': meta['rebases'],
            'rebases_with_device_clock': [{**r, 'clock': milestone(r.get('seq'))} for r in meta['rebases']],
            'model_gates': meta['gates']}


def adjudicate(path, expected_sha, environment, summary, firmware, event_log=None,
               data_role='UNSPECIFIED'):
    policy = frozen_contract()
    digest = sha(path)
    out = {'classification': 'STAGE5PA13B_INDEPENDENT_10HZ_DATA_NOT_PRODUCT_QUALIFICATION',
           'csv_sha256': digest, 'expected_sha256': expected_sha,
           'policy': policy, 'rules': RULES, 'controller_input': '10Hz gross, sequence and MCU timestamp; no event markers',
           'data_role': data_role}
    if digest != expected_sha:
        return {**out, 'decision': 'INVALID/INCOMPLETE', 'reason': 'CSV SHA-256 mismatch'}
    try:
        data = list(samples(path))
        out['acquisition'] = validate_recorded_rows(path, data, environment, summary, firmware)
    except SafetyGateError as exc:
        return {**out, 'decision': 'FAIL', 'reason': str(exc)}
    except (KeyError, TypeError, ValueError) as exc:
        return {**out, 'decision': 'INVALID/INCOMPLETE', 'reason': str(exc)}
    # Crucial: modes are STATIC for the entire causal pass; no operator or physical edge inputs.
    trace, meta = replay(data, POLICY, ())
    out.update(score_after_replay(path, data, trace, meta))
    if event_log is not None:
        # Human markers are read after replay and never used to drive a mode transition.
        out['operator_event_log_sha256'] = sha(event_log)
        out['operator_markers_postscore_only'] = [json.loads(line)
            for line in event_log.read_text(encoding='utf-8').splitlines() if line]
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--input', type=Path, required=True)
    p.add_argument('--expected-sha256', required=True)
    p.add_argument('--environment', type=Path, required=True)
    p.add_argument('--summary', type=Path, required=True)
    p.add_argument('--firmware', required=True)
    p.add_argument('--events', type=Path)
    p.add_argument('--data-role', choices=('OPENED_DEVELOPMENT_SMOKE', 'INDEPENDENT_HOLDOUT_POST_FREEZE'), required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    result = adjudicate(args.input, args.expected_sha256.upper(),
                        json.loads(args.environment.read_text(encoding='utf-8')),
                        json.loads(args.summary.read_text(encoding='utf-8')),
                        args.firmware, args.events, args.data_role)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'decision': result['decision'], 'sha256': result['csv_sha256'],
                      'rows': result.get('acquisition', {}).get('records'),
                      'output': str(args.output)}, indent=2))
    return 0 if result['decision'] in ('PASS', 'SAFETY PASS; EFFICACY INCONCLUSIVE') else 2


if __name__ == '__main__':
    raise SystemExit(main())
