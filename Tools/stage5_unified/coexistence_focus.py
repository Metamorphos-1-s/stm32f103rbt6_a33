"""Short, no-SAVE hardware checkpoints for 0x052A coexistence."""
import argparse
import json
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools/stage5b_hw"))
sys.path.insert(0, str(ROOT / "Tools/stage5pa13c"))
from hardware_shadow import SerialTransport, ModbusClient, probe, word64, word32  # noqa: E402
from hw_common import execute_command  # noqa: E402
import register_map as reg  # noqa: E402


def utc():
    return datetime.now(timezone.utc).isoformat()


def u32(words):
    return (words[0] << 16) | words[1]


def words64(value):
    value &= (1 << 64) - 1
    return [(value >> shift) & 0xffff for shift in (48, 32, 16, 0)]


def snapshot(client):
    identity = probe(client)
    if (identity["firmware"], identity["map"]) != ("0x052A", "0x010F"):
        raise RuntimeError("unexpected firmware identity")
    a13, _ = client.read(0x340, 40)
    alarm, _ = client.read(0x220, 27)
    check, _ = client.read(0x2c0, 8)
    candidate, _ = client.read(0x300, 37)
    if candidate[0] != 0xA13C or check[0] != 0x5BB5:
        raise RuntimeError("diagnostic signature mismatch")
    return {"identity": identity, "a13_generation": u32(a13[2:4]),
            "a13_actual_offset_ug": word32(a13[14:16], "high"),
            "a13_apply": u32(a13[16:18]),
            "a13_mode": u32(a13[34:36]), "a13_state": u32(a13[36:38]),
            "alarm": alarm, "checkweigh": check,
            "check_generation": u32(check[2:4]),
            "candidate_offset_ug": word64(candidate[5:9], "high")}


def issue(client, token, command, **kwargs):
    result = execute_command(client, token, command, **kwargs)
    if (result["token"] != token or result["last_command"] != command or
            result["result"] != 0):
        raise RuntimeError(f"command {command} failed: {result}")
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("phase", choices=("inspect", "configure", "dosing",
                                           "dynamic", "static", "off"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("evidence path already exists")
    args.output.mkdir(parents=True)
    events = []
    with (args.output / "frames.txt").open("w", encoding="ascii") as frames:
        def log_frame(line):
            frames.write(f"{utc()} {time.monotonic_ns()} {line}\n")
            frames.flush()
        try:
            with SerialTransport("COM5", 115200, "N", 1, 350,
                                 frame_logger=log_frame) as port:
                client = ModbusClient(port, 1)
                before = snapshot(client)
                events.append({"utc": utc(), "kind": "BEFORE", "value": before})
                if before["identity"]["fault_mask"] or before["identity"]["overrun_count"]:
                    raise RuntimeError("device safety counter nonzero")
                last_response, _ = client.read(reg.RESPONSE_TOKEN, 4)
                token = (last_response[0] + 1) & 0xffff
                if token == 0:
                    token = 1
                if args.phase == "configure":
                    if before["checkweigh"][1] != 0 or before["alarm"][0] != 0:
                        raise RuntimeError("expected formal checkweigh and alarm OFF")
                    events.append({"utc": utc(), "kind": "CONFIG_BEGIN",
                                   "result": issue(client, token, 9)})
                    client.write_single(0x0240, 1)
                    client.write_single(0x0241, 0)
                    client.write_multiple(0x0242, words64(-1000000))
                    client.write_multiple(0x0246, words64(1000000))
                    client.write_multiple(0x024A, words64(10000))
                    client.write_single(0x024E, 0)
                    client.write_single(0x024F, 0)
                    client.write_single(0x0250, 0)
                    events.append({"utc": utc(), "kind": "CONFIG_VALIDATE",
                                   "result": issue(client, token + 1, 10)})
                    events.append({"utc": utc(), "kind": "CONFIG_APPLY_RAM",
                                   "result": issue(client, token + 2, 11)})
                    updated = snapshot(client)
                    events.append({"utc": utc(), "kind": "AFTER_CONFIG", "value": updated})
                    if updated["alarm"][0] != 1 or updated["alarm"][1] != 0 or \
                            word64(updated["alarm"][2:6], "high") != -1000000 or \
                            word64(updated["alarm"][6:10], "high") != 1000000:
                        raise RuntimeError("RAM alarm readback mismatch")
                    generation = updated["check_generation"]
                    events.append({"utc": utc(), "kind": "CHECKWEIGH_STATIC",
                                   "result": issue(client, token + 3, 34, arg0=1,
                                                   arg1=generation, flags=1)})
                elif args.phase in ("dosing", "static"):
                    target = 1 if args.phase == "dosing" else 2
                    events.append({"utc": utc(), "kind": "R5_PAIR",
                                   "result": issue(client, token, 36, arg0=1,
                                                   arg1=target,
                                                   arg64=before["a13_generation"],
                                                   flags=1)})
                elif args.phase in ("dynamic", "off"):
                    target = 2 if args.phase == "dynamic" else 0
                    events.append({"utc": utc(), "kind": "CHECKWEIGH_MODE",
                                   "result": issue(client, token, 34, arg0=target,
                                                   arg1=before["check_generation"],
                                                   flags=1)})
                time.sleep(0.5)
                after = snapshot(client)
                events.append({"utc": utc(), "kind": "AFTER", "value": after})
                expected_check = {"configure": 1, "dynamic": 2, "off": 0}.get(args.phase)
                if expected_check is not None and after["checkweigh"][1] != expected_check:
                    raise RuntimeError("checkweigh mode readback mismatch")
                expected_r5 = {"dosing": 1, "static": 2}.get(args.phase)
                if expected_r5 is not None and after["identity"]["r5"]["mode"] != expected_r5:
                    raise RuntimeError("R5 mode readback mismatch")
                if args.phase == "dosing" and (
                        after["identity"]["r5"]["offset_ug"] !=
                        before["identity"]["r5"]["offset_ug"] or
                        after["checkweigh"][4] != 3 or
                        after["checkweigh"][5] != 0):
                    raise RuntimeError("STATIC DOSING freeze/pending mismatch")
                if after["identity"]["fault_mask"] or after["identity"]["overrun_count"]:
                    raise RuntimeError("new device safety fault")
                if args.phase != "inspect" and after["identity"]["saved_revision"] != 22:
                    raise RuntimeError("unexpected SAVE")
                print(json.dumps({"phase": args.phase,
                    "before": {"r5": before["identity"]["r5"],
                               "check": before["checkweigh"][1]},
                    "after": {"r5": after["identity"]["r5"],
                              "check": after["checkweigh"][1],
                              "check_reason": after["checkweigh"][4],
                              "formal": after["checkweigh"][5],
                              "alarm_state": after["alarm"][17],
                              "revision": after["identity"]["revision"],
                              "saved": after["identity"]["saved_revision"]}}))
        except Exception as exc:
            events.append({"utc": utc(), "kind": "ERROR", "error": str(exc)})
            raise
        finally:
            (args.output / "events.json").write_text(
                json.dumps(events, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
