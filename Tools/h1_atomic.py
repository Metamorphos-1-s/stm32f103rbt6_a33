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
from hardware_shadow import now

def u32(w, i): return (w[i] << 16) | w[i + 1]
def s64(hi, lo): return ((hi << 32) | lo) if hi < 0x80000000 else ((hi - 0x100000000) << 32) | lo
def s32(value): return value if value < 0x80000000 else value - 0x100000000
def read_atomic(client):
    a, _ = client.read(0x340, 40)
    v = lambda i: u32(a, i)
    return dict(utc=now(), host_monotonic_ns=time.monotonic_ns(), signature=v(0), generation=v(2),
        exit_offset=s32(v(4)), exit_reason=v(6), application=v(8), sequence=v(10), mcu_ms=v(12),
        applied_offset=s32(v(14)), apply=v(16), uncompensated_gross=s64(v(18), v(20)),
        formal_gross=s64(v(22), v(24)), formal_net=s64(v(26), v(28)), tare=s64(v(30), v(32)),
        mode=v(34), state=v(36), offset=s32(v(38)))

def main():
    p = argparse.ArgumentParser(); p.add_argument('--out', type=Path, required=True)
    p.add_argument('--duration', type=float, default=900); p.add_argument('--event')
    p.add_argument('--application', type=int); p.add_argument('--mode', type=int); p.add_argument('--stop', action='store_true')
    args = p.parse_args(); args.out.mkdir(parents=True, exist_ok=True); requests = args.out / 'requests.jsonl'
    if args.event or args.mode is not None or args.stop:
        with requests.open('a') as f: f.write(json.dumps(dict(utc=now(), event=args.event, application=args.application, mode=args.mode, stop=args.stop)) + '\n')
        return
    if (args.out/'samples.jsonl').exists():
        raise ValueError('existing raw record: refusing overwrite')
    seen = 0; deadline = time.monotonic() + args.duration
    with SerialTransport('COM5', 115200, 'N', 1, 350) as port:
        client = ModbusClient(port, 1)
        with (args.out / 'samples.jsonl').open('w') as samples, (args.out / 'events.jsonl').open('a') as events:
            while time.monotonic() < deadline:
                lines = requests.read_text().splitlines() if requests.exists() else []
                for line in lines[seen:]:
                    request = json.loads(line); pre = read_atomic(client)
                    if request.get('stop'): deadline = 0
                    if request.get('mode') is not None:
                        previous,_=client.read(0x4c,1)
                        token=(previous[0]+1)&65535 or 1
                        result = execute_command(client, token, 36, arg0=request['application'], arg1=request['mode'], arg64=pre['generation'], flags=1)
                        post = read_atomic(client); events.write(json.dumps(dict(kind='PAIR', request=request, pre=pre, result=result, post=post)) + '\n'); events.flush()
                        if result['result'] or post['application'] != request['application'] or post['mode'] != request['mode']: raise RuntimeError('pair failed')
                    seen += 1
                if not deadline: break
                samples.write(json.dumps(read_atomic(client)) + '\n'); samples.flush(); time.sleep(0.08)

if __name__ == '__main__': main()
