import argparse
import json
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'Tools/stage5b_hw'))
sys.path.insert(0, str(ROOT / 'Tools/stage5pa13c'))
from hw_common import ModbusClient, execute_command
from serial_transport import SerialTransport
from hardware_shadow import word32, word64, now

def u32(words, index):
    return (words[index] << 16) | words[index + 1]

def sample(client):
    realtime, _ = client.read(0, 32)
    diag, _ = client.read(0x20, 28)
    active, _ = client.read(0x340, 40)
    return dict(utc=now(), host_monotonic_ns=time.monotonic_ns(),
        firmware=realtime[15], map=realtime[14], display_count=u32(realtime,0),
        gross_ug=word64(realtime[20:24], 'high'), net_ug=word64(realtime[16:20], 'high'),
        raw_adc=word32(realtime[28:30], 'high'), filtered_adc_counts=word32(realtime[30:32], 'high'),
        sample_sequence=u32(diag,0), mcu_uptime_ms=u32(diag,2), fault=u32(diag,25),
        overrun=u32(diag,13), dirty=diag[18], revision=u32(diag,19), saved_revision=u32(diag,21),
        calibration_valid=diag[27], signature=u32(active,0), generation=u32(active,2),
        exit_offset=u32(active,4), exit_reason=u32(active,6), application=u32(active,8),
        active_sequence=u32(active,10), active_mcu_ms=u32(active,12), applied_offset=u32(active,14),
        apply=u32(active,16), mode=u32(active,34), state=u32(active,36), offset=u32(active,38))

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--duration', type=float, default=900)
    parser.add_argument('--event'); parser.add_argument('--application', type=int)
    parser.add_argument('--mode', type=int); parser.add_argument('--stop', action='store_true')
    args = parser.parse_args(); args.out.mkdir(parents=True, exist_ok=True)
    requests = args.out / 'requests.jsonl'
    if args.event or args.mode is not None or args.stop:
        with requests.open('a') as stream:
            stream.write(json.dumps(dict(utc=now(), event=args.event,
                application=args.application, mode=args.mode, stop=args.stop))+'\n')
        return
    seen = 0; rows = []; deadline = time.monotonic() + args.duration
    args.out.mkdir(parents=True, exist_ok=True)
    samples = (args.out/'samples.jsonl').open('w')
    events = (args.out/'events.jsonl').open('a')
    with SerialTransport('COM5',115200,'N',1,350) as transport:
        client = ModbusClient(transport,1)
        while time.monotonic() < deadline:
            lines = requests.read_text().splitlines() if requests.exists() else []
            for line in lines[seen:]:
                request = json.loads(line); pre = sample(client)
                if request.get('stop'):
                    deadline = 0
                if request.get('mode') is not None:
                    result = execute_command(client,1800+seen,36,
                        arg0=request['application'], arg1=request['mode'],
                        arg64=pre['generation'], flags=1)
                    post = sample(client)
                    events.write(json.dumps(dict(utc=now(), kind='PAIR',
                        request=request, pre=pre, result=result, post=post))+'\n'); events.flush()
                    if result['result'] or post['application'] != request['application'] or post['mode'] != request['mode']:
                        raise RuntimeError('pair failed '+str(result))
                seen += 1
            row=sample(client); rows.append(row); samples.write(json.dumps(row)+'\n'); samples.flush(); time.sleep(0.08)
    samples.close(); events.close()
    (args.out/'summary.json').write_text(json.dumps(dict(status='PASS',rows=len(rows)),indent=2)+'\n')

if __name__ == '__main__': main()
