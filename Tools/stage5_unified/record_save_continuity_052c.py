"""Atomic 0x052C weight/state polling and one file-triggered SAVE on COM5."""
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
from hardware_shadow import SerialTransport, ModbusClient, probe, word32, word64  # noqa: E402
from hw_common import execute_command  # noqa: E402
import register_map as reg  # noqa: E402


def now():
    return datetime.now(timezone.utc).isoformat()


def u32(words):
    return (words[0] << 16) | words[1]


def sample(client):
    active, _ = client.read(0x340, 40)
    diag, _ = client.read(0x20, 28)
    check, _ = client.read(0x2c0, 8)
    r5, _ = client.read(0x280, 10)
    storage, _ = client.read(0x1c0, 4)
    if active[0:2] != [0xA13E, 1] or check[0] != 0x5BB5:
        raise RuntimeError("diagnostic signature changed")
    return dict(utc=now(), host_monotonic_ns=time.monotonic_ns(),
        sequence=u32(active[10:12]), uptime_ms=u32(active[12:14]),
        uncompensated_gross_ug=word64(active[18:22], "high"),
        formal_gross_ug=word64(active[22:26], "high"),
        formal_net_ug=word64(active[26:30], "high"),
        tare_ug=word64(active[30:34], "high"),
        applied_offset_ug=word32(active[14:16], "high"),
        offset_applied=u32(active[16:18]),
        r5_application=u32(active[8:10]),
        r5_mode=u32(active[34:36]),
        r5_state=u32(active[36:38]),
        r5_exit_reason=u32(active[6:8]),
        r5_generation=u32(active[2:4]),
        r5_offset_ug=word64(r5[6:10], "high"),
        check_mode=check[1], check_reason=check[4],
        check_formal=check[5], check_flags=check[6],
        revision=u32(diag[19:21]), saved_revision=u32(diag[21:23]),
        dirty=diag[18], fault=u32(diag[25:27]),
        overrun=u32(diag[13:15]),
        storage_slot=storage[1], storage_sequence=u32(storage[2:4]),
        save_request_count_low=r5[9] if False else None)


def save_preflight(client, prior):
    identity = probe(client)
    alarm, _ = client.read(0x220, 27)
    check, _ = client.read(0x2c0, 8)
    battery, _ = client.read(0x3a0, 9)
    config, _ = client.read(0x100, 64)
    r5, _ = client.read(0x280, 40)
    expected_battery = [0, 400, 0, 13600, 0, 13200, 0, 13900, 1]
    conditions = dict(
        identity=(identity["firmware"], identity["map"]) == ("0x052C", "0x010F"),
        profile=(identity["configuration"]["sample_rate"],
                 identity["configuration"]["filter_mode"],
                 identity["configuration"]["filter_strength"]) == (1, 1, 3),
        calibrated=identity["calibration_valid"] == 1,
        safe=not identity["fault_mask"] and not identity["overrun_count"],
        r5=(r5[1], r5[2], r5[3], r5[4]) == (1, 2, 5, 0) and
           prior["r5_application"] == 1 and
           prior["r5_mode"] == 2 and prior["offset_applied"] == 1,
        check=check[1] == 1 and prior["check_mode"] == 1,
        alarm=alarm[0] == 1 and alarm[1] == 0 and
              word64(alarm[2:6], "high") == 100000000 and
              word64(alarm[6:10], "high") == 400000000 and
              word64(alarm[10:14], "high") == 10000,
        battery=battery == expected_battery,
        unit=config[1] == 1,
        revision=prior["revision"] == identity["revision"] and
                 prior["saved_revision"] == identity["saved_revision"] and
                 prior["revision"] > prior["saved_revision"] and prior["dirty"] == 1,
        storage=identity["configuration"]["storage_sequence"] == prior["storage_sequence"],
        no_prior_save=r5[39] == 0,
        measurable_offset=abs(prior["applied_offset_ug"]) >= 100,
    )
    return {"identity": identity, "conditions": conditions,
            "alarm": alarm, "check": check, "battery": battery,
            "config": config, "r5": r5}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--duration", type=int, default=420)
    parser.add_argument("--trigger", type=Path, required=True)
    parser.add_argument("--stop-file", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("output already exists")
    args.output.mkdir(parents=True)
    fields = ("utc", "host_monotonic_ns", "sequence", "uptime_ms",
        "uncompensated_gross_ug", "formal_gross_ug", "formal_net_ug", "tare_ug",
        "applied_offset_ug", "offset_applied", "r5_application", "r5_mode",
        "r5_state", "r5_exit_reason", "r5_generation", "r5_offset_ug",
        "check_mode", "check_reason", "check_formal", "check_flags",
        "revision", "saved_revision", "dirty", "fault", "overrun",
        "storage_slot", "storage_sequence")
    events = []
    rows = errors = 0
    save_sent = False
    save_completed = False
    pre_save_offset = None
    result_label = "INCOMPLETE"
    with (args.output / "frames.txt").open("w", encoding="ascii") as frames, \
         (args.output / "samples.csv").open("w", encoding="utf-8", newline="") as data, \
         (args.output / "errors.jsonl").open("w", encoding="utf-8") as failures:
        def raw(line):
            frames.write(f"{now()} {time.monotonic_ns()} {line}\n")
            frames.flush()
        writer = csv.DictWriter(data, fieldnames=fields)
        writer.writeheader()
        data.flush()
        try:
            with SerialTransport("COM5", 115200, "N", 1, 350,
                                 frame_logger=raw) as port:
                client = ModbusClient(port, 1)
                start = probe(client)
                events.append(dict(kind="START", utc=now(), value=start))
                if (start["firmware"], start["map"]) != ("0x052C", "0x010F"):
                    raise RuntimeError("unexpected firmware identity")
                next_poll = time.monotonic()
                deadline = next_poll + args.duration
                post_save_deadline = None
                previous = None
                while time.monotonic() < deadline and not args.stop_file.exists():
                    try:
                        current = sample(client)
                        writer.writerow({key: current[key] for key in fields})
                        data.flush()
                        rows += 1
                        if previous is not None and current["uptime_ms"] < previous["uptime_ms"]:
                            raise RuntimeError("MCU reset during continuous record")
                        if current["fault"] or current["overrun"]:
                            raise RuntimeError("device safety counter became nonzero")
                        if args.trigger.exists() and not save_sent:
                            guard = save_preflight(client, current)
                            events.append(dict(kind="SAVE_PRECONDITION", utc=now(), value=guard))
                            if not all(guard["conditions"].values()):
                                raise RuntimeError("SAVE preconditions failed; no SAVE sent")
                            response, _ = client.read(reg.RESPONSE_TOKEN, 4)
                            token = (response[0] + 1) & 0xffff or 1
                            command = execute_command(client, token, 13)
                            save_sent = True
                            pre_save_offset = current["applied_offset_ug"]
                            events.append(dict(kind="SAVE_COMMAND", utc=now(), response=command,
                                preceding=current))
                            if command["token"] != token or command["last_command"] != 13 or \
                                    command["result"] != 1:
                                raise RuntimeError("SAVE command was not ACCEPTED")
                            post_save_deadline = time.monotonic() + 180
                        if save_sent:
                            if current["revision"] == current["saved_revision"] and not current["dirty"]:
                                if current["r5_application"] != 1 or current["r5_mode"] != 2 or \
                                        current["check_mode"] != 1:
                                    raise RuntimeError("post-SAVE runtime/request mismatch")
                                if not save_completed:
                                    if abs(current["applied_offset_ug"] - pre_save_offset) > 5000:
                                        raise RuntimeError("post-SAVE offset discontinuity")
                                    events.append(dict(kind="SAVE_COMPLETED", utc=now(),
                                        value=current))
                                    save_completed = True
                            if post_save_deadline and time.monotonic() >= post_save_deadline:
                                result_label = "CAPTURED_180S" if save_completed else "FAIL"
                                break
                        if current["formal_gross_ug"] != current["uncompensated_gross_ug"] - \
                                (current["applied_offset_ug"] if current["offset_applied"] else 0) or \
                                current["formal_net_ug"] != current["formal_gross_ug"] - current["tare_ug"]:
                            raise RuntimeError("authoritative weight equation mismatch")
                        previous = current
                    except Exception as exc:
                        errors += 1
                        failures.write(json.dumps(dict(utc=now(),
                            host_monotonic_ns=time.monotonic_ns(),
                            error=str(exc))) + "\n")
                        failures.flush()
                        if ("device safety" in str(exc) or "runtime/request" in str(exc) or
                                "MCU reset" in str(exc) or "SAVE preconditions" in str(exc) or
                                "SAVE command" in str(exc) or "discontinuity" in str(exc) or
                                "weight equation" in str(exc)):
                            result_label = "FAIL"
                            break
                    next_poll += 0.1
                    time.sleep(max(0, next_poll - time.monotonic()))
                events.append(dict(kind="END", utc=now(), reason=result_label))
        except Exception as exc:
            events.append(dict(kind="ERROR", utc=now(), error=str(exc)))
            result_label = "FAIL"
        finally:
            (args.output / "events.json").write_text(
                json.dumps(events, indent=2) + "\n", encoding="utf-8")
    summary = dict(result=result_label, rows=rows, read_errors=errors,
        save_sent=save_sent, save_completed=save_completed, ended_utc=now(),
        samples_sha256=hashlib.sha256(
            (args.output / "samples.csv").read_bytes()).hexdigest().upper())
    (args.output / "summary.json").write_text(
        json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary), flush=True)
    return 1 if result_label == "FAIL" else 0


if __name__ == "__main__":
    raise SystemExit(main())
