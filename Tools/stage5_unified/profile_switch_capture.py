"""Capture one mailbox profile switch with raw RTU frames and async readback."""
import argparse
import json
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Tools/stage5pa13c'))
sys.path.insert(0, str(ROOT / 'Tools/stage5b_hw'))
from hardware_shadow import SerialTransport, ModbusClient, now, probe
from modbus_frame import registers_from_read_response
from profile_switch_contract import MAILBOX_COMMAND_SWITCH_PROFILE


def raw(exchange):
    return dict(tx=exchange.tx.hex().upper(), rx=exchange.rx.hex().upper())


def run(output, requested=1, timeout=12.0):
    output.parent.mkdir(parents=True, exist_ok=True)
    events = []
    with SerialTransport('COM5', 115200, 'N', 1, 350) as port:
        client = ModbusClient(port, 1)
        pre = probe(client)
        events.append(dict(kind='PRE', utc=now(), monotonic_ns=time.monotonic_ns(), probe=pre))
        token = (int(time.time() * 1000) & 0xFFFF) or 1
        values = [token, MAILBOX_COMMAND_SWITCH_PROFILE, 0, requested, 0, 0,
                  0, 0, 0, 0, 0]
        exchange = client.write_multiple(0x0040, values)
        events.append(dict(kind='REQUEST_FC16', utc=now(), monotonic_ns=time.monotonic_ns(),
                           command=MAILBOX_COMMAND_SWITCH_PROFILE, token=token, arg0=requested,
                           arg1=0, arg64=0, flags=0, raw=raw(exchange)))
        exchange = client.write_single(0x004B, 0xA55A)
        events.append(dict(kind='EXECUTE_FC06', utc=now(), monotonic_ns=time.monotonic_ns(), raw=raw(exchange)))
        deadline = time.monotonic() + timeout
        result = None
        while time.monotonic() < deadline:
            words, exchange = client.read(0x004C, 4)
            parsed = registers_from_read_response(exchange.rx, 1)
            events.append(dict(kind='RESPONSE_FC03', utc=now(), monotonic_ns=time.monotonic_ns(),
                               raw=raw(exchange), words=words))
            if words[0] == token:
                result = dict(token=words[0], result=words[1], state=words[2], last_command=words[3])
                break
            time.sleep(0.05)
        events.append(dict(kind='RESULT', result=result))
        samples = []
        while time.monotonic() < deadline + 0.5:
            snap = probe(client)
            cs, ex = client.read(0x002B, 2)
            snap['cs1237_state'] = cs[0]
            snap['cs1237_buffered'] = cs[1]
            samples.append(dict(utc=now(), monotonic_ns=time.monotonic_ns(), probe=snap,
                                cs_raw=raw(ex)))
            if snap['configuration']['active_profile'] == requested:
                break
            time.sleep(0.1)
        events.append(dict(kind='ASYNC_READBACK', samples=samples))
    output.write_text(json.dumps(dict(requested_profile=requested, events=events), indent=2) + '\n', encoding='utf-8')
    return events


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    run(args.output)
