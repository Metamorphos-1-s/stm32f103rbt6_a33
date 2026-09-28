#!/usr/bin/env python3
"""Read-only hardware evidence scoring, frozen Python replay and sample coverage.

Never infers absent ADC, temperature, CPU cycle, stack or operator edge data.
Observed volatile modes enter replay; user markers never drive the controller.
"""
import argparse
import csv
import hashlib
import json
import statistics
import sys
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Tools/stage5pa13'))
sys.path.insert(0, str(ROOT / 'Tools/stage5pa12'))
from auto_static_review import POLICIES
from sample_clock_review import SampleController

STATES = ('OFF', 'DOSING', 'STEP_SETTLING', 'STEP_PENDING', 'HOLDOFF',
          'REFERENCE_FILL', 'OBSERVATION_FILL', 'TRACKING', 'LIMITED')
REASONS = ('NONE', 'INITIAL', 'TIME_GAP', 'STATIC_STEP_PENDING',
           'STATIC_STEP_BASELINE_ONLY', 'STATIC_STEP_FAST',
           'STATIC_STEP_RETURNED', 'STATIC_STEP_UNSETTLED', 'DOSING_ENTRY',
           'DOSING_EXIT_STATIC', 'DOSING_RECENT_STEP', 'ZERO', 'CALIBRATION',
           'PROFILE', 'INVALID_INPUT', 'FAULT', 'OVERLOAD', 'NEAR_RAIL',
           'REPRESENTATION', 'NUMERIC')


def analyze(path):
    # Reading an actively appended CSV is allowed for provisional monitoring;
    # the finished capture must be rescored with its final SHA.
    data_bytes = path.read_bytes()
    text = data_bytes.decode('utf-8')
    if not text.endswith('\n'):
        text = text.rsplit('\n', 1)[0] + '\n'
    rows = list(csv.DictReader(text.splitlines()))
    samples = []
    last_seq = None
    duplicate = 0
    transitions = []
    last_state = None
    boundaries = []
    for row in rows:
        if row.get('candidate_sequence') is None:
            continue
        converted = {key: int(value) for key, value in row.items()
                     if value and key not in ('utc', 'aux_fields_note')}
        converted['utc'] = row['utc']
        state_key = (converted['mode'], converted['state'], converted['reason'])
        if state_key != last_state:
            transitions.append({key: converted[key] for key in (
                'utc', 'host_monotonic_ns', 'candidate_sequence', 'candidate_mcu_ms',
                'mode', 'state', 'reason', 'offset_ug', 'gate_count', 'rebuild_count')})
            last_state = state_key
        seq = converted['candidate_sequence']
        if seq == last_seq:
            duplicate += 1
            if samples and converted['mode'] != samples[-1]['mode']:
                boundaries.append({'seq': seq, 'note': 'mode command snapshot on already observed sample'})
            continue
        samples.append(converted)
        last_seq = seq
    gaps = []
    max_delta = 0
    max10 = 0
    max_abs = 0
    paired = 0
    official_mismatches = 0
    safety_bad = []
    dosing_runs = []
    dosing = None
    last_mode = None
    window = deque()
    controller = None
    parity_count = 0
    mismatches = []
    step_edges = []
    before = None
    gate_timings = {}
    for sample in samples:
        mode = sample['mode']
        seq = sample['candidate_sequence']
        mass = sample['candidate_uncompensated_ug']
        ms = sample['candidate_mcu_ms']
        if any(sample[key] for key in ('limited', 'fault', 'dirty', 'overrun')):
            safety_bad.append({'seq': seq, 'limited': sample['limited'],
                               'fault': sample['fault'], 'dirty': sample['dirty'],
                               'overrun': sample['overrun']})
        if sample['sample_pair_matched']:
            paired += 1
            official_mismatches += sample['gross_ug'] != mass
        max_abs = max(max_abs, abs(sample['offset_ug']))
        if before is not None:
            ds = seq - before['candidate_sequence']
            dt = ms - before['candidate_mcu_ms']
            if ds != 1 or not 0 < dt <= 250:
                gaps.append({'prev_seq': before['candidate_sequence'], 'seq': seq,
                             'delta_seq': ds, 'delta_mcu_ms': dt})
            if mode == last_mode == 2 and ds == 1:
                max_delta = max(max_delta, abs(sample['offset_ug'] - before['offset_ug']))
            if abs(mass - before['candidate_uncompensated_ug']) >= 2000000:
                raw_delta = mass - before['candidate_uncompensated_ug']
                corrected_delta = sample['candidate_gross_ug'] - before['candidate_gross_ug']
                step_edges.append({'previous_seq': before['candidate_sequence'],
                    'seq': seq, 'previous_mcu_ms': before['candidate_mcu_ms'],
                    'mcu_ms': ms, 'direction': 'LOAD' if raw_delta > 0 else 'UNLOAD',
                    'raw_delta_ug': raw_delta, 'corrected_delta_ug': corrected_delta,
                    'step_loss_ug': raw_delta - corrected_delta,
                    'offset_before_ug': before['offset_ug'], 'offset_after_ug': sample['offset_ug']})
        if mode != last_mode:
            window.clear()
            if dosing is not None:
                dosing['end_seq'] = before['candidate_sequence'] if before else seq
                dosing_runs.append(dosing)
                dosing = None
            if mode == 1:
                dosing = dict(start_seq=seq, offset_ug=sample['offset_ug'], samples=0,
                              violations=0, nonzero=sample['offset_ug'] != 0)
        if mode == 1:
            dosing['samples'] += 1
            dosing['violations'] += sample['offset_ug'] != dosing['offset_ug']
        window.append((ms, sample['offset_ug']))
        while window and ms - window[0][0] > 10000:
            window.popleft()
        max10 = max(max10, max(abs(sample['offset_ug'] - old) for _, old in window))
        if mode == 0:
            controller = None
        else:
            if controller is None:
                controller = SampleController(POLICIES[1])
            expected_mass, expected_offset, expected_state = controller.feed(seq, ms, mass, mode == 1)
            parity_count += 1
            actual = (sample['candidate_gross_ug'], sample['offset_ug'],
                      STATES[sample['state']], sample['gate_count'],
                      sample['rebuild_count'], sample['boost_samples'],
                      REASONS[sample['reason']])
            reason = controller.rebases[-1]['reason'] if controller.rebases else 'INITIAL'
            expected = (expected_mass, expected_offset, expected_state,
                        len(controller.gates), len(controller.rebases),
                        controller.boost_samples, reason)
            if actual != expected:
                mismatches.append({'seq': seq, 'expected': expected, 'actual': actual})
        gate = sample['gate_count']
        if gate:
            item = gate_timings.setdefault(str(gate), {
                'first_observed_sequence': seq,
                'reason_at_first_observation': sample['reason'],
                'source': ('DOSING_EXIT' if sample['reason'] in (9, 10) else
                           'STATIC_OBVIOUS_STEP' if sample['reason'] == 5 else
                           'CHECK_REASON'),
                'offset_at_first_observation_ug': sample['offset_ug']})
            for field in ('obvious_sequence', 'robust_sequence', 'quiet_sequence',
                          'reference_lock_sequence', 'first_correction_sequence'):
                if sample[field]:
                    item[field] = sample[field]
            item['boost_samples_latest'] = sample['boost_samples']
            item['latest_seq'] = seq
            item['latest_offset_ug'] = sample['offset_ug']
        before = sample
        last_mode = mode
    if dosing is not None:
        dosing['end_seq'] = samples[-1]['candidate_sequence']
        dosing_runs.append(dosing)
    edge_bursts = []
    for edge in step_edges:
        if (not edge_bursts or edge['direction'] != edge_bursts[-1]['direction'] or
                edge['seq'] - edge_bursts[-1]['last_sequence'] > 20):
            edge_bursts.append(dict(direction=edge['direction'],
                first_sequence=edge['seq'], last_sequence=edge['seq'],
                filtered_ramp_substeps=0, maximum_step_loss_ug=0,
                mcu_bracket_first_ms=edge['previous_mcu_ms'],
                mcu_bracket_last_ms=edge['mcu_ms']))
        burst = edge_bursts[-1]
        burst['last_sequence'] = edge['seq']
        burst['mcu_bracket_last_ms'] = edge['mcu_ms']
        burst['filtered_ramp_substeps'] += 1
        burst['maximum_step_loss_ug'] = max(burst['maximum_step_loss_ug'], abs(edge['step_loss_ug']))
    segments = []
    if edge_bursts:
        starts = [samples[0]['candidate_sequence']] + [b['last_sequence'] + 1 for b in edge_bursts]
        ends = [b['first_sequence'] for b in edge_bursts] + [samples[-1]['candidate_sequence'] + 1]
        labels = ['EMPTY_BASELINE'] + ['LOADED' if b['direction'] == 'LOAD' else 'EMPTY_RECOVERY' for b in edge_bursts]
        for label, start, end in zip(labels, starts, ends):
            selected = [s for s in samples if start <= s['candidate_sequence'] < end]
            if not selected:
                continue
            first_ms, last_ms = selected[0]['candidate_mcu_ms'], selected[-1]['candidate_mcu_ms']
            entry = dict(label=label, first_seq=selected[0]['candidate_sequence'],
                last_seq=selected[-1]['candidate_sequence'], samples=len(selected),
                mcu_duration_s=(last_ms - first_ms) / 1000, windows={})
            for name, low, high in [('early_15_45s', first_ms + 15000, first_ms + 45000),
                                    ('last_30s', last_ms - 30000, last_ms + 1),
                                    ('minute_5_6', first_ms + 300000, first_ms + 360000)]:
                window_rows = [s for s in selected if low <= s['candidate_mcu_ms'] < high]
                if low < first_ms or high > last_ms + 1 or not window_rows:
                    entry['windows'][name] = 'NOT AVAILABLE'
                else:
                    entry['windows'][name] = dict(samples=len(window_rows),
                        medians={k: statistics.median(s[k] for s in window_rows) for k in
                            ('candidate_uncompensated_ug', 'candidate_gross_ug', 'offset_ug',
                             'raw_adc', 'filtered_adc_counts', 'display_count')})
            segments.append(entry)
    window_spans = {}
    if len(segments) >= 3:
        before_load = segments[0]['windows']['last_30s']
        early_load = segments[1]['windows']['early_15_45s']
        before_unload = segments[1]['windows']['last_30s']
        early_empty = segments[2]['windows']['early_15_45s']
        if all(isinstance(w, dict) for w in (before_load, early_load, before_unload, early_empty)):
            for key in ('candidate_uncompensated_ug', 'candidate_gross_ug'):
                loaded = early_load['medians'][key] - before_load['medians'][key]
                unloaded = before_unload['medians'][key] - early_empty['medians'][key]
                window_spans[key] = dict(load_span_ug=loaded, unload_span_ug=unloaded)
            window_spans['note'] = ('Robust phase windows, not instantaneous edge loss: '
                'offset can change across the 30s pre-unload window. Immediate edge '
                'preservation is separately evaluated at each observed >=2g substep.')
    clocks = {s['candidate_sequence']: s['candidate_mcu_ms'] for s in samples}
    for item in gate_timings.values():
        # Legacy step metadata persists on an explicit DOSING exit. Do not
        # re-label those old edge sequences as a new physical auto gate.
        keys = ('reference_lock_sequence', 'first_correction_sequence')
        if item['source'] == 'STATIC_OBVIOUS_STEP':
            keys += ('obvious_sequence', 'robust_sequence', 'quiet_sequence')
        origin = (item.get('robust_sequence') if item['source'] == 'STATIC_OBVIOUS_STEP'
                  else item['first_observed_sequence'])
        item['deadline_sequence'] = origin + 3000 if origin is not None else None
        item['fast_window_finished_in_observed_data'] = (
            item['latest_seq'] >= item['deadline_sequence']
            if item['deadline_sequence'] is not None else 'NOT AVAILABLE')
        for key in keys:
            sequence = item.get(key)
            if sequence in clocks:
                item[key.replace('_sequence', '_mcu_ms')] = clocks[sequence]
    return dict(classification='FOCUSED_HARDWARE_FUNCTIONAL_SAFETY_NOT_EFFICACY_QUALIFICATION',
        source=str(path), source_sha256=hashlib.sha256(data_bytes).hexdigest().upper(),
        raw_poll_rows=len(rows), unique_samples=len(samples), duplicate_polls=duplicate,
        sample_gaps=gaps, mode_boundary_snapshots=boundaries, state_transitions=transitions,
        matched_authoritative_pairs=paired, authoritative_input_mismatches=official_mismatches,
        safety_bad_samples=safety_bad, maximum_one_sample_offset_change_ug=max_delta,
        maximum_observed_ten_second_offset_change_ug=max10, maximum_absolute_offset_ug=max_abs,
        observed_obvious_edges=step_edges, edge_bursts=edge_bursts, segments=segments,
        segment_window_note='window origin is end of observed >=2g filtered ramp, not a precise operator completion time',
        robust_window_spans=window_spans,
        gate_timings=gate_timings,
        dosing_runs=dosing_runs,
        live_python_parity={'compared_samples': parity_count, 'mismatches': len(mismatches),
                            'first_mismatches': mismatches[:10],
                            'note': 'actual observed device mode; no artificial event timing; missing samples not interpolated'},
        target_cycles='NOT RUN', target_stack_watermark='NOT RUN',
        efficacy='INCONCLUSIVE; A13B owner deferral unchanged')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = analyze(args.input)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({key: report[key] for key in ('raw_poll_rows', 'unique_samples',
        'maximum_one_sample_offset_change_ug', 'maximum_observed_ten_second_offset_change_ug',
        'maximum_absolute_offset_ug', 'live_python_parity')}))


if __name__ == '__main__':
    main()
