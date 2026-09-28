"""Five-block MCU timing/watermark acquisition; actual modes, no tuning."""
import argparse
import csv
import hashlib
import json
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Tools/stage5pa13c'))
from hardware_shadow import SerialTransport, ModbusClient, now, unsigned32, word64, word32
from hw_common import execute_command
from restore_current import restore


def read(client):
    r, _ = client.read(0, 32)
    d, _ = client.read(0x20, 28)
    storage, _ = client.read(0x1c0, 10)
    beta, _ = client.read(0x280, 40)
    full, _ = client.read(0x300, 108)
    c, metrics = full[:37], full[64:108]
    order = 'high'  # frozen preflight is independently confirmed high-word-first
    m = [unsigned32(metrics[i:i+2], order) for i in range(0, 44, 2)]
    return dict(utc=now(), host_monotonic_ns=time.monotonic_ns(),
        firmware=r[15], map=r[14], signature=c[0],
        gross_ug=word64(r[20:24], order), raw_adc=word32(r[28:30], order),
        filtered_adc_counts=word32(r[30:32], order), display_count=word32(r[:2], order),
        status_flags=r[4] | (r[5] << 16), realtime_sequence=unsigned32(d[:2], order),
        sequence=unsigned32(c[17:19], order), mcu_ms=unsigned32(c[19:21], order),
        mode=c[1], state=c[2], reason=c[3], limited=c[4],
        offset_ug=word64(c[5:9], order), candidate_ug=word64(c[9:13], order),
        input_ug=word64(c[13:17], order), gates=unsigned32(c[21:23], order),
        boost_samples=unsigned32(c[25:27], order),
        fault=unsigned32(d[25:27], order), dirty=d[18],
        revision=unsigned32(d[19:21], order), saved_revision=unsigned32(d[21:23], order),
        calibration_valid=d[27], driver_state=d[11], save_count=beta[39], application=beta[1],
        hclk_hz=m[0], call_cycles=m[2], call_sequence=m[3], reserved_word_4=m[4],
        loop_max_cycles=m[7], loop_interval_max_cycles=m[8],
        lowest_touched=m[10], flags=m[11] & 255, overhead_max_cycles=m[11] >> 8,
        static_end=m[12], stack_top=m[13], control=m[14],
        produced=m[15], consumed=m[16], invalid=m[17], fifo=m[18],
        driver_read_errors=m[19], overrun=m[20], engine_sequence=m[21],
        persistent_format=storage[0], storage_slot=storage[1])


def run(args):
    output = args.output
    output.mkdir(parents=True, exist_ok=True)
    if (output / 'samples.csv').exists():
        raise RuntimeError('refusing to overwrite raw acquisition')
    event_file = (output / 'events.jsonl').open('a', encoding='utf-8')
    def event(kind, **kwargs):
        event_file.write(json.dumps(dict(utc=now(), host_monotonic_ns=time.monotonic_ns(),
                                        kind=kind, **kwargs)) + '\n')
        event_file.flush()
    failure = None
    rows = 0
    unique = 0
    prior = None
    seen = 0
    errors = 0
    mode = 0
    started = False
    backlog_since = None
    latest_write = 0
    deadline = time.monotonic() + args.duration
    try:
        with SerialTransport('COM5', 115200, 'N', 1, 350) as transport:
            client = ModbusClient(transport, 1)
            with (output / 'samples.csv').open('w', newline='', encoding='utf-8') as stream:
                writer = None
                event('CAPTURE_BEGIN', five_blocks=[[0,32],[32,28],[448,10],[640,40],[768,108]])
                while time.monotonic() < deadline:
                    requests = output / 'requests.jsonl'
                    lines = requests.read_text().splitlines() if requests.exists() else []
                    for line in lines[seen:]:
                        request = json.loads(line)
                        event('HOST_REQUEST', request=request)
                        if request.get('mode') is not None:
                            result = execute_command(client, 1200 + seen, 29, arg0=int(request['mode']))
                            event('MODE_RESULT', result=result)
                            if result['result']:
                                raise RuntimeError('volatile mode rejected')
                            mode = int(request['mode'])
                        if request.get('stop'):
                            deadline = 0
                        seen += 1
                    if not deadline:
                        break
                    try:
                        sample = read(client)
                    except Exception as exc:
                        errors += 1
                        event('READ_ERROR', error=str(exc))
                        raise RuntimeError('read interruption: ' + str(exc))
                    if writer is None:
                        writer = csv.DictWriter(stream, fieldnames=list(sample))
                        writer.writeheader()
                    writer.writerow(sample)
                    stream.flush()
                    rows += 1
                    if (sample['firmware'], sample['map'], sample['signature']) != (0x051e, 0x0106, 0xa13c):
                        raise RuntimeError('identity mismatch')
                    if sample['flags'] != 1 or sample['control'] & 2:
                        raise RuntimeError('paint/DWT/sentinel/MSP validity failure')
                    if sample['lowest_touched'] - sample['static_end'] < 512:
                        raise RuntimeError('runtime RAM margin below 512')
                    if sample['fault'] or sample['overrun'] or sample['driver_read_errors'] or sample['dirty']:
                        raise RuntimeError('fault/overrun/read-error/dirty')
                    if sample['revision'] != 19 or sample['saved_revision'] != 19 or sample['save_count']:
                        raise RuntimeError('configuration/SAVE invariant changed')
                    if sample['mode'] != mode or sample['application']:
                        raise RuntimeError('unexpected mode/application')
                    hclk = sample['hclk_hz']
                    if hclk < 1000000 or sample['call_cycles'] * 100 > hclk:
                        raise RuntimeError('A13C 10ms budget')
                    if sample['loop_max_cycles'] * 40 > hclk or sample['loop_interval_max_cycles'] * 40 > hclk:
                        raise RuntimeError('main loop 25ms budget')
                    if not started and sample['sequence'] > 0 and sample['status_flags'] & 8 and sample['call_sequence'] == sample['sequence'] and sample['driver_state'] == 4 and sample['calibration_valid']:
                        started = True
                        event('QUALIFICATION_START', sequence=sample['sequence'], mcu_ms=sample['mcu_ms'])
                    if started:
                        if sample['produced'] - sample['consumed'] != sample['fifo']:
                            raise RuntimeError('driver/FIFO/bridge conservation')
                        if sample['engine_sequence'] != sample['consumed'] - sample['invalid']:
                            raise RuntimeError('engine/bridge conservation')
                        if sample['sequence'] != sample['call_sequence']:
                            raise RuntimeError('call/candidate sequence mismatch')
                        if sample['fifo'] > 1:
                            backlog_since = backlog_since or time.monotonic()
                            if time.monotonic() - backlog_since > 1:
                                raise RuntimeError('sustained FIFO backlog')
                        else:
                            backlog_since = None
                        if prior is None or sample['sequence'] != prior['sequence']:
                            if prior is not None and (sample['sequence'] != prior['sequence'] + 1 or not 0 < sample['mcu_ms'] - prior['mcu_ms'] <= 250):
                                raise RuntimeError('unobserved sequence/reset/time gap')
                            unique += 1
                            prior = sample
                    if time.monotonic() - latest_write >= 1:
                        (output / 'live_summary.json').write_text(json.dumps(dict(status='RUNNING',
                            rows=rows, qualified_unique_samples=unique, read_errors=errors,
                            requests_consumed=seen, latest=sample), indent=2) + '\n')
                        latest_write = time.monotonic()
    except Exception as exc:
        failure = str(exc)
        event('MEASUREMENT_FAILURE', error=failure)
    finally:
        event('CAPTURE_END', failure=failure)
        try:
            restore(output / 'restore')
            event('EXACT_051D_RESTORATION_VERIFIED')
        except Exception as exc:
            event('RESTORE_BLOCKED', error=str(exc))
            failure = (failure or '') + '; restore: ' + str(exc)
        result = dict(status='FAIL' if failure else 'STOPPED_RESTORED_PENDING_SCORING', failure=failure,
            raw_polls=rows, qualified_unique_samples=unique, read_errors=errors,
            csv_sha256=hashlib.sha256((output / 'samples.csv').read_bytes()).hexdigest().upper())
        (output / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
        (output / 'live_summary.json').write_text(json.dumps(result, indent=2) + '\n')
        event_file.close()
        print(json.dumps(result), flush=True)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--duration', type=float, default=7200)
    p.add_argument('--event')
    p.add_argument('--mode', type=int, choices=(0,1,2))
    p.add_argument('--stop', action='store_true')
    args = p.parse_args()
    if args.event or args.mode is not None or args.stop:
        args.output.mkdir(parents=True, exist_ok=True)
        with (args.output / 'requests.jsonl').open('a') as f:
            f.write(json.dumps(dict(utc=now(), host_monotonic_ns=time.monotonic_ns(),
                event=args.event, mode=args.mode, stop=args.stop)) + '\n')
    else:
        run(args)


if __name__ == '__main__':
    main()
