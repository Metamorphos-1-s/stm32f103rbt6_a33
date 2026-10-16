"""Read-only 100-FC03 COM link gate with raw RTU evidence."""
import argparse
import json
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools/stage5pa13c"))
from hardware_shadow import ModbusClient, SerialTransport  # noqa: E402


def utc():
    return datetime.now(timezone.utc).isoformat()


def classify(exc):
    value = str(exc).lower()
    if "timeout" in value or "no response" in value:
        return "MODBUS_TIMEOUT"
    if "crc" in value:
        return "CRC_ERROR"
    if "exception" in value:
        return "MODBUS_EXCEPTION"
    return "PROTOCOL_OR_IO_ERROR"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--attempts", type=int, default=100)
    args = parser.parse_args()
    if args.output.exists() or args.attempts <= 0:
        parser.error("output must be new and attempts positive")
    args.output.mkdir(parents=True)
    counts = {"VALID": 0, "PORT_OPEN_FAILURE": 0, "MODBUS_TIMEOUT": 0,
              "CRC_ERROR": 0, "MODBUS_EXCEPTION": 0,
              "PROTOCOL_OR_IO_ERROR": 0, "IDENTITY_MISMATCH": 0}
    with (args.output / "frames.txt").open("w", encoding="ascii") as frames, \
         (args.output / "events.jsonl").open("w", encoding="utf-8") as events:
        def log_frame(line):
            frames.write("%s %d %s\n" % (utc(), time.monotonic_ns(), line))
            frames.flush()
        try:
            port = SerialTransport(args.port, 115200, "N", 1, 350,
                                   frame_logger=log_frame)
        except Exception as exc:
            counts["PORT_OPEN_FAILURE"] = 1
            events.write(json.dumps({"utc": utc(), "kind": "PORT_OPEN_FAILURE",
                                     "error": str(exc)}) + "\n")
        else:
            with port:
                client = ModbusClient(port, 1)
                for index in range(args.attempts):
                    try:
                        words, exchange = client.read(0, 16)
                        kind = "VALID" if words[14] == 0x010F and \
                            words[15] in (0x052E, 0x0530) else "IDENTITY_MISMATCH"
                        event = {"utc": utc(), "index": index, "kind": kind,
                                 "map": words[14], "firmware": words[15],
                                 "tx_hex": exchange.tx.hex().upper(),
                                 "rx_hex": exchange.rx.hex().upper()}
                    except Exception as exc:
                        kind = classify(exc)
                        event = {"utc": utc(), "index": index, "kind": kind,
                                 "error": str(exc)}
                    counts[kind] += 1
                    events.write(json.dumps(event) + "\n")
                    events.flush()
    summary = {"utc": utc(), "port": args.port, "requested_attempts": args.attempts,
               "counts": counts, "pass": counts["VALID"] == args.attempts}
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n",
                                               encoding="utf-8")
    print(json.dumps(summary))
    return 0 if summary["pass"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
