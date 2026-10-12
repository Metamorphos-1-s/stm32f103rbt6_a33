"""Guarded battery-only transaction for the 0x0527 engineering candidate.

No SAVE is sent unless CONFIG_VALIDATE and CONFIG_APPLY_RAM both succeed and
the five active battery fields read back exactly. Importing this module has no
hardware side effects.
"""
import json
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools" / "stage5pa13c"))
sys.path.insert(0, str(ROOT / "Tools" / "stage5b_hw"))
from hw_common import execute_command
from hardware_shadow import ModbusClient, SerialTransport, now, probe

BATTERY_ACTIVE = 0x03A0
BATTERY_STAGING = 0x03A9
COMMAND_CONFIG_BEGIN = 9
COMMAND_CONFIG_VALIDATE = 10
COMMAND_CONFIG_APPLY = 11
COMMAND_CONFIG_CANCEL = 12
COMMAND_SAVE = 13


def words32(value):
    value &= 0xffffffff
    return [(value >> 16) & 0xffff, value & 0xffff]


def target_words():
    return (words32(400) + words32(13600) + words32(13200) +
            words32(13900) + [1])


def read_battery(client, base=BATTERY_ACTIVE):
    words, _ = client.read(base, 9)
    def signed(value):
        return value - 0x100000000 if value & 0x80000000 else value
    return {
        "calibration_offset_mv": signed((words[0] << 16) | words[1]),
        "low_warning_mv": (words[2] << 16) | words[3],
        "critical_low_mv": (words[4] << 16) | words[5],
        "recovery_mv": (words[6] << 16) | words[7],
        "low_voltage_alarm_enable": words[8],
    }


def write_battery(client, values):
    fields = (values["calibration_offset_mv"], values["low_warning_mv"],
              values["critical_low_mv"], values["recovery_mv"])
    words = []
    for value in fields:
        words.extend(words32(value))
    words.append(1 if values["low_voltage_alarm_enable"] else 0)
    client.write_multiple(BATTERY_STAGING, words)


def run(output: Path, perform_save=False):
    """Run the guarded transaction; SAVE is opt-in and never implicit."""
    events = []
    with SerialTransport("COM5", 115200, "N", 1, 350) as port:
        client = ModbusClient(port, 1)
        before = probe(client)
        events.append({"kind": "BEFORE", "utc": now(), "probe": before})
        generation = before.get("r5", {}).get("generation")
        if generation is None:
            raise RuntimeError("candidate must expose generation before writes")
        result = execute_command(client, int(time.time() * 1000) & 0xffff,
                                 COMMAND_CONFIG_BEGIN)
        events.append({"kind": "CONFIG_BEGIN", "result": result})
        if result["result"] != 0:
            raise RuntimeError("CONFIG_BEGIN rejected")
        write_battery(client, {
            "calibration_offset_mv": 400, "low_warning_mv": 13600,
            "critical_low_mv": 13200, "recovery_mv": 13900,
            "low_voltage_alarm_enable": True})
        events.append({"kind": "BATTERY_CANDIDATE", "values": target_words()})
        validation = execute_command(client, int(time.time() * 1000) & 0xffff,
                                     COMMAND_CONFIG_VALIDATE)
        events.append({"kind": "CONFIG_VALIDATE", "result": validation})
        if validation["result"] != 0:
            execute_command(client, int(time.time() * 1000) & 0xffff,
                            COMMAND_CONFIG_CANCEL)
            raise RuntimeError("validation rejected; SAVE prohibited")
        applied = execute_command(client, int(time.time() * 1000) & 0xffff,
                                  COMMAND_CONFIG_APPLY)
        events.append({"kind": "CONFIG_APPLY_RAM", "result": applied})
        if applied["result"] != 0 or read_battery(client) != {
                "calibration_offset_mv": 400, "low_warning_mv": 13600,
                "critical_low_mv": 13200, "recovery_mv": 13900,
                "low_voltage_alarm_enable": 1}:
            raise RuntimeError("RAM apply/readback mismatch; SAVE prohibited")
        if perform_save:
            saved = execute_command(client, int(time.time() * 1000) & 0xffff,
                                    COMMAND_SAVE)
            events.append({"kind": "SAVE", "result": saved})
            if saved["result"] != 1:
                raise RuntimeError("SAVE not accepted")
        output.write_text(json.dumps({"events": events}, indent=2) + "\n",
                                    encoding="utf-8")


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--perform-save", action="store_true")
    args = parser.parse_args()
    run(args.output, args.perform_save)
