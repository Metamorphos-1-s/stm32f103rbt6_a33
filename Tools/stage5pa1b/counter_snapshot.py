#!/usr/bin/env python3
import argparse
import hashlib
import json
import struct
import subprocess
from pathlib import Path

NAMES = ("s_consumed_count", "s_buffer_overrun_count",
         "s_read_error_count", "s_sample_count", "s_count")


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def symbols(elf, nm):
    output = subprocess.check_output([nm, "-a", "-S", str(elf)], text=True)
    result = {}
    for line in output.splitlines():
        parts = line.split()
        if (len(parts) >= 4 and parts[3] in NAMES and
            (parts[3] != "s_count" or int(parts[1], 16) == 2)):
            result[parts[3]] = {"address": int(parts[0], 16),
                                "size": int(parts[1], 16)}
    if set(result) != set(NAMES) or any(item["size"] !=
        (2 if name == "s_count" else 4) for name, item in result.items()):
        raise ValueError("required counter symbols missing or changed")
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--sn", required=True)
    parser.add_argument("--programmer", required=True)
    parser.add_argument("--nm", default="arm-none-eabi-nm")
    parser.add_argument("--swd-khz", type=int, default=1800)
    args = parser.parse_args()
    found = symbols(args.elf, args.nm)
    first = min(item["address"] for item in found.values())
    end = max(item["address"] + item["size"] for item in found.values())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    raw = args.output.with_suffix(".bin")
    command = [args.programmer, "-c", "port=SWD", "mode=HotPlug",
        "freq=%d" % args.swd_khz, "sn=%s" % args.sn,
        "-halt", "-u", "0x%08X" % first, str(end - first),
        str(raw.resolve()), "-run"]
    completed = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT)
    if completed.returncode:
        raise RuntimeError(completed.stdout)
    data = raw.read_bytes()
    counters = {name: struct.unpack_from("<H" if item["size"] == 2 else "<I",
        data, item["address"] - first)[0] for name, item in found.items()}
    value = {"elf": {"path": str(args.elf), "sha256": sha256(args.elf)},
        "tool_sha256": sha256(__file__), "symbols": found,
        "read_address": "0x%08X" % first, "read_length": end - first,
        "counters": counters, "continuous_swd_polling": False,
        "core_halted_only_for_boundary_snapshot": True,
        "programmer_output": completed.stdout}
    args.output.write_text(json.dumps(value, indent=2) + "\n",
                           encoding="utf-8")
    print(json.dumps(counters))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
