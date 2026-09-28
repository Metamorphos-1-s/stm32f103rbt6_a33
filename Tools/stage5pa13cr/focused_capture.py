#!/usr/bin/env python3
"""One serial owner for A13C-R evidence and authorized volatile mode commands.

Never changes configuration, ZERO/TARE/calibration/SAVE or candidate ACTIVE.
On a safety failure close COM5 and restore only the validated current backup.
"""
import argparse
import csv
import hashlib
import json
import subprocess
import sys
import time
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Tools/stage5pa13c'))
from hardware_shadow import probe, now, unsigned32, word32, word64
from hw_common import ModbusClient, execute_command
from serial_transport import SerialTransport

PROGRAMMER = 'E:/ST/STM32CubeCLT_1.18.0/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe'
APP_SHA = '08A5E26DB77914E4B41588F21AA51E1C5390606BDFD77A624B336945FA359A4F'
CFG_SHA = '856BD8F5C14760561FC4BFEC4274FE0C5480C19BC4422046617C2450F8439733'


def json_write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def rollback(output):
    """Application only. A changed config is a blocker, never overwritten."""
    app = ROOT / 'Results/stage5pa13c_r/backup_preflash_application.bin'
    config = ROOT / 'Results/stage5pa13c_r/backup_preflash_config/config_region.bin'
    if app.stat().st_size != 126976 or sha(app) != APP_SHA or sha(config) != CFG_SHA:
        raise RuntimeError('current-run backup hashes invalid; no rollback attempted')
    def cli(arguments, log):
        result = subprocess.run([PROGRAMMER] + arguments, capture_output=True)
        (output / log).write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError('programmer failed: ' + log)
    current = output / 'failure_config.bin'
    cli(['-c', 'port=SWD', 'mode=HotPlug', '-u', '0x0801F000', '0x1000', str(current)], 'failure_config.log')
    if current.read_bytes() != config.read_bytes():
        raise RuntimeError('configuration changed; no historical config overwrite allowed')
    cli(['-c', 'port=SWD', 'mode=UR', 'reset=HWrst', '-w', str(app), '0x08000000', '-v', '-rst'], 'failure_rollback.log')
    restored = output / 'restored_application.bin'
    cli(['-c', 'port=SWD', 'mode=HotPlug', '-u', '0x08000000', '0x1F000', str(restored)], 'restored_application.log')
    after = output / 'restored_config.bin'
    cli(['-c', 'port=SWD', 'mode=HotPlug', '-u', '0x0801F000', '0x1000', str(after)], 'restored_config.log')
    if restored.read_bytes() != app.read_bytes() or after.read_bytes() != config.read_bytes():
        raise RuntimeError('rollback byte verification failed')
    with SerialTransport('COM5', 115200, 'N', 1, 350) as transport:
        terminal = probe(ModbusClient(transport, 1))
    json_write(output / 'rollback_terminal.json', terminal)
    return terminal


def sample(client, order):
    r, _ = client.read(0, 64)
    c, _ = client.read(0x300, 37)
    d = r[32:64]
    return {
        'utc': now(), 'host_monotonic_ns': time.monotonic_ns(),
        'firmware': r[15], 'map': r[14], 'signature': c[0],
        'gross_ug': word64(r[20:24], order),
        'raw_adc': word32(r[28:30], order),
        'filtered_adc_counts': word32(r[30:32], order),
        'display_count': word32(r[0:2], order),
        'status_flags': r[4] | (r[5] << 16),
        'sample_sequence': unsigned32(d[0:2], order),
        'mcu_uptime_ms': unsigned32(d[2:4], order),
        'fault': unsigned32(d[25:27], order), 'overrun': unsigned32(d[13:15], order),
        'dirty': d[18], 'revision': unsigned32(d[19:21], order),
        'saved_revision': unsigned32(d[21:23], order), 'calibration_valid': d[27],
        'mode': c[1], 'state': c[2], 'reason': c[3], 'limited': c[4],
        'offset_ug': word64(c[5:9], order),
        'candidate_gross_ug': word64(c[9:13], order),
        'candidate_uncompensated_ug': word64(c[13:17], order),
        'candidate_sequence': unsigned32(c[17:19], order),
        'candidate_mcu_ms': unsigned32(c[19:21], order),
        'gate_count': unsigned32(c[21:23], order),
        'rebuild_count': unsigned32(c[23:25], order),
        'boost_samples': unsigned32(c[25:27], order),
        'obvious_sequence': unsigned32(c[27:29], order),
        'robust_sequence': unsigned32(c[29:31], order),
        'quiet_sequence': unsigned32(c[31:33], order),
        'reference_lock_sequence': unsigned32(c[33:35], order),
        'first_correction_sequence': unsigned32(c[35:37], order),
    }


def run(args):
    output = args.output
    output.mkdir(parents=True, exist_ok=True)
    if (output / 'samples.csv').exists():
        raise ValueError('refusing to overwrite existing raw capture')
    events = (output / 'events.jsonl').open('a', encoding='utf-8')
    def event(kind, **fields):
        entry = dict(utc=now(), host_monotonic_ns=time.monotonic_ns(), kind=kind, **fields)
        events.write(json.dumps(entry) + '\n')
        events.flush()
    failure = None
    rows = 0
    distinct = 0
    missing = 0
    errors = 0
    previous = None
    history = deque()
    consumed = 0
    expected_mode = 0
    expected_save = 0
    last_status = 0
    last_aux = 0
    offset_frozen = None
    deadline = time.monotonic() + args.duration
    try:
        with SerialTransport('COM5', 115200, 'N', 1, 350) as transport:
            client = ModbusClient(transport, 1)
            preflight = probe(client)
            json_write(output / 'preflight.json', preflight)
            if preflight['firmware'] != '0x051D' or preflight['map'] != '0x0105':
                raise RuntimeError('unexpected identity')
            expected_save = preflight['r5']['save_request_count_low']
            aux = dict(application=preflight['r5']['application'], save_count=expected_save,
                       checkweigh_mode=preflight['checkweigh']['mode'])
            event('RECORDER_START', data_role='FOCUSED_SHADOW_FUNCTIONAL_SAFETY')
            with (output / 'samples.csv').open('w', encoding='utf-8', newline='') as raw:
                writer = None
                while time.monotonic() < deadline:
                    request = output / 'requests.jsonl'
                    if request.exists():
                        requests = request.read_text(encoding='utf-8').splitlines()
                        for line in requests[consumed:]:
                            command = json.loads(line)
                            event('HOST_REQUEST', request=command)
                            if command.get('mode') is not None:
                                mode = int(command['mode'])
                                if mode not in (0, 1, 2):
                                    raise RuntimeError('invalid requested mode')
                                result = execute_command(client, (1000 + consumed) % 65535, 29, arg0=mode)
                                event('VOLATILE_MODE_RESULT', mode=mode, result=result)
                                if result['result'] != 0:
                                    raise RuntimeError('mode command rejected: ' + str(result))
                                expected_mode = mode
                                history.clear()
                                previous = None
                                offset_frozen = None
                            if command.get('stop'):
                                deadline = 0
                            consumed += 1
                    if deadline == 0:
                        break
                    try:
                        entry = sample(client, preflight['word_order'])
                        if time.monotonic() - last_aux >= 1.0:
                            a, _ = client.read(0x281, 1)
                            save, _ = client.read(0x2a7, 1)
                            alarm, _ = client.read(0x2c0, 8)
                            aux = dict(application=a[0], save_count=save[0], checkweigh_mode=alarm[1])
                            last_aux = time.monotonic()
                        entry.update(aux)
                        entry['sample_pair_matched'] = int(entry['sample_sequence'] == entry['candidate_sequence'])
                        entry['aux_fields_note'] = 'latest_1s_poll_not_same_sample'
                        if writer is None:
                            writer = csv.DictWriter(raw, fieldnames=list(entry))
                            writer.writeheader()
                        writer.writerow(entry)
                        raw.flush()
                        rows += 1
                        if entry['firmware'] != 0x051d or entry['map'] != 0x0105 or entry['signature'] != 0xa13c:
                            raise RuntimeError('identity/signature changed')
                        if entry['limited'] or entry['fault'] or entry['overrun'] or entry['dirty']:
                            raise RuntimeError('limited/fault/overrun/dirty safety gate')
                        if entry['revision'] != 19 or entry['saved_revision'] != 19 or entry['save_count'] != expected_save:
                            raise RuntimeError('revision/SAVE safety gate')
                        if entry['application'] != 0 or entry['mode'] != expected_mode:
                            raise RuntimeError('unexpected mode/application')
                        if abs(entry['offset_ug']) > 500000:
                            raise RuntimeError('absolute offset limit')
                        if entry['sample_pair_matched'] and entry['gross_ug'] != entry['candidate_uncompensated_ug']:
                            raise RuntimeError('authoritative weight differs from SHADOW input')
                        if expected_mode == 0 and entry['offset_ug'] != 0:
                            raise RuntimeError('nonzero OFF offset')
                        if previous is None or entry['candidate_sequence'] != previous['candidate_sequence']:
                            distinct += 1
                            if previous is not None:
                                ds = entry['candidate_sequence'] - previous['candidate_sequence']
                                dt = entry['candidate_mcu_ms'] - previous['candidate_mcu_ms']
                                if ds <= 0 or dt <= 0:
                                    raise RuntimeError('MCU reset/time reversal')
                                missing += max(0, ds - 1)
                                if expected_mode == 2 and abs(entry['offset_ug'] - previous['offset_ug']) > 35 * ds:
                                    raise RuntimeError('sample correction limit')
                            if expected_mode == 1:
                                if offset_frozen is None:
                                    offset_frozen = entry['offset_ug']
                                elif entry['offset_ug'] != offset_frozen:
                                    raise RuntimeError('DOSING offset changed')
                            history.append((entry['candidate_mcu_ms'], entry['offset_ug']))
                            while history and entry['candidate_mcu_ms'] - history[0][0] > 10000:
                                history.popleft()
                            if history and max(abs(entry['offset_ug'] - item[1]) for item in history) > 3500:
                                raise RuntimeError('10 second correction limit')
                            previous = entry
                        if time.monotonic() - last_status >= 1.0:
                            json_write(output / 'live_summary.json', dict(rows=rows, distinct_samples=distinct,
                                unobserved_sequences=missing, read_errors=errors, latest=entry,
                                status='RUNNING', requests_consumed=consumed))
                            last_status = time.monotonic()
                    except RuntimeError:
                        raise
                    except Exception as exc:
                        errors += 1
                        event('READ_ERROR', error=str(exc))
                        if errors >= 3:
                            raise RuntimeError('persistent read errors')
                    time.sleep(0.015)
            # A normal timeout/explicit stop must leave the volatile
            # engineering controller OFF+SHADOW with its offset cleared.
            event('FINAL_OFF_REQUEST')
            result = execute_command(client, 32001, 29, arg0=0)
            event('FINAL_OFF_RESULT', result=result)
            if result['result'] != 0:
                raise RuntimeError('final OFF command rejected')
            terminal = probe(client)
            if terminal['r5']['application'] != 0 or terminal['r5']['mode'] != 0 or terminal['r5']['offset_ug'] != 0:
                raise RuntimeError('unsafe terminal state')
            json_write(output / 'terminal.json', terminal)
    except Exception as exc:
        failure = str(exc)
        event('SAFETY_FAILURE', error=failure)
        try:
            rollback(output)
            event('ROLLBACK_VERIFIED')
        except Exception as recovery:
            event('ROLLBACK_BLOCKED', error=str(recovery))
    finally:
        event('RECORDER_STOP', failure=failure)
        events.close()
        result = dict(status='FAIL_ROLLED_BACK_OR_CHECK_ROLLBACK_LOG' if failure else 'STOPPED',
            failure=failure, rows=rows, distinct_samples=distinct, unobserved_sequences=missing,
            read_errors=errors, csv_sha256=sha(output / 'samples.csv') if (output / 'samples.csv').exists() else None)
        json_write(output / 'summary.json', result)
        json_write(output / 'live_summary.json', result)
        print(json.dumps(result), flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--duration', type=float, default=7200)
    parser.add_argument('--event')
    parser.add_argument('--mode', type=int, choices=(0, 1, 2))
    parser.add_argument('--stop', action='store_true')
    args = parser.parse_args()
    if args.event or args.mode is not None or args.stop:
        args.output.mkdir(parents=True, exist_ok=True)
        with (args.output / 'requests.jsonl').open('a', encoding='utf-8') as stream:
            stream.write(json.dumps(dict(utc=now(), host_monotonic_ns=time.monotonic_ns(),
                event=args.event, mode=args.mode, stop=args.stop)) + '\n')
        return
    run(args)


if __name__ == '__main__':
    main()
