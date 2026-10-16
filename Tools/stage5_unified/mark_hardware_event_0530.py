"""Append an operator message marker without claiming an exact physical edge."""
import argparse
import json
import os
import time
from datetime import datetime, timezone
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--events", type=Path, required=True)
    parser.add_argument("--event", required=True)
    parser.add_argument("--message", required=True)
    args = parser.parse_args()
    if not args.events.parent.is_dir():
        parser.error("evidence directory does not exist")
    record = {
        "utc_received": datetime.now(timezone.utc).isoformat(),
        "host_monotonic_ns": time.monotonic_ns(),
        "event": args.event,
        "operator_message": args.message,
        "time_role": "message_received_not_physical_edge",
    }
    with args.events.open("a", encoding="utf-8") as stream:
        stream.write(json.dumps(record, ensure_ascii=False) + "\n")
        stream.flush()
        os.fsync(stream.fileno())
    print(json.dumps(record, ensure_ascii=False))


if __name__ == "__main__":
    main()
