#!/usr/bin/env python3
"""Read-only A13C Modbus identity and focused SHADOW evidence recorder.

The only write operation here is writing local evidence files. Never issues a
Modbus write, ZERO, TARE, SAVE, calibration command, or flash operation.
"""

import argparse
import csv
import hashlib
import json
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "stage5b_hw"))
from hw_common import ModbusClient  # noqa: E402
from serial_transport import SerialTransport  # noqa: E402
import modbus_frame as frame  # noqa: E402


def now():
    return datetime.now(timezone.utc).isoformat()


def word32(words, order):
    return frame.decode_i32_words(words, order)


def unsigned32(words, order):
    return word32(words, order) & 0xffffffff


def word64(words, order):
    return frame.decode_i64_words(words, order)


def probe(client):
    realtime, _ = client.read(0, 32)
    diagnostic, _ = client.read(0x20, 28)
    configuration, _ = client.read(0x100, 64)
    calibration, _ = client.read(0x190, 12)
    storage, _ = client.read(0x1c0, 10)
    r5, _ = client.read(0x280, 40)
    checkweigh, _ = client.read(0x2c0, 8)
    order = "low" if configuration[3] else "high"
    active = configuration[0x1f]
    base = 0x20 if active == 0 else 0x2e
    candidate = None
    if realtime[14] == 0x0105:
        c, _ = client.read(0x300, 37)
        if c[0] != 0xa13c:
            raise ValueError("invalid A13C diagnostic signature")
        candidate = {
            "signature": "0xA13C", "mode": c[1], "state": c[2],
            "reason": c[3], "limited": c[4],
            "offset_ug": word64(c[5:9], order),
            "corrected_ug": word64(c[9:13], order),
            "uncompensated_ug": word64(c[13:17], order),
            "sample_sequence": unsigned32(c[17:19], order),
            "mcu_uptime_ms": unsigned32(c[19:21], order),
            "gate_count": unsigned32(c[21:23], order),
            "rebuild_count": unsigned32(c[23:25], order),
            "boost_samples": unsigned32(c[25:27], order),
            "obvious_sequence": unsigned32(c[27:29], order),
            "robust_sequence": unsigned32(c[29:31], order),
            "quiet_sequence": unsigned32(c[31:33], order),
            "reference_lock_sequence": unsigned32(c[33:35], order),
            "first_correction_sequence": unsigned32(c[35:37], order),
        }
    return {
        "utc": now(), "host_monotonic_ns": time.monotonic_ns(),
        "firmware": "0x%04X" % realtime[15],
        "map": "0x%04X" % realtime[14], "word_order": order,
        "display_count": word32(realtime[0:2], order),
        "display_decimals": realtime[2],
        "gross_ug": word64(realtime[20:24], order),
        "net_ug": word64(realtime[16:20], order),
        "raw_adc": word32(realtime[28:30], order),
        "filtered_adc_counts": word32(realtime[30:32], order),
        "sample_sequence": unsigned32(diagnostic[0:2], order),
        "mcu_uptime_ms": unsigned32(diagnostic[2:4], order),
        "overrun_count": unsigned32(diagnostic[13:15], order),
        "fault_mask": unsigned32(diagnostic[25:27], order),
        "dirty": diagnostic[18],
        "revision": unsigned32(diagnostic[19:21], order),
        "saved_revision": unsigned32(diagnostic[21:23], order),
        "calibration_valid": diagnostic[27],
        "configuration": {
            "active_profile": active, "sample_rate": configuration[base],
            "gain": configuration[base + 1],
            "filter_mode": configuration[base + 2],
            "filter_strength": configuration[base + 3],
            "raw_zero": word32(calibration[0:2], order),
            "raw_span": word32(calibration[2:4], order),
            "span_mass_ug": word64(calibration[4:8], order),
            "schema": configuration[0x3e],
            "persistent_format": storage[0], "slot": storage[1],
            "storage_sequence": unsigned32(storage[2:4], order),
        },
        "r5": {"signature": r5[0], "application": r5[1],
               "mode": r5[2], "state": r5[3], "limited": r5[4],
               "offset_ug": word64(r5[6:10], order),
               "save_request_count_low": r5[39]},
        "checkweigh": {"signature": checkweigh[0],
                        "mode": checkweigh[1]},
        "candidate": candidate,
    }


def capture(client, target, duration, interval):
    columns = ("utc", "host_monotonic_ns", "firmware", "map", "gross_ug",
               "raw_adc", "filtered_adc_counts", "display_count",
               "sample_sequence", "mcu_uptime_ms", "fault_mask",
               "overrun_count", "dirty", "revision", "saved_revision",
               "r5_mode", "r5_application", "candidate_state",
               "candidate_reason", "candidate_corrected_ug",
               "candidate_offset_ug", "candidate_gate_count",
               "candidate_rebuild_count", "candidate_boost_samples",
               "candidate_obvious_sequence", "candidate_robust_sequence",
               "candidate_quiet_sequence", "candidate_reference_lock_sequence",
               "candidate_first_correction_sequence", "status_flags",
               "checkweigh_mode", "candidate_sample_sequence",
               "candidate_mcu_uptime_ms", "candidate_uncompensated_ug",
               "sample_pair_matched")
    attempts = 0
    errors = []
    end = time.monotonic() + duration
    next_poll = time.monotonic()
    initial = probe(client)
    order = initial["word_order"]
    expected_identity = (initial["firmware"], initial["map"])
    with target.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=columns)
        writer.writeheader()
        stream.flush()
        while time.monotonic() < end:
            attempts += 1
            try:
                raw, _ = client.read(0, 64)
                diag = raw[32:64]
                candidate, _ = client.read(0x300, 37)
                if candidate[0] != 0xa13c:
                    raise ValueError("lost A13C diagnostic signature")
                identity = ("0x%04X" % raw[15], "0x%04X" % raw[14])
                if identity != expected_identity:
                    raise ValueError("firmware/Map identity changed")
                entry = {
                    "utc": now(), "host_monotonic_ns": time.monotonic_ns(),
                    "firmware": identity[0], "map": identity[1],
                    "gross_ug": word64(raw[20:24], order),
                    "raw_adc": word32(raw[28:30], order),
                    "filtered_adc_counts": word32(raw[30:32], order),
                    "display_count": word32(raw[0:2], order),
                    "status_flags": unsigned32(raw[4:6], order),
                    "sample_sequence": unsigned32(diag[0:2], order),
                    "mcu_uptime_ms": unsigned32(diag[2:4], order),
                    "overrun_count": unsigned32(diag[13:15], order),
                    "fault_mask": unsigned32(diag[25:27], order),
                    "dirty": diag[18],
                    "revision": unsigned32(diag[19:21], order),
                    "saved_revision": unsigned32(diag[21:23], order),
                    # Not read in the fast per-sample pair. Preflight and
                    # postflight contain the actual application register.
                    "r5_application": None,
                    "r5_mode": candidate[1],
                    "candidate_state": candidate[2],
                    "candidate_reason": candidate[3],
                    "candidate_offset_ug": word64(candidate[5:9], order),
                    "candidate_corrected_ug": word64(candidate[9:13], order),
                    "candidate_uncompensated_ug": word64(candidate[13:17], order),
                    "candidate_sample_sequence": unsigned32(candidate[17:19], order),
                    "candidate_mcu_uptime_ms": unsigned32(candidate[19:21], order),
                    "candidate_gate_count": unsigned32(candidate[21:23], order),
                    "candidate_rebuild_count": unsigned32(candidate[23:25], order),
                    "candidate_boost_samples": unsigned32(candidate[25:27], order),
                    "candidate_obvious_sequence": unsigned32(candidate[27:29], order),
                    "candidate_robust_sequence": unsigned32(candidate[29:31], order),
                    "candidate_quiet_sequence": unsigned32(candidate[31:33], order),
                    "candidate_reference_lock_sequence":
                        unsigned32(candidate[33:35], order),
                    "candidate_first_correction_sequence":
                        unsigned32(candidate[35:37], order),
                }
                entry["sample_pair_matched"] = int(
                    entry["sample_sequence"] == entry["candidate_sample_sequence"])
                if attempts % 10 == 1:
                    checkweigh, _ = client.read(0x2c0, 2)
                    entry["checkweigh_mode"] = checkweigh[1]
                writer.writerow(entry)
                stream.flush()
            except Exception as exc:  # keep the failure, do not invent samples
                errors.append({"utc": now(), "host_monotonic_ns":
                               time.monotonic_ns(), "error": str(exc)})
            next_poll += interval
            time.sleep(max(0.0, next_poll - time.monotonic()))
    return {"attempts": attempts, "errors": errors,
            "sha256": hashlib.sha256(target.read_bytes()).hexdigest().upper()}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM5")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--duration", type=float, default=0.0)
    parser.add_argument("--interval", type=float, default=0.1)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    with SerialTransport(args.port, 115200, "N", 1, 350) as transport:
        client = ModbusClient(transport, 1)
        start = probe(client)
        (args.output / "preflight.json").write_text(
            json.dumps(start, indent=2) + "\n", encoding="utf-8")
        print(json.dumps({"firmware": start["firmware"], "map": start["map"],
                          "configuration": start["configuration"],
                          "r5": start["r5"], "candidate": start["candidate"],
                          "fault": start["fault_mask"],
                          "overrun": start["overrun_count"],
                          "dirty": start["dirty"],
                          "revision": start["revision"],
                          "saved_revision": start["saved_revision"]}))
        if args.duration > 0:
            result = capture(client, args.output / "samples.csv",
                             args.duration, args.interval)
            result["last"] = probe(client)
            (args.output / "summary.json").write_text(
                json.dumps(result, indent=2) + "\n", encoding="utf-8")
            print(json.dumps({"attempts": result["attempts"],
                              "errors": len(result["errors"]),
                              "sha256": result["sha256"]}))


if __name__ == "__main__":
    main()
