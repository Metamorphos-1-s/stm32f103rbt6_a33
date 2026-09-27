#!/usr/bin/env python3
"""Compare the A13C C controller to the unmodified A13 Python at every sample."""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Tools/stage5pa13'))
sys.path.insert(0, str(ROOT / 'Tools/stage5pa12'))
sys.path.insert(0, str(ROOT / 'Tools/stage5pa13b'))
sys.path.insert(0, str(ROOT / 'Tools/stage5pa11'))
from auto_static_review import POLICIES  # noqa: E402
from sample_clock_review import replay, samples  # noqa: E402
from event_gated_review import sha  # noqa: E402

POLICY = POLICIES[1]
STATE = ('OFF', 'DOSING', 'STEP_SETTLING', 'STEP_PENDING', 'HOLDOFF',
         'REFERENCE_FILL', 'OBSERVATION_FILL', 'TRACKING', 'LIMITED')
REASON = ('NONE', 'INITIAL', 'TIME_GAP', 'STATIC_STEP_PENDING',
          'STATIC_STEP_BASELINE_ONLY', 'STATIC_STEP_FAST',
          'STATIC_STEP_RETURNED', 'STATIC_STEP_UNSETTLED', 'DOSING_ENTRY',
          'DOSING_EXIT_STATIC', 'DOSING_RECENT_STEP', 'ZERO', 'CALIBRATION',
          'PROFILE', 'INVALID_INPUT', 'FAULT', 'OVERLOAD', 'NEAR_RAIL',
          'REPRESENTATION', 'NUMERIC')


def synthetic(name):
    sys.path.insert(0, str(ROOT / 'Tools/stage5pa13'))
    from test_auto_static_review import sample_path, mode_interval
    if name == 'step':
        return sample_path(((1300, 500_000_000),), slope_ug_sample=200), ()
    if name == 'cycles':
        return sample_path(((1300, 500_000_000), (1900, -500_000_000),
                            (2500, 500_000_000), (3100, -500_000_000)),
                           slope_ug_sample=200), ()
    if name == 'pulse':
        return sample_path(((1300, 3_000_000), (1350, -3_000_000))), ()
    if name == 'motion':
        data = sample_path(count=2200)
        return [(s, ms, h, 0 if i < 1300 else (i - 1299) * 2_100_000)
                for i, (s, ms, h, _) in enumerate(data)], ()
    if name == 'gap':
        return sample_path(((1300, 500_000_000),), skip=range(1340, 1420)), ()
    if name == 'dosing':
        data = sample_path(count=3500, slope_ug_sample=200)
        return [(seq, ms, h, mass + min(max(i - 1300, 0), 500) * 200)
                for i, (seq, ms, h, mass) in enumerate(data)], mode_interval(1000, 1800)
    raise ValueError(name)


def compare(data, intervals, executable, output):
    expected, meta = replay(data, POLICY, intervals)
    payload = ''.join(f'{s},{ms},{mass},{1 if any(a <= h < b for a, b in intervals) else 2}\n'
                      for s, ms, h, mass in data)
    run = subprocess.run([str(executable)], input=payload, text=True,
                         capture_output=True, timeout=300, check=False)
    report = {'classification': 'A13C_PYTHON_C_SAMPLE_PARITY',
              'candidate': POLICY.name, 'samples': len(data),
              'runner_exit_code': run.returncode,
              'python_gates': meta['gates'], 'python_rebases': meta['rebases'],
              'python_max_10s_offset_ug': meta['max_10s_offset_ug'],
              'first_mismatch': None, 'mismatch_count': 0}
    if run.returncode != 0:
        report['runner_stderr_tail'] = run.stderr[-3000:]
    lines = run.stdout.splitlines()
    if len(lines) != len(data):
        report['first_mismatch'] = {'kind': 'line_count', 'expected': len(data),
                                    'actual': len(lines)}
        report['mismatch_count'] = 1
    rb = {}
    for item in meta['rebases']:
        rb.setdefault(item['seq'], []).append(item['reason'])
    gate = {}
    for item in meta['gates']:
        seq = item.get('settled_seq', item.get('mode_confirmed_seq'))
        gate.setdefault(seq, []).append(item)
    observed_rebuilds = 0
    observed_gates = 0
    c_max10 = 0
    offset_window = []
    c_gates = []
    c_trace = []
    for index, (sample, py, line) in enumerate(zip(data, expected, lines)):
        seq, ms = sample[:2]
        cols = line.split(',')
        if len(cols) != 14:
            actual = {'invalid_c_column_count': len(cols), 'line': line[:200]}
        else:
            fields = [int(c) for c in cols]
            c_trace.append(fields)
            (c_seq, c_corrected, c_offset, c_state, c_gates_count,
             c_rebuilds, c_boost, c_reason, c_obvious, c_robust,
             c_quiet, c_lock, c_correction, c_limited) = fields
            observed_rebuilds += len(rb.get(seq, []))
            observed_gates += len(gate.get(seq, []))
            actual = {'c': fields, 'py': [seq, py[1], py[2], py[3]],
                      'expected_rebuild_count': observed_rebuilds,
                      'expected_gate_count': observed_gates}
            defects = []
            if c_seq != seq or c_corrected != py[1] or c_offset != py[2]:
                defects.append('corrected_offset_or_sequence')
            if c_state >= len(STATE) or STATE[c_state] != py[3]:
                defects.append('state')
            if c_gates_count != observed_gates:
                defects.append('gate_count')
            if c_rebuilds != observed_rebuilds:
                defects.append('rebuild_count')
            if c_reason < len(REASON) and rb.get(seq) and REASON[c_reason] != rb[seq][-1]:
                defects.append('rebuild_reason')
            if c_limited:
                defects.append('limited')
            if gate.get(seq):
                c_gates.append({'seq': seq, 'source': gate[seq][-1]['source'],
                                'c_obvious_seq': c_obvious, 'c_robust_seq': c_robust,
                                'c_quiet_seq': c_quiet, 'c_reference_lock_seq': c_lock,
                                'c_correction_seq': c_correction})
            offset_window = [(old_ms, old_off) for old_ms, old_off in offset_window
                             if ms - old_ms <= 10_000]
            if offset_window:
                c_max10 = max(c_max10, max(abs(c_offset - old_off)
                                             for _, old_off in offset_window))
            offset_window.append((ms, c_offset))
            if defects:
                actual['defects'] = defects
            else:
                actual = None
        if actual is not None:
            report['mismatch_count'] += 1
            if report['first_mismatch'] is None:
                report['first_mismatch'] = {'index': index, 'sequence': seq,
                                            'mass_ug': sample[3], **actual}
    # Post-replay timing is derived only from the causal Python trace and
    # frozen gate metadata, never from retrospective physical edges.
    seq_index = {item[0]: index for index, item in enumerate(data)}
    timing = []
    if len(c_trace) == len(data):
        for number, source in enumerate(meta['gates']):
            emitted = source.get('settled_seq', source.get('mode_confirmed_seq'))
            start = seq_index[emitted]
            end = (seq_index[meta['gates'][number + 1].get(
                'settled_seq', meta['gates'][number + 1].get('mode_confirmed_seq'))]
                if number + 1 < len(meta['gates']) else len(data))
            expected_lock = next((data[j][0] for j in range(start, end - 1)
                                  if expected[j][3] == 'REFERENCE_FILL' and
                                     expected[j + 1][3] == 'OBSERVATION_FILL'), None)
            expected_correction = next((data[j][0] for j in range(max(1, start), end)
                                        if expected[j][2] != expected[j - 1][2]), None)
            actual_lock = next((c_trace[j][11] for j in range(start, end)
                                if c_trace[j][11] != 0), None)
            actual_correction = next((c_trace[j][12] for j in range(start, end)
                                      if c_trace[j][12] != 0), None)
            detail = {'source': source['source'], 'emitted_seq': emitted,
                      'expected_obvious_seq': source.get('obvious_step_seq'),
                      'expected_robust_seq': source.get('event_detected_seq'),
                      'expected_quiet_seq': source.get('settled_seq',
                                                       source.get('mode_confirmed_seq')),
                      'actual_obvious_seq': c_trace[start][8],
                      'actual_robust_seq': c_trace[start][9],
                      'actual_quiet_seq': c_trace[start][10],
                      'expected_reference_lock_seq': expected_lock,
                      'actual_reference_lock_seq': actual_lock,
                      'expected_first_correction_seq': expected_correction,
                      'actual_first_correction_seq': actual_correction}
            for part in ('obvious', 'robust', 'quiet', 'reference_lock',
                         'first_correction'):
                expected_value = detail['expected_' + part + '_seq']
                actual_value = detail['actual_' + part + '_seq']
                if (0 if expected_value is None else expected_value) != (
                        0 if actual_value is None else actual_value):
                    report['mismatch_count'] += 1
                    if report['first_mismatch'] is None:
                        report['first_mismatch'] = {
                            'kind': 'gate_timing', 'part': part, **detail}
            timing.append(detail)
        if c_trace[-1][6] != meta['boost_samples']:
            report['mismatch_count'] += 1
            if report['first_mismatch'] is None:
                report['first_mismatch'] = {
                    'kind': 'boost_samples', 'python': meta['boost_samples'],
                    'c': c_trace[-1][6]}
    report['gate_timing'] = timing
    report['c_max_10s_offset_ug'] = c_max10
    report['c_gate_details'] = c_gates
    report['status'] = 'PASS' if report['mismatch_count'] == 0 and run.returncode == 0 else 'FAIL'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n',
                      encoding='utf-8')
    print(json.dumps({'status': report['status'], 'samples': len(data),
                      'mismatches': report['mismatch_count'],
                      'first': report['first_mismatch'],
                      'c_max_10s_ug': c_max10}, ensure_ascii=False))
    return report['status'] == 'PASS'


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--input', type=Path)
    p.add_argument('--sha256')
    p.add_argument('--synthetic', choices=('step', 'cycles', 'pulse', 'motion', 'gap', 'dosing'))
    p.add_argument('--runner', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    if bool(args.input) == bool(args.synthetic):
        p.error('choose exactly one --input or --synthetic')
    if args.input:
        if not args.sha256 or sha(args.input) != args.sha256.upper():
            raise ValueError('CSV SHA-256 mismatch')
        data, intervals = list(samples(args.input)), ()
    else:
        data, intervals = synthetic(args.synthetic)
    return 0 if compare(data, intervals, args.runner, args.output) else 1


if __name__ == '__main__':
    raise SystemExit(main())
