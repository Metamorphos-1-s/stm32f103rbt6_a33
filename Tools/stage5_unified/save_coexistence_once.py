"""One guarded explicit SAVE of the already tested 0x052B RAM combination."""
import argparse
import json
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools/stage5b_hw"))
sys.path.insert(0, str(ROOT / "Tools/stage5pa13c"))
from hardware_shadow import SerialTransport, ModbusClient, probe, word64  # noqa: E402
from hw_common import execute_command  # noqa: E402
import register_map as reg  # noqa: E402


def utc():
    return datetime.now(timezone.utc).isoformat()


def read(client):
    identity = probe(client)
    alarm, _ = client.read(0x220, 27)
    check, _ = client.read(0x2c0, 8)
    battery, _ = client.read(0x3a0, 9)
    active, _ = client.read(0x100, 64)
    return dict(identity=identity, alarm=alarm, check=check,
                battery=battery, active=active)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("output already exists")
    args.output.mkdir(parents=True)
    events = []
    with (args.output / "frames.txt").open("w", encoding="ascii") as frames:
        def log(line):
            frames.write(f"{utc()} {time.monotonic_ns()} {line}\n")
            frames.flush()
        try:
            with SerialTransport("COM5", 115200, "N", 1, 350,
                                 frame_logger=log) as port:
                client = ModbusClient(port, 1)
                before = read(client)
                events.append(dict(kind="BEFORE", utc=utc(), value=before))
                i, a, c, b, config = (before[key] for key in
                    ("identity", "alarm", "check", "battery", "active"))
                if (i["firmware"], i["map"]) != ("0x052B", "0x010F") or \
                        i["configuration"]["storage_sequence"] != 22 or \
                        i["configuration"]["slot"] != 2 or \
                        i["configuration"]["sample_rate"] != 1 or \
                        i["configuration"]["filter_mode"] != 1 or \
                        i["configuration"]["filter_strength"] != 3 or \
                        i["calibration_valid"] != 1 or i["fault_mask"] or \
                        i["overrun_count"] or i["revision"] != 28 or \
                        i["saved_revision"] != 22 or i["dirty"] != 1 or \
                        i["r5"]["application"] != 1 or i["r5"]["mode"] != 2 or \
                        i["r5"]["save_request_count_low"] != 0 or \
                        c[1] != 1 or a[0] != 1 or a[1] != 0 or \
                        word64(a[2:6], "high") != 100000000 or \
                        word64(a[6:10], "high") != 400000000 or \
                        word64(a[10:14], "high") != 10000 or \
                        any(a[x] != 0 for x in (14, 15, 16)) or \
                        config[1] != 1 or b != [0, 400, 0, 13600,
                                                  0, 13200, 0, 13900, 1]:
                    raise RuntimeError("SAVE precondition mismatch; no SAVE sent")
                response, _ = client.read(reg.RESPONSE_TOKEN, 4)
                token = (response[0] + 1) & 0xffff or 1
                result = execute_command(client, token, 13)
                events.append(dict(kind="SAVE_REQUEST", utc=utc(), response=result))
                if result["token"] != token or result["last_command"] != 13 or \
                        result["result"] != 1:
                    raise RuntimeError("SAVE was not accepted")
                deadline = time.monotonic() + 12
                while time.monotonic() < deadline:
                    current = read(client)
                    events.append(dict(kind="SAVE_POLL", utc=utc(), value=current))
                    j = current["identity"]
                    if j["fault_mask"] or j["overrun_count"]:
                        raise RuntimeError("safety changed while saving")
                    if (j["revision"] == j["saved_revision"] == 28 and
                            j["dirty"] == 0 and
                            j["configuration"]["storage_sequence"] == 23 and
                            j["r5"]["save_request_count_low"] == 1):
                        print(json.dumps(dict(status="SAVED", revision=28,
                            sequence=23, slot=j["configuration"]["slot"],
                            config_before_seq=22, check_mode=current["check"][1])))
                        break
                    time.sleep(0.2)
                else:
                    raise RuntimeError("SAVE did not complete within 12 seconds")
        except Exception as exc:
            events.append(dict(kind="ERROR", utc=utc(), error=str(exc)))
            raise
        finally:
            (args.output / "events.json").write_text(
                json.dumps(events, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
