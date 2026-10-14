"""Read-only atomic status/net snapshots for the 0x052C zero lamp gate."""
import argparse
import csv
import hashlib
import json
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "stage5pa13c"))
from hardware_shadow import SerialTransport, ModbusClient, word32, word64  # noqa: E402


def utc():
    return datetime.now(timezone.utc).isoformat()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--duration", type=float, default=60.0)
    args = parser.parse_args()
    args.output.mkdir(parents=True)
    fields = ("utc", "host_monotonic_ns", "sequence", "uptime_ms",
              "display_count", "weight_view", "net_ug", "gross_ug", "tare_ug",
              "status_low_word", "status_high_word", "stable", "zero",
              "tare_active", "fault", "overrun")
    rows = errors = 0
    with (args.output / "frames.txt").open("w", encoding="ascii") as frames, \
         (args.output / "samples.csv").open("w", encoding="utf-8", newline="") as data, \
         (args.output / "errors.jsonl").open("w", encoding="utf-8") as failures:
        def log(line):
            frames.write(f"{utc()} {time.monotonic_ns()} {line}\n")
            frames.flush()
        writer = csv.DictWriter(data, fieldnames=fields)
        writer.writeheader()
        data.flush()
        with SerialTransport("COM5", 115200, "N", 1, 350,
                             frame_logger=log) as port:
            client = ModbusClient(port, 1)
            end = time.monotonic() + args.duration
            while time.monotonic() < end:
                try:
                    words, _ = client.read(0, 64)
                    if (words[14], words[15]) != (0x010F, 0x052C):
                        raise RuntimeError("unexpected firmware identity")
                    lo = words[4]
                    entry = dict(utc=utc(), host_monotonic_ns=time.monotonic_ns(),
                        sequence=(words[32] << 16) | words[33],
                        uptime_ms=(words[34] << 16) | words[35],
                        display_count=word32(words[0:2], "high"),
                        weight_view=words[13],
                        net_ug=word64(words[16:20], "high"),
                        gross_ug=word64(words[20:24], "high"),
                        tare_ug=word64(words[24:28], "high"),
                        status_low_word=lo, status_high_word=words[5],
                        stable=int(bool(lo & 0x10)), zero=int(bool(lo & 0x20)),
                        tare_active=int(bool(lo & 0x40)),
                        fault=(words[57] << 16) | words[58],
                        overrun=(words[45] << 16) | words[46])
                    writer.writerow(entry)
                    data.flush()
                    rows += 1
                except Exception as exc:
                    failures.write(json.dumps({"utc": utc(), "error": str(exc)}) + "\n")
                    failures.flush()
                    errors += 1
                time.sleep(0.08)
    summary = dict(rows=rows, read_errors=errors, ended_utc=utc(),
        samples_sha256=hashlib.sha256((args.output / "samples.csv").read_bytes()).hexdigest().upper())
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary), flush=True)


if __name__ == "__main__":
    main()
