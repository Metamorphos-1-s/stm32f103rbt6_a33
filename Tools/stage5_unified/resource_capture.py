"""Cumulative 40 Hz target-resource capture for the 0x0525 diagnostic image."""
import argparse
import csv
import hashlib
import json
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Tools/stage5pa13dr'))
sys.path.insert(0, str(ROOT / 'Tools/stage5pa13c'))
from capture import read
from hardware_shadow import SerialTransport, ModbusClient, now, unsigned32
from hw_common import execute_command
from contract import Checker, PATHS
from hardware_resource import restore_current


def append(path, obj):
    with path.open('a', encoding='utf-8') as stream:
        stream.write(json.dumps(obj) + '\n')


def run(args):
    out = args.output
    out.mkdir(parents=True, exist_ok=True)
    if (out / 'snapshots.jsonl').exists():
        raise ValueError('raw evidence already exists')
    backup = args.backup
    checker = Checker(allow_expected_profile_change=args.allow_profile_revision_change,
                      baseline_revision=args.baseline_revision,
                      allow_active=args.allow_active)
    seen = polls = 0; error = None; last = None
    application = mode = 0; transition_deadline = 0
    deadline = time.monotonic() + args.duration

    def event(kind, **data):
        append(out / 'events.jsonl', dict(utc=now(), host_monotonic_ns=time.monotonic_ns(), kind=kind, **data))

    try:
        with SerialTransport('COM5', 115200, 'N', 1, 350) as port:
            client = ModbusClient(port, 1)
            with (out / 'snapshots.jsonl').open('w', encoding='utf-8') as frames, \
                 (out / 'polls.csv').open('w', newline='', encoding='utf-8') as csvfile:
                writer = None
                event('CAPTURE_START', identity='0x0526/0x010E', rate='40Hz', five_blocks=[[0,32],[32,28],[448,10],[640,40],[896,122]])
                while time.monotonic() < deadline:
                    request_file = out / 'requests.jsonl'
                    requests = request_file.read_text(encoding='utf-8').splitlines() if request_file.exists() else []
                    for line in requests[seen:]:
                        request = json.loads(line); event('HOST_REQUEST', request=request)
                        if request.get('mode') is not None:
                            target, safety, original = read(client)
                            words, _ = client.read(0x0342, 2)
                            generation = unsigned32(words, 'high')
                            event('PAIR_PRECONDITION', generation=generation, application=safety['application'],
                                  mode=target['mode'], offset_ug=original['beta_offset_ug'],
                                  fault=target['fault'], overrun=target['overrun'])
                            response = execute_command(client, 2600 + seen, 36,
                                                       arg0=int(request['application']), arg1=int(request['mode']),
                                                       arg64=generation, flags=1)
                            post_target, post_safety, post_original = read(client)
                            checker.record_revision_transaction(
                                'a13_pair', safety['revision'], post_safety['revision'],
                                safety['saved_revision'], post_safety['saved_revision'],
                                (post_safety['revision'] != safety['revision']))
                            event('PAIR_RESULT', result=response, expected_generation=generation,
                                  post_application=post_safety['application'], post_mode=post_target['mode'],
                                  post_revision=post_safety['revision'], post_saved_revision=post_safety['saved_revision'])
                            if response['result']:
                                raise ValueError('pair request rejected: ' + str(response))
                            application = int(request['application']); mode = int(request['mode'])
                            transition_deadline = time.monotonic() + 1.0
                        if request.get('stop'):
                            deadline = 0
                        seen += 1
                    if not deadline:
                        break
                    target, safety, original = read(client)
                    stamp = time.monotonic_ns()
                    frame = dict(utc=now(), host_monotonic_ns=stamp, target=target, safety=safety, original=original)
                    frames.write(json.dumps(frame) + '\n'); frames.flush(); last = frame
                    flat = dict(utc=frame['utc'], host_monotonic_ns=stamp, firmware=safety['firmware'], map=safety['map'],
                                application=safety['application'], mode=target['mode'], sample_sequence=target['last'],
                                mcu_ms=target['now_ms'], gross_ug=original['gross_ug'],
                                filtered_adc_counts=original['filtered_adc_counts'], display_count=original['display_count'],
                                candidate_offset_ug=original['beta_offset_ug'], cumulative_peak_cycles=target['max_cycles'],
                                loop_peak_cycles=target['loop_max'], stack_untouched_bytes=target['touched']-target['static_end'],
                                coverage=target['coverage'], read_errors=target['read_errors'], overrun=target['overrun'], fault=target['fault'])
                    for i, path in enumerate(PATHS):
                        flat[path + '_count'] = target['counts'][i]
                        flat[path + '_max_cycles'] = target['maxima'][i]
                        flat[path + '_peak_sequence'] = target['peaks'][i]
                    if writer is None:
                        writer = csv.DictWriter(csvfile, fieldnames=list(flat)); writer.writeheader()
                    writer.writerow(flat); csvfile.flush(); polls += 1
                    checker.check(target, stamp, safety)
                    if original['beta_limited'] or (target['mode'] != mode and time.monotonic() > transition_deadline):
                        raise ValueError('unexpected LIMITED/mode')
    except Exception as exc:
        error = str(exc); event('CAPTURE_FAILURE', error=error)
    finally:
        event('CAPTURE_END', error=error)
        if last:
            (out / 'final_target_snapshot.json').write_text(json.dumps(last, indent=2) + '\n', encoding='utf-8')
        try:
            terminal = restore_current(backup, out / 'restore')
            event('EXACT_CURRENT_IMAGE_RESTORED', firmware=terminal['firmware'], map=terminal['map'])
        except Exception as exc:
            error = (error or '') + '; restore: ' + str(exc)
            event('RESTORE_FAILURE', error=str(exc))
        result = dict(status='FAIL' if error else checker.complete(), error=error, polls=polls,
                      host_gap_events=checker.gaps,
                      snapshots_sha256=hashlib.sha256((out / 'snapshots.jsonl').read_bytes()).hexdigest().upper()
                          if (out / 'snapshots.jsonl').exists() else None,
                      polls_sha256=hashlib.sha256((out / 'polls.csv').read_bytes()).hexdigest().upper()
                          if (out / 'polls.csv').exists() else None,
                      note='MCU cumulative maxima preserve calls across host polling gaps; no interpolation')
        (out / 'summary.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
        print(json.dumps(result), flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--backup', type=Path, required=True)
    parser.add_argument('--duration', type=float, default=1800)
    parser.add_argument('--allow-profile-revision-change', action='store_true')
    parser.add_argument('--baseline-revision', type=int, default=19)
    parser.add_argument('--allow-active', action='store_true')
    parser.add_argument('--event')
    parser.add_argument('--application', type=int, choices=(0, 1))
    parser.add_argument('--mode', type=int, choices=(0, 1, 2))
    parser.add_argument('--stop', action='store_true')
    args = parser.parse_args()
    if args.event or args.mode is not None or args.stop:
        args.output.mkdir(parents=True, exist_ok=True)
        append(args.output / 'requests.jsonl', dict(utc=now(), host_monotonic_ns=time.monotonic_ns(),
                                                     event=args.event, mode=args.mode, application=args.application,
                                                     stop=args.stop))
    else:
        run(args)


if __name__ == '__main__':
    main()
