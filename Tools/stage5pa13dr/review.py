"""Replay the pre-acquisition checker; never interpolate missing host polls."""
import argparse
import hashlib
import json
from pathlib import Path
from contract import Checker, PATHS


def review(directory):
    checker = Checker()
    first = last = None
    polls = 0
    with (directory / 'snapshots.jsonl').open(encoding='utf-8') as stream:
        for line in stream:
            record = json.loads(line)
            checker.check(record['target'], record['host_monotonic_ns'], record['safety'])
            first = first or record
            last = record
            polls += 1
    if last is None:
        raise ValueError('no target evidence')
    captured = json.loads((directory / 'summary.json').read_text())
    target = last['target']
    terminal = json.loads((directory / 'restore/terminal.json').read_text())
    backup = directory.parent / 'backup'
    restored = directory / 'restore'
    same_app = (backup / 'application.bin').read_bytes() == (restored / 'application_after.bin').read_bytes()
    same_config = (backup / 'config_region.bin').read_bytes() == (restored / 'config_after.bin').read_bytes()
    ordinary = (directory.parents[1] / 'software/ordinary_051D_rebuilt.bin').read_bytes()
    application = (restored / 'application_after.bin').read_bytes()
    prefix_same = application[:len(ordinary)] == ordinary
    tail_nonff = sum(value != 255 for value in application[len(ordinary):])
    safe = (terminal['firmware'] == '0x051D' and terminal['map'] == '0x0105'
            and terminal['revision'] == terminal['saved_revision'] == 19
            and not any(terminal[k] for k in ('dirty', 'fault_mask', 'overrun_count'))
            and not any(terminal['r5'][k] for k in ('application', 'mode', 'offset_ug', 'save_request_count_low'))
            and terminal['candidate']['offset_ug'] == 0)
    paths = [dict(path=name, calls=target['counts'][i], max_cycles=target['maxima'][i],
                  max_us=target['maxima'][i] * 1e6 / target['hclk_hz'],
                  peak_sequence=target['peaks'][i]) for i, name in enumerate(PATHS)]
    result = dict(
        result='PASS' if checker.complete() == captured['status'] == 'PASS' and same_app and same_config and safe and prefix_same and not tail_nonff else 'FAIL',
        classification='DIAGNOSTIC_TARGET_DIRECT_MEASUREMENT',
        start_utc=first['utc'], end_utc=last['utc'],
        host_monotonic_duration_s=(last['host_monotonic_ns']-first['host_monotonic_ns'])/1e9,
        host_polls=polls, host_gap_events=checker.gaps,
        target_first_sequence=target['first'], target_last_sequence=target['last'],
        target_timed_calls=target['total'], target_produced=target['produced'],
        target_consumed=target['consumed'], target_engine=target['engine'], fifo=target['fifo'],
        hclk_hz=target['hclk_hz'], paths=paths,
        feed_max_us=target['max_cycles']*1e6/target['hclk_hz'],
        app_run_max_us=target['loop_max']*1e6/target['hclk_hz'],
        loop_interval_max_us=target['interval_max']*1e6/target['hclk_hz'],
        empty_bracket_cycles=target['overhead_cycles'], overhead_subtracted=False,
        runtime_untouched_bytes=target['touched']-target['static_end'],
        runtime_stack_top_to_touched_bytes=target['stack_top']-target['touched'],
        static_end=hex(target['static_end']), deepest_touched=hex(target['touched']),
        stack_top=hex(target['stack_top']), control=target['control'],
        gates=target['gates'], boost_evaluations=target['boost_samples'],
        fault=target['fault'], overrun=target['overrun'], read_errors=target['read_errors'],
        restore_application_byte_equal=same_app, restore_config_byte_equal=same_config,
        restored_prefix_byte_equal=prefix_same, restored_tail_nonff=tail_nonff,
        restored_application_sha256=hashlib.sha256(application).hexdigest().upper(),
        restored_config_sha256=hashlib.sha256((restored/'config_after.bin').read_bytes()).hexdigest().upper(),
        terminal=terminal,
        historical_A13D='FAIL; unchanged', A13B_efficacy='INCONCLUSIVE; unchanged')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = review(args.input)
    args.output.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(result, indent=2))
    raise SystemExit(0 if result['result'] == 'PASS' else 1)
