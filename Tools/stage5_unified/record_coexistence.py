"""Read-only, bounded 0x052A load/unload recorder with atomic weight block."""
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
from hardware_shadow import SerialTransport, ModbusClient, probe  # noqa: E402
from modbus_frame import decode_i32_words, decode_i64_words  # noqa: E402


def stamp():
    return datetime.now(timezone.utc).isoformat()


def u32(words):
    return (words[0] << 16) | words[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--duration", type=int, default=300)
    parser.add_argument("--interval", type=float, default=0.4)
    parser.add_argument("--stop-file", type=Path)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("output directory exists")
    args.output.mkdir(parents=True)
    rows = errors = 0
    fields = ("utc", "host_monotonic_ns", "sequence", "uptime_ms",
              "uncompensated_gross_ug", "formal_gross_ug", "formal_net_ug",
              "tare_ug", "applied_offset_ug", "offset_applied", "mode",
              "state", "check_mode", "check_reason", "check_formal",
              "alarm_state", "fault", "overrun", "revision", "saved_revision")
    with (args.output / "frames.txt").open("w", encoding="ascii") as frames, \
         (args.output / "samples.csv").open("w", encoding="utf-8", newline="") as data, \
         (args.output / "errors.jsonl").open("w", encoding="utf-8") as failures:
        def log_frame(line):
            frames.write(f"{stamp()} {time.monotonic_ns()} {line}\n")
            frames.flush()
        writer = csv.DictWriter(data, fieldnames=fields)
        writer.writeheader()
        data.flush()
        with SerialTransport("COM5", 115200, "N", 1, 350,
                             frame_logger=log_frame) as port:
            client = ModbusClient(port, 1)
            initial = probe(client)
            if (initial["firmware"], initial["map"]) != ("0x052A", "0x010F"):
                raise RuntimeError("unexpected firmware identity")
            (args.output / "preflight.json").write_text(
                json.dumps(initial, indent=2) + "\n", encoding="utf-8")
            end = time.monotonic() + args.duration
            next_poll = time.monotonic()
            while time.monotonic() < end and not (args.stop_file and args.stop_file.exists()):
                try:
                    active, _ = client.read(0x340, 40)
                    check, _ = client.read(0x2c0, 8)
                    alarm, _ = client.read(0x220, 27)
                    diagnostic, _ = client.read(0x20, 28)
                    if active[0:2] != [0xA13E, 1]:
                        raise RuntimeError("A13 active signature changed")
                    entry = dict(utc=stamp(), host_monotonic_ns=time.monotonic_ns(),
                        sequence=u32(active[10:12]), uptime_ms=u32(active[12:14]),
                        uncompensated_gross_ug=decode_i64_words(active[18:22], "high"),
                        formal_gross_ug=decode_i64_words(active[22:26], "high"),
                        formal_net_ug=decode_i64_words(active[26:30], "high"),
                        tare_ug=decode_i64_words(active[30:34], "high"),
                        applied_offset_ug=decode_i32_words(active[14:16], "high"),
                        offset_applied=u32(active[16:18]),
                        mode=u32(active[34:36]), state=u32(active[36:38]),
                        check_mode=check[1], check_reason=check[4],
                        check_formal=check[5], alarm_state=alarm[17],
                        fault=u32(diagnostic[25:27]),
                        overrun=u32(diagnostic[13:15]),
                        revision=u32(diagnostic[19:21]),
                        saved_revision=u32(diagnostic[21:23]))
                    writer.writerow(entry)
                    data.flush()
                    rows += 1
                except Exception as exc:
                    errors += 1
                    failures.write(json.dumps({"utc": stamp(),
                        "host_monotonic_ns": time.monotonic_ns(),
                        "error": str(exc)}) + "\n")
                    failures.flush()
                next_poll += args.interval
                time.sleep(max(0, next_poll - time.monotonic()))
            final = probe(client)
            (args.output / "final.json").write_text(
                json.dumps(final, indent=2) + "\n", encoding="utf-8")
    summary = {"rows": rows, "errors": errors, "ended_utc": stamp(),
               "samples_sha256": hashlib.sha256(
                   (args.output / "samples.csv").read_bytes()).hexdigest().upper()}
    (args.output / "summary.json").write_text(
        json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary), flush=True)


if __name__ == "__main__":
    main()
