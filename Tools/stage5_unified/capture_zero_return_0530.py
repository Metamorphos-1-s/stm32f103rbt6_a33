"""Read-only 0x0530 recorder with atomic authority and separate diagnostics.

No mailbox commands, configuration writes, SWD, reset, or flash operations.
The frozen on-site configuration uses high-word-first encoding. Other word
orders are refused rather than changing the instrument configuration.
"""
import argparse
import json
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

from analyze_zero_return_0530 import REQUESTS, decode_batch, digest, integer

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools/stage5pa13c"))
from hardware_shadow import ModbusClient, SerialTransport, probe  # noqa: E402


def utc():
    return datetime.now(timezone.utc).isoformat()


def decode_authority(words):
    if len(words) != 40 or integer(words, 0, 2, False) != 0xA13E0001:
        raise ValueError("atomic authority signature/count mismatch")
    result = dict(
        generation=integer(words, 2, 2, False), application=integer(words, 8, 2, False),
        sequence=integer(words, 10, 2, False), uptime_ms=integer(words, 12, 2, False),
        applied_offset_ug=integer(words, 14, 2), apply=integer(words, 16, 2, False),
        uncompensated_ug=integer(words, 18, 4), gross_ug=integer(words, 22, 4),
        net_ug=integer(words, 26, 4), tare_ug=integer(words, 30, 4),
        mode=integer(words, 34, 2, False), state=integer(words, 36, 2, False),
        candidate_offset_ug=integer(words, 38, 2), snapshot_atomic=True)
    applied = result["applied_offset_ug"] if result["apply"] else 0
    if (result["gross_ug"] != result["uncompensated_ug"] - applied or
            result["net_ug"] != result["gross_ug"] - result["tare_ug"]):
        raise ValueError("atomic authority weight equation mismatch")
    return result


def require_preflight(identity):
    cfg, r5 = identity["configuration"], identity["r5"]
    if ((identity["firmware"], identity["map"], identity["word_order"]) !=
            ("0x0530", "0x010F", "high") or identity["dirty"] or
            identity["fault_mask"] or identity["overrun_count"] or
            not identity["calibration_valid"] or identity["revision"] != identity["saved_revision"] or
            (cfg["active_profile"], cfg["sample_rate"], cfg["filter_mode"],
             cfg["filter_strength"], cfg["gain"]) != (0, 1, 1, 3, 3) or
            (r5["application"], r5["mode"], r5["limited"]) != (1, 2, 0) or
            identity["checkweigh"]["mode"] != 1):
        raise ValueError("preflight outside the frozen 0x0530 high-word-first envelope")


def collect(client):
    def read(address, count):
        before_ns = time.monotonic_ns()
        words, _ = client.read(address, count)
        return dict(address=address, count=count, words=words,
                    tx_ns=before_ns, rx_ns=time.monotonic_ns())
    atomic = read(0x340, 40)
    authority = decode_authority(atomic["words"])
    blocks = [read(address, count) for address, count in REQUESTS]
    observation = decode_batch(blocks)
    candidate = read(0x300, 37)
    words = candidate["words"]
    diag = blocks[1]["words"]
    if (observation["firmware"] != 0x0530 or observation["fault"] or observation["overrun"] or
            observation["r5_application"] != 1 or observation["r5_mode"] != 2 or
            observation["r5_limited"] or observation["tare_ug"] or
            observation["r5_equation_error_ug"] or diag[18] or
            integer(diag, 19, 2, False) != integer(diag, 21, 2, False) or not diag[27] or
            (authority["application"], authority["mode"], authority["apply"], authority["tare_ug"]) !=
            (1, 2, 1, 0) or (words[0], words[1], words[4]) != (0xA13C, 2, 0)):
        raise ValueError("identity, state, calibration, tare or safety changed during recording")
    return dict(utc=utc(), host_monotonic_ns=time.monotonic_ns(),
        authority=authority, authority_request_ns=[atomic["tx_ns"], atomic["rx_ns"]],
        observation=observation, observation_snapshot_atomic=False,
        revision=integer(diag, 19, 2, False), saved_revision=integer(diag, 21, 2, False),
        candidate=dict(mode=words[1], state=words[2], reason=words[3], limited=words[4],
            sequence=integer(words, 17, 2, False), uptime_ms=integer(words, 19, 2, False),
            gate_count=integer(words, 21, 2, False), rebuild_count=integer(words, 23, 2, False),
            boost_samples=integer(words, 25, 2, False),
            obvious_step_sequence=integer(words, 27, 2, False),
            robust_step_sequence=integer(words, 29, 2, False),
            quiet_sequence=integer(words, 31, 2, False),
            reference_lock_sequence=integer(words, 33, 2, False),
            first_correction_sequence=integer(words, 35, 2, False)),
        candidate_request_ns=[candidate["tx_ns"], candidate["rx_ns"]])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--duration", type=float, default=1800)
    parser.add_argument("--stop-file", type=Path)
    args = parser.parse_args()
    if args.duration <= 0 or args.output.exists():
        parser.error("duration must be positive and output must be new")
    args.output.mkdir(parents=True)
    rows, errors, reason = 0, 0, "DURATION_OR_STOP_FILE"
    pre = post = None
    with (args.output / "frames.txt").open("w", encoding="ascii") as frames, \
            (args.output / "samples.jsonl").open("w", encoding="utf-8") as samples, \
            (args.output / "errors.jsonl").open("w", encoding="utf-8") as error_file:
        def log(line):
            frames.write(f"{utc()} {time.monotonic_ns()} {line}\n")
            frames.flush()
        try:
            with SerialTransport(args.port, 115200, "N", 1, 350, frame_logger=log) as transport:
                client = ModbusClient(transport, 1)
                pre = probe(client)
                require_preflight(pre)
                (args.output / "preflight.json").write_text(json.dumps(pre, indent=2) + "\n")
                end = time.monotonic() + args.duration
                while time.monotonic() < end and not (args.stop_file and args.stop_file.exists()):
                    row = collect(client)
                    samples.write(json.dumps(row) + "\n")
                    samples.flush()
                    rows += 1
                    time.sleep(.05)
                post = probe(client)
                require_preflight(post)
                (args.output / "postflight.json").write_text(json.dumps(post, indent=2) + "\n")
                if (pre["configuration"] != post["configuration"] or
                        (pre["revision"], pre["saved_revision"], pre["r5"]["save_request_count_low"]) !=
                        (post["revision"], post["saved_revision"], post["r5"]["save_request_count_low"])):
                    raise ValueError("configuration/revision/SAVE count changed")
        except Exception as exc:
            errors += 1
            reason = "STOPPED_ERROR"
            error_file.write(json.dumps(dict(utc=utc(), error=str(exc))) + "\n")
    summary = dict(rows=rows, read_errors=errors, stopped_reason=reason, port=args.port,
        classification="READONLY_CAPTURE_NO_AUTOMATIC_METROLOGY_PASS",
        authority_snapshot_atomic=True, cross_request_snapshot_atomic=False,
        samples_sha256=digest(args.output / "samples.jsonl"),
        frames_sha256=digest(args.output / "frames.txt"))
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary))
    return 2 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
