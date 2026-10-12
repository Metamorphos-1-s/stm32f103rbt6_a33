"""Timestamped read-only COM5 capture; no command mailbox or writes."""
import argparse
import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'stage5pa13c'))
from hardware_shadow import SerialTransport, ModbusClient, probe


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=720)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    end = time.monotonic() + args.seconds
    errors = 0
    count = 0
    first = last = None
    with (args.out / 'samples.jsonl').open('x', encoding='utf-8') as samples, \
         (args.out / 'events.jsonl').open('x', encoding='utf-8') as events, \
         SerialTransport('COM5', 115200, 'N', 1, 350) as port:
        client = ModbusClient(port, 1)
        while time.monotonic() < end:
            try:
                sample = probe(client)
                config, _ = client.read(0x100, 64)
                sample['configured_brightness'] = config[0x16]
                sample['unit'] = config[1]
                sample['status_raw'], _ = client.read(4, 2)
                sample['stable'] = bool(sample['status_raw'][0] & (1 << 4))
                sample['battery_adc_raw'] = None
                sample['battery_corrected_mv'] = None
                sample['actual_display_brightness'] = None
                sample['panel_text'] = None
                samples.write(json.dumps(sample) + '\n')
                samples.flush()
                count += 1
                first = first or sample
                last = sample
            except Exception as exc:
                errors += 1
                events.write(json.dumps(dict(host_monotonic_ns=time.monotonic_ns(),
                                             kind='READ_ERROR', error=str(exc))) + '\n')
                events.flush()
            time.sleep(0.8)
    (args.out / 'summary.json').write_text(json.dumps(dict(samples=count, read_errors=errors,
        first_utc=first['utc'] if first else None, last_utc=last['utc'] if last else None,
        note='Battery ADC, corrected voltage, actual brightness and panel text are not public Modbus fields.'),
        indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
