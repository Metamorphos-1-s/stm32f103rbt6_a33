"""Read-only frozen-rule scoring; never discard the failed control window."""
import argparse
import csv
import hashlib
import json
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
STATE = {0:'OFF', 1:'DOSING', 2:'STEP_SETTLING', 3:'STEP_PENDING',
         4:'HOLDOFF', 5:'REFERENCE_FILL', 6:'OBSERVATION_FILL', 7:'TRACKING', 8:'LIMITED'}


def score(directory):
    path = directory / 'samples.csv'
    rows = list(csv.DictReader(path.open(newline='', encoding='utf-8')))
    events = [json.loads(line) for line in (directory / 'events.jsonl').read_text().splitlines()]
    begin = next(e for e in events if e['kind'] == 'QUALIFICATION_START')
    sequence_start = int(begin['sequence'])
    samples = []
    prior_seq = None
    for row in rows:
        seq = int(row['sequence'])
        if seq < sequence_start or seq == prior_seq:
            continue
        sample = {key:int(value) for key,value in row.items() if key != 'utc'}
        sample['utc'] = row['utc']
        samples.append(sample)
        prior_seq = seq
    gaps = []
    previous = None
    groups = defaultdict(list)
    paired_realtime = []
    bad = []
    for sample in samples:
        seq = sample['sequence']
        if previous is not None and (seq != previous['sequence']+1 or
                not 0 < sample['mcu_ms']-previous['mcu_ms'] <= 250):
            gaps.append(dict(previous_sequence=previous['sequence'], sequence=seq,
                missing_sequences=list(range(previous['sequence']+1,seq)),
                delta_mcu_ms=sample['mcu_ms']-previous['mcu_ms'],
                previous_host_monotonic_ns=previous['host_monotonic_ns'],
                host_monotonic_ns=sample['host_monotonic_ns'],
                realtime_diagnostic_sequence=sample['realtime_sequence']))
        if (sample['produced']-sample['consumed'] != sample['fifo'] or
            sample['engine_sequence'] != sample['consumed']-sample['invalid'] or
            sample['call_sequence'] != sample['engine_sequence'] or
            sample['fault'] or sample['dirty'] or sample['overrun'] or
            sample['driver_read_errors'] or sample['flags'] != 1):
            bad.append(seq)
        groups[STATE[sample['timed_state']]].append(sample)
        paired_realtime.append(sample['realtime_sequence'])
        previous = sample
    maxima = {}
    for name, data in groups.items():
        peak = max(data, key=lambda s:s['call_cycles'])
        maxima[name] = dict(observed_calls=len(data), maximum_cycles=peak['call_cycles'],
            corresponding_sequence=peak['sequence'],
            microseconds=peak['call_cycles']*1e6/peak['hclk_hz'])
    last = samples[-1]
    low = min(samples, key=lambda s:s['lowest_touched']-s['static_end'])
    failure = next((e for e in events if e['kind'] == 'MEASUREMENT_FAILURE'), None)
    summary = json.loads((directory/'summary.json').read_text())
    required = {'OFF', 'DOSING', 'REFERENCE_FILL', 'OBSERVATION_FILL', 'TRACKING', 'STEP_SETTLING'}
    missing_coverage = sorted(required-set(maxima))
    result = 'FAIL' if failure or gaps or bad else 'INCOMPLETE' if missing_coverage else 'PASS'
    return dict(stage='A13D', result='A13D TARGET RESOURCE GATE '+result,
        classification='DIAGNOSTIC_IMAGE_DIRECT_MEASUREMENTS_NOT_EXACT_UNMODIFIED_BIN_TIMING',
        raw_sha256=hashlib.sha256(path.read_bytes()).hexdigest().upper(),
        raw_poll_rows=len(rows), accepted_before_abort_unique_samples=summary['qualified_unique_samples'],
        raw_unique_qualified_domain_including_failure=len(samples),
        first_sequence=sequence_start, last_observed_sequence=last['sequence'],
        gaps=gaps, conservation_or_safety_bad_sequences=bad,
        resource_rule_failure=failure, state_cycle_maxima=maxima,
        main_loop_max_cycles=max(s['loop_max_cycles'] for s in samples),
        main_loop_interval_max_cycles=max(s['loop_interval_max_cycles'] for s in samples),
        actual_read_hclk_hz=last['hclk_hz'],
        overhead_empty_bracket_max_cycles=max(s['overhead_max_cycles'] for s in samples),
        overhead_subtracted=False,
        lowest_touched_address='0x%08X'%low['lowest_touched'],
        static_end_address='0x%08X'%low['static_end'], stack_top_address='0x%08X'%low['stack_top'],
        minimum_untouched_bytes=low['lowest_touched']-low['static_end'],
        observed_stack_top_to_touched_bytes=low['stack_top']-low['lowest_touched'],
        missing_state_coverage=missing_coverage, fast_tracking_coverage='NOT RUN',
        hardware_adc_loss_supported=False if not bad else 'UNCONFIRMED',
        failure_interpretation='one timed-call record overwritten before host read at mode-command/five-block handoff; ordinary block saw1590; do not relabel as complete',
        a13b_efficacy='INCONCLUSIVE; owner deferral unchanged')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = score(args.directory)
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
