"""Read-only same-sample static/dynamic checkweigh evidence for 0x052C."""
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


def run(output, duration, interval):
    output.mkdir(parents=True)
    fields = ("utc", "host_monotonic_ns", "active_sequence", "shadow_sequence",
              "active_timestamp_ms", "r5_offset_ug", "uncompensated_gross_ug",
              "formal_gross_ug", "formal_net_ug", "tare_ug", "dynamic_input_ug",
              "inferred_fast_gross_ug", "static_input_ug", "dynamic_class",
              "static_class", "formal_state", "formal_outputs", "check_mode",
              "check_reason", "check_formal", "fault", "overrun", "revision",
              "saved_revision", "dirty")
    errors = 0
    with (output / "frames.txt").open("w", encoding="ascii") as frames, \
         (output / "samples.csv").open("w", encoding="utf-8", newline="") as data, \
         (output / "errors.jsonl").open("w", encoding="utf-8") as error_file:
        writer = csv.DictWriter(data, fieldnames=fields)
        writer.writeheader()
        end = time.monotonic() + duration
        while time.monotonic() < end:
            try:
                def log(line):
                    frames.write(f"{utc()} {time.monotonic_ns()} {line}\n")
                    frames.flush()
                with SerialTransport("COM5", 115200, "N", 1, 350,
                                     frame_logger=log) as port:
                    client = ModbusClient(port, 1)
                    active, _ = client.read(0x340, 40)
                    shadow, _ = client.read(0x2e0, 31)
                    check, _ = client.read(0x2c0, 8)
                    diag, _ = client.read(0x20, 28)
                    realtime, _ = client.read(0, 32)
                    if (realtime[14], realtime[15]) != (0x010F, 0x052C):
                        raise RuntimeError("unexpected identity")
                    active_sequence = u32(active[10:12])
                    shadow_sequence = u32(shadow[17:19])
                    offset = word32(active[14:16], "high")
                    fast = word64(shadow[5:9], "high")
                    tare = word64(active[30:34], "high")
                    writer.writerow({
                        "utc": utc(), "host_monotonic_ns": time.monotonic_ns(),
                        "active_sequence": active_sequence,
                        "shadow_sequence": shadow_sequence,
                        "active_timestamp_ms": u32(active[12:14]),
                        "r5_offset_ug": offset,
                        "uncompensated_gross_ug": word64(active[18:22], "high"),
                        "formal_gross_ug": word64(active[22:26], "high"),
                        "formal_net_ug": word64(active[26:30], "high"),
                        "tare_ug": tare,
                        "dynamic_input_ug": fast,
                        "inferred_fast_gross_ug": fast + offset + tare,
                        "static_input_ug": word64(shadow[1:5], "high"),
                        "dynamic_class": (shadow[21] >> 4) & 0xF,
                        "static_class": (shadow[21] >> 8) & 0xF,
                        "formal_state": check[5], "formal_outputs": shadow[24],
                        "check_mode": check[1], "check_reason": check[4],
                        "check_formal": check[5], "fault": u32(diag[25:27]),
                        "overrun": u32(diag[13:15]),
                        "revision": u32(diag[19:21]),
                        "saved_revision": u32(diag[21:23]), "dirty": diag[18]})
                    data.flush()
            except Exception as exc:
                errors += 1
                error_file.write(json.dumps({"utc": utc(), "error": str(exc)}) + "\n")
                error_file.flush()
            time.sleep(interval)
    digest = hashlib.sha256((output / "samples.csv").read_bytes()).hexdigest().upper()
    summary = {"rows": sum(1 for _ in (output / "samples.csv").open()) - 1,
               "read_errors": errors, "samples_sha256": digest,
               "ended_utc": utc()}
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n",
                                         encoding="utf-8")
    print(json.dumps(summary), flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--duration", type=float, default=30.0)
    parser.add_argument("--interval", type=float, default=0.12)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("output exists")
    run(args.output, args.duration, args.interval)


if __name__ == "__main__":
    main()
