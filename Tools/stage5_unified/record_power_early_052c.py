"""Reconnectable read-only recorder for the 0x052C power-cycle boundary."""
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
from hardware_shadow import SerialTransport, ModbusClient, word64  # noqa: E402


def utc(): return datetime.now(timezone.utc).isoformat()
def u32(w): return (w[0] << 16) | w[1]


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--duration", type=float, default=600.0)
    p.add_argument("--stop-file", type=Path, required=True)
    a = p.parse_args()
    a.output.mkdir(parents=True)
    fields = ("utc", "host_monotonic_ns", "sequence", "uptime_ms", "r5_application",
              "r5_mode", "r5_state", "r5_offset_ug", "check_mode", "check_reason",
              "check_formal", "formal_outputs", "revision", "saved_revision", "dirty",
              "fault", "overrun", "storage_slot", "storage_sequence")
    rows = errors = 0
    with (a.output / "frames.txt").open("w", encoding="ascii") as frames, \
         (a.output / "samples.csv").open("w", encoding="utf-8", newline="") as data, \
         (a.output / "errors.jsonl").open("w", encoding="utf-8") as ef:
        writer = csv.DictWriter(data, fieldnames=fields); writer.writeheader(); data.flush()
        deadline = time.monotonic() + a.duration
        port = None
        try:
            while time.monotonic() < deadline and not a.stop_file.exists():
                try:
                    if port is None:
                        def log(line):
                            frames.write(f"{utc()} {time.monotonic_ns()} {line}\n")
                            frames.flush()
                        port = SerialTransport("COM5", 115200, "N", 1, 350,
                                               frame_logger=log)
                        client = ModbusClient(port, 1)
                    diag, _ = client.read(0x20, 28)
                    r5, _ = client.read(0x280, 40)
                    check, _ = client.read(0x2c0, 8)
                    shadow, _ = client.read(0x2e0, 31)
                    storage, _ = client.read(0x1c0, 4)
                    if r5[0] != 0x55B5:
                        raise RuntimeError("R5 signature")
                    writer.writerow({"utc": utc(), "host_monotonic_ns": time.monotonic_ns(),
                        "sequence": u32(diag[0:2]), "uptime_ms": u32(diag[2:4]),
                        "r5_application": r5[1], "r5_mode": r5[2], "r5_state": r5[3],
                        "r5_offset_ug": word64(r5[6:10], "high"),
                        "check_mode": check[1], "check_reason": check[4],
                        "check_formal": check[5], "formal_outputs": shadow[24],
                        "revision": u32(diag[19:21]), "saved_revision": u32(diag[21:23]),
                        "dirty": diag[18], "fault": u32(diag[25:27]),
                        "overrun": u32(diag[13:15]), "storage_slot": storage[1],
                        "storage_sequence": u32(storage[2:4])})
                    data.flush(); rows += 1
                except Exception as exc:
                    errors += 1
                    ef.write(json.dumps({"utc": utc(), "error": str(exc)}) + "\n")
                    ef.flush()
                    if port is not None:
                        port.close()
                        port = None
                time.sleep(0.02)
        finally:
            if port is not None:
                port.close()
    summary = {"rows": rows, "read_errors": errors, "ended_utc": utc(),
               "samples_sha256": hashlib.sha256((a.output / "samples.csv").read_bytes()).hexdigest().upper()}
    (a.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary), flush=True)


if __name__ == "__main__": main()
