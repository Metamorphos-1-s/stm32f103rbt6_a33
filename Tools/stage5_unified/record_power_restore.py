"""Read-only bounded 0x052B power-cycle and checkweigh reacquisition log."""
import argparse
import csv
import hashlib
import json
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools/stage5b_hw"))
sys.path.insert(0, str(ROOT / "Tools/stage5pa13c"))
from hardware_shadow import SerialTransport, ModbusClient, word32, word64  # noqa: E402


def utc():
    return datetime.now(timezone.utc).isoformat()


def u32(words):
    return (words[0] << 16) | words[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--duration", type=int, default=300)
    parser.add_argument("--stop-file", type=Path)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("output exists")
    args.output.mkdir(parents=True)
    fields = ("utc", "host_monotonic_ns", "firmware", "map", "sequence",
              "uptime_ms", "r5_application", "r5_mode", "r5_state",
              "r5_offset_ug", "applied_offset_ug", "offset_applied",
              "check_mode", "check_reason", "check_formal", "check_flags",
              "revision", "saved_revision", "dirty", "fault", "overrun",
              "storage_slot", "storage_sequence")
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
            while time.monotonic() < end and not (args.stop_file and args.stop_file.exists()):
                try:
                    realtime, _ = client.read(0, 16)
                    diag, _ = client.read(0x20, 28)
                    r5, _ = client.read(0x280, 10)
                    check, _ = client.read(0x2c0, 8)
                    active, _ = client.read(0x340, 18)
                    storage, _ = client.read(0x1c0, 4)
                    if (realtime[14], realtime[15]) != (0x010F, 0x052B):
                        raise RuntimeError("identity changed")
                    entry = dict(utc=utc(), host_monotonic_ns=time.monotonic_ns(),
                        firmware=realtime[15], map=realtime[14],
                        sequence=u32(diag[0:2]), uptime_ms=u32(diag[2:4]),
                        r5_application=r5[1], r5_mode=r5[2], r5_state=r5[3],
                        r5_offset_ug=word64(r5[6:10], "high"),
                        applied_offset_ug=word32(active[14:16], "high"),
                        offset_applied=u32(active[16:18]),
                        check_mode=check[1], check_reason=check[4],
                        check_formal=check[5], check_flags=check[6],
                        revision=u32(diag[19:21]), saved_revision=u32(diag[21:23]),
                        dirty=diag[18], fault=u32(diag[25:27]),
                        overrun=u32(diag[13:15]),
                        storage_slot=storage[1], storage_sequence=u32(storage[2:4]))
                    writer.writerow(entry)
                    data.flush()
                    rows += 1
                except Exception as exc:
                    failures.write(json.dumps(dict(utc=utc(),
                        host_monotonic_ns=time.monotonic_ns(),
                        error=str(exc))) + "\n")
                    failures.flush()
                    errors += 1
                time.sleep(0.08)
    summary = dict(rows=rows, read_errors=errors, ended_utc=utc(),
        samples_sha256=hashlib.sha256(
            (args.output / "samples.csv").read_bytes()).hexdigest().upper())
    (args.output / "summary.json").write_text(
        json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary), flush=True)


if __name__ == "__main__":
    main()
