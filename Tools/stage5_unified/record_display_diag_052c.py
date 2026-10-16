"""Read-only display-chain recorder; separate FC03 blocks are not atomic."""
import argparse
import csv
import hashlib
import json
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools/stage5pa13c"))
from hardware_shadow import SerialTransport, ModbusClient, word32, word64

FIELDS = ("utc", "host_monotonic_ns", "sequence", "uptime_ms", "raw_adc",
          "filtered_raw", "net_ug", "gross_ug", "display_state", "display_locked",
          "display_mass_ug", "display_anchor_ug", "display_threshold_ug",
          "candidate_elapsed_ms", "release_reason", "r5_application", "r5_mode",
          "r5_state", "r5_offset_ug", "stable", "fault", "overrun",
          "display_count", "weight_view")

class IdentityChanged(RuntimeError):
    pass

def utc():
    return datetime.now(timezone.utc).isoformat()

def collect_row(client, order, firmware):
    realtime, _ = client.read(0, 32)
    diag, _ = client.read(0x20, 28)
    disp, _ = client.read(0x1e0, 17)
    r5, _ = client.read(0x280, 40)
    if (realtime[14], realtime[15]) != (0x010F, firmware):
        raise IdentityChanged("identity changed")
    return dict(
        utc=utc(), host_monotonic_ns=time.monotonic_ns(),
        sequence=word32(diag[0:2], order) & 0xffffffff,
        uptime_ms=word32(diag[2:4], order) & 0xffffffff,
        raw_adc=word32(realtime[28:30], order),
        filtered_raw=word32(realtime[30:32], order),
        net_ug=word64(realtime[16:20], order),
        gross_ug=word64(realtime[20:24], order),
        display_state=disp[0], display_locked=disp[1],
        display_mass_ug=word64(disp[2:6], order),
        display_anchor_ug=word64(disp[6:10], order),
        display_threshold_ug=word64(disp[10:14], order),
        candidate_elapsed_ms=word32(disp[14:16], order),
        release_reason=disp[16], r5_application=r5[1], r5_mode=r5[2],
        r5_state=r5[3], r5_offset_ug=word64(r5[6:10], order),
        stable=int(bool(realtime[4] & 0x10)),
        fault=word32(diag[25:27], order) & 0xffffffff,
        overrun=word32(diag[13:15], order) & 0xffffffff,
        display_count=word32(realtime[0:2], order), weight_view=realtime[13])

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--duration", type=float, default=600)
    parser.add_argument("--stop-file", type=Path)
    parser.add_argument("--firmware", type=lambda v: int(v, 0), default=0x052C)
    parser.add_argument("--port", default="COM5")
    parser.add_argument("--word-order", choices=("high", "low"), default="high")
    args = parser.parse_args()
    if args.duration <= 0:
        parser.error("duration must be positive")
    args.output.mkdir(parents=True)
    rows = errors = 0
    stopped_reason = "DURATION_OR_STOP_FILE"
    with (args.output / "frames.txt").open("w", encoding="ascii") as frames, \
            (args.output / "samples.csv").open("w", encoding="utf-8", newline="") as data, \
            (args.output / "errors.jsonl").open("w", encoding="utf-8") as error_file:
        def log(line):
            frames.write(f"{utc()} {time.monotonic_ns()} {line}\n")
            frames.flush()
        writer = csv.DictWriter(data, fieldnames=FIELDS)
        writer.writeheader()
        data.flush()
        end = time.monotonic() + args.duration
        with SerialTransport(args.port, 115200, "N", 1, 350, frame_logger=log) as port:
            client = ModbusClient(port, 1)
            while time.monotonic() < end and not (args.stop_file and args.stop_file.exists()):
                try:
                    writer.writerow(collect_row(client, args.word_order, args.firmware))
                    data.flush()
                    rows += 1
                except Exception as exc:
                    errors += 1
                    error_file.write(json.dumps({"utc": utc(), "type": type(exc).__name__,
                                                 "error": str(exc)}) + "\n")
                    error_file.flush()
                    if isinstance(exc, IdentityChanged):
                        stopped_reason = "IDENTITY_CHANGED"
                        break
                time.sleep(.05)
    summary = dict(
        rows=rows, read_errors=errors, ended_utc=utc(), snapshot_atomic=False,
        stopped_reason=stopped_reason, classification="DECIMATED_NON_ATOMIC_HOST_SNAPSHOTS",
        port=args.port, word_order=args.word_order,
        samples_sha256=hashlib.sha256((args.output / "samples.csv").read_bytes()).hexdigest().upper())
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary))

if __name__ == "__main__":
    main()
