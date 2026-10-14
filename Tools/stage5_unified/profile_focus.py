"""Bounded, no-SAVE 0x052B profile transition with raw RTU evidence."""
import argparse
import json
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools/stage5b_hw"))
from hw_common import ModbusClient, execute_command  # noqa: E402
from serial_transport import SerialTransport  # noqa: E402
import register_map as reg  # noqa: E402


def now():
    return datetime.now(timezone.utc).isoformat()


def status(client):
    realtime, _ = client.read(0, 16)
    config, _ = client.read(0x100, 64)
    diag, _ = client.read(0x20, 28)
    r5, _ = client.read(0x280, 10)
    a13, _ = client.read(0x340, 18)
    return dict(firmware=realtime[15], map=realtime[14],
        active_profile=config[31],
        profiles=[dict(sample_rate=config[32 + i * 14],
                       filter_mode=config[34 + i * 14],
                       strength=config[35 + i * 14]) for i in range(2)],
        driver_state=diag[11], sequence=(diag[0] << 16) | diag[1],
        overrun=(diag[13] << 16) | diag[14],
        fault=(diag[25] << 16) | diag[26],
        revision=(diag[19] << 16) | diag[20],
        saved=(diag[21] << 16) | diag[22], dirty=diag[18],
        r5_application=r5[1], r5_mode=r5[2], r5_state=r5[3],
        offset_ug=((r5[6] << 48) | (r5[7] << 32) | (r5[8] << 16) | r5[9]),
        offset_applied=(a13[16] << 16) | a13[17])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("target", type=int, choices=(0, 1))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("output exists")
    args.output.mkdir(parents=True)
    events = []
    with (args.output / "frames.txt").open("w", encoding="ascii") as frames:
        def raw(line):
            frames.write(f"{now()} {time.monotonic_ns()} {line}\n")
            frames.flush()
        try:
            with SerialTransport("COM5", 115200, "N", 1, 350,
                                 frame_logger=raw) as port:
                client = ModbusClient(port, 1)
                before = status(client)
                events.append(dict(kind="BEFORE", utc=now(), state=before))
                if (before["firmware"], before["map"]) != (0x052B, 0x010F):
                    raise RuntimeError("wrong firmware")
                if before["fault"] or before["overrun"] or before["saved"] != 22:
                    raise RuntimeError("preflight safety mismatch")
                if before["active_profile"] == args.target:
                    raise RuntimeError("target profile already active")
                target = before["profiles"][args.target]
                if target["filter_mode"] != 1 or target["strength"] != 3 or \
                        target["sample_rate"] not in (0, 1):
                    raise RuntimeError("target profile outside filt1/strength3 10/40 range")
                response, _ = client.read(reg.RESPONSE_TOKEN, 4)
                token = (response[0] + 1) & 0xffff or 1
                result = execute_command(client, token, 7, arg0=args.target)
                events.append(dict(kind="COMMAND", utc=now(), response=result))
                if result["token"] != token or result["last_command"] != 7 or \
                        result["result"] != 1:
                    raise RuntimeError("profile request not ACCEPTED")
                deadline = time.monotonic() + 12
                while time.monotonic() < deadline:
                    state = status(client)
                    events.append(dict(kind="POLL", utc=now(), state=state))
                    if state["fault"] or state["overrun"] or state["saved"] != 22:
                        raise RuntimeError("safety changed during switch")
                    if state["active_profile"] == args.target and \
                            state["driver_state"] == 4 and \
                            state["profiles"][args.target] == target and \
                            state["sequence"] > before["sequence"]:
                        print(json.dumps(dict(result="COMPLETE", target=args.target,
                                              before=before, after=state)))
                        break
                    time.sleep(0.15)
                else:
                    raise RuntimeError("asynchronous profile switch not complete")
        except Exception as exc:
            events.append(dict(kind="ERROR", utc=now(), error=str(exc)))
            raise
        finally:
            (args.output / "events.json").write_text(
                json.dumps(events, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
