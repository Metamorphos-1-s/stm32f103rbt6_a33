#!/usr/bin/env python3
import argparse
import hashlib
import json
import struct
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools" / "stage5b_hw"))
from hw_common import ModbusClient
from serial_transport import SerialTransport

MAGIC = 0x31504135
VERSION = 1
COMMAND_MAGIC = 0x31415753
CONTROL_NAMES = ("magic", "version", "request_sequence",
    "applied_sequence", "command_magic", "duration_ms", "state", "length")
CONTROL_SIZE = len(CONTROL_NAMES) * 4
QUANTITIES = (1, 10, 27, 32, 40)
SNAPSHOT_NAMES = (
    "magic", "version", "cpu_clock_hz", "start_ms", "end_ms",
    "app_run_count", "app_run_max_interval_cycles",
    "app_run_max_execution_cycles", "app_run_over_25ms_count",
    "app_run_over_50ms_count", "communication_count",
    "communication_total_cycles", "communication_max_cycles",
    "modbus_read_count", "modbus_read_total_cycles",
    "modbus_read_max_cycles",
    *("quantity_%d" % index for index in range(5)),
    *("quantity_count_%d" % index for index in range(5)),
    *("quantity_total_cycles_%d" % index for index in range(5)),
    *("quantity_max_cycles_%d" % index for index in range(5)),
    "ready_observed_count", "read_success_count", "read_failure_count",
    "fifo_push_count", "fifo_pop_count", "fifo_max_depth",
    "bridge_accept_count", "bridge_reject_count",
    "sample_sequence_increment_count", "cs1237_sample_start",
    "cs1237_sample_end", "bridge_consumed_start", "bridge_consumed_end",
    "engine_sequence_start", "engine_sequence_end", "overrun_start",
    "overrun_end", "read_error_start", "read_error_end", "fault_start",
    "fault_end", "cs1237_config_register", "cs1237_state")
SNAPSHOT_SIZE = len(SNAPSHOT_NAMES) * 4
LOADS = {
    "none": (), "q1": ((0x0103, 1),), "q10": ((0x01C0, 10),),
    "q27": ((0x0020, 27),), "q32": ((0x0000, 32),),
    "q40": ((0x0280, 40),),
    "full": ((0x0000, 32), (0x0020, 27), (0x01C0, 10),
             (0x0103, 1), (0x0280, 40))}


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def write_json(path, value):
    Path(path).write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def symbols(elf, nm):
    output = subprocess.check_output([nm, "-S", "--defined-only", str(elf)],
                                     text=True)
    found = {}
    required = {"g_stage5pa1_diagnostic_control",
                "g_stage5pa1_throughput_snapshot"}
    for line in output.splitlines():
        parts = line.split()
        if len(parts) >= 4 and parts[3] in required:
            found[parts[3]] = {"address": int(parts[0], 16),
                               "size": int(parts[1], 16)}
    if set(found) != required:
        raise ValueError("Stage 5P-A1 SWD symbols missing")
    if found["g_stage5pa1_diagnostic_control"]["size"] != CONTROL_SIZE:
        raise ValueError("control size mismatch")
    if found["g_stage5pa1_throughput_snapshot"]["size"] != SNAPSHOT_SIZE:
        raise ValueError("snapshot size mismatch")
    return found


def programmer(program, serial, khz, arguments):
    command = [program, "-c", "port=SWD", "mode=HotPlug",
               "freq=%d" % khz, "sn=%s" % serial] + arguments
    result = subprocess.run(command, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True)
    if result.returncode:
        raise RuntimeError(result.stdout)
    return {"command": command, "output": result.stdout}


def upload(args, address, size, path):
    return programmer(args.programmer, args.sn, args.swd_khz,
        ["-u", "0x%08X" % address, str(size), str(Path(path).resolve())])


def write32(args, address, value):
    return programmer(args.programmer, args.sn, args.swd_khz,
        ["-w32", "0x%08X" % address, "0x%08X" % value, "-v"])


def decode(path, names, magic=True):
    data = Path(path).read_bytes()
    if len(data) != len(names) * 4:
        raise ValueError("binary size mismatch")
    value = dict(zip(names, struct.unpack("<%dI" % len(names), data)))
    if magic and (value["magic"] != MAGIC or value["version"] != VERSION):
        raise ValueError("diagnostic magic/version mismatch")
    return value


def run_capture(args):
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    syms = symbols(args.elf, args.nm)
    control = syms["g_stage5pa1_diagnostic_control"]
    snapshot = syms["g_stage5pa1_throughput_snapshot"]
    operations = []
    before = output / "control_before.bin"
    operations.append(upload(args, control["address"], CONTROL_SIZE, before))
    initial = decode(before, CONTROL_NAMES)
    sequence = (initial["request_sequence"] + 1) & 0xFFFFFFFF
    operations.append(write32(args, control["address"] + 20,
                              int(args.duration_s * 1000)))
    operations.append(write32(args, control["address"] + 16, COMMAND_MAGIC))
    operations.append(write32(args, control["address"] + 8, sequence))
    frame_path = output / "frames.jsonl"

    def frame_log(line):
        with frame_path.open("a", encoding="utf-8", newline="\n") as stream:
            stream.write(json.dumps({"utc": time.strftime(
                "%Y-%m-%dT%H:%M:%SZ", time.gmtime()), "frame": line}) + "\n")

    reads = errors = 0
    started = time.monotonic()
    if LOADS[args.load]:
        with SerialTransport(args.port, args.baud, "N", 1, 300,
                             frame_logger=frame_log) as transport:
            client = ModbusClient(transport, 1)
            while time.monotonic() - started < args.duration_s:
                for address, quantity in LOADS[args.load]:
                    try:
                        client.read(address, quantity)
                        reads += 1
                    except Exception as exc:
                        errors += 1
                        with (output / "errors.jsonl").open(
                                "a", encoding="utf-8") as stream:
                            stream.write(json.dumps({"error": str(exc)}) + "\n")
    else:
        time.sleep(args.duration_s)
    time.sleep(0.5)
    after = output / "control_after.bin"
    raw = output / "snapshot.bin"
    operations.append(upload(args, control["address"], CONTROL_SIZE, after))
    operations.append(upload(args, snapshot["address"], SNAPSHOT_SIZE, raw))
    final_control = decode(after, CONTROL_NAMES)
    result = decode(raw, SNAPSHOT_NAMES)
    clock = result["cpu_clock_hz"]
    duration = ((result["end_ms"] - result["start_ms"]) & 0xFFFFFFFF) / 1000
    deltas = {name: result[name + "_end"] - result[name + "_start"]
              for name in ("cs1237_sample", "bridge_consumed",
                           "engine_sequence", "overrun", "read_error", "fault")}
    quantity_stats = []
    for index in range(5):
        count = result["quantity_count_%d" % index]
        total = result["quantity_total_cycles_%d" % index]
        quantity_stats.append({"quantity": result["quantity_%d" % index],
            "count": count, "average_cycles": total / count if count else None,
            "maximum_cycles": result["quantity_max_cycles_%d" % index]})
    analysis = {"load": args.load, "duration_s": duration,
        "target_rate_hz": deltas["engine_sequence"] / duration if duration else None,
        "counter_deltas": deltas, "quantity_stats": quantity_stats,
        "app_run": {"count": result["app_run_count"],
            "max_interval_cycles": result["app_run_max_interval_cycles"],
            "max_execution_cycles": result["app_run_max_execution_cycles"],
            "over_25ms": result["app_run_over_25ms_count"],
            "over_50ms": result["app_run_over_50ms_count"]},
        "communication": {"count": result["communication_count"],
            "average_cycles": result["communication_total_cycles"] /
                result["communication_count"] if result["communication_count"] else None,
            "maximum_cycles": result["communication_max_cycles"]},
        "chain": {name: result[name] for name in (
            "ready_observed_count", "read_success_count", "read_failure_count",
            "fifo_push_count", "fifo_pop_count", "fifo_max_depth",
            "bridge_accept_count", "bridge_reject_count",
            "sample_sequence_increment_count")},
        "cs1237_config_register": result["cs1237_config_register"],
        "cs1237_state": result["cs1237_state"], "host_reads": reads,
        "host_errors": errors, "control": final_control}
    identity = {"elf": {"path": str(args.elf), "length": args.elf.stat().st_size,
        "sha256": sha256(args.elf)}, "symbols": syms,
        "tool_sha256": sha256(__file__), "continuous_swd_polling": False}
    write_json(output / "snapshot.json", result)
    write_json(output / "analysis.json", analysis)
    write_json(output / "identity.json", identity)
    write_json(output / "swd_operations.json", operations)
    print(json.dumps(analysis, indent=2))
    return 0 if final_control["state"] == 2 and errors == 0 else 2


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--load", choices=sorted(LOADS), required=True)
    parser.add_argument("--duration-s", type=float, default=30.0)
    parser.add_argument("--port", default="COM5")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--sn", required=True)
    parser.add_argument("--swd-khz", type=int, default=1800)
    parser.add_argument("--programmer", default="STM32_Programmer_CLI.exe")
    parser.add_argument("--nm", default="arm-none-eabi-nm")
    return run_capture(parser.parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
