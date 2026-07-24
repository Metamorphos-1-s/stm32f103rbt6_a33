"""Guarded SAVE request and storage-state polling."""

import time
from hw_common import execute_command, HardwareTestError, probe_device
import register_map as reg


def _power_cycle_and_reconnect(client, report, dangerous=False):
    client.transport.close()
    if dangerous:
        print("CUT POWER NOW. Operator timing is not controlled.")
    if input("After power is off, type POWER_OFF: ").strip() != "POWER_OFF":
        raise HardwareTestError("power-off confirmation rejected")
    if input("Power the board on, then type POWER_ON: ").strip() != "POWER_ON":
        raise HardwareTestError("power-on confirmation rejected")
    client.transport.serial.open()
    client.transport.reset()
    identity = probe_device(client)
    report.add("post-power-cycle probe", "PASS", "device reconnected", identity)


def run(client, report, manual_power_cycle=False, dangerous_power_loss=False):
    diag = client.read(reg.STORAGE_STATE_DIAG, 8)[0]
    if diag[1] != 1:
        raise HardwareTestError("StoragePowerGuard is not SAFE")
    if diag[0] != 0:
        raise HardwareTestError("storage is busy")
    before_revision = (diag[3] << 16) | diag[4]
    before_saved = (diag[5] << 16) | diag[6]
    before_sequence_words = client.read(reg.STORAGE_SEQUENCE, 2)[0]
    before_sequence = (before_sequence_words[0] << 16) | before_sequence_words[1]
    token = int(time.time() * 1000) & 0xFFFF or 1
    start = time.perf_counter_ns()
    response = execute_command(client, token, reg.COMMANDS["REQUEST_SAVE"])
    response_ms = (time.perf_counter_ns() - start) / 1e6
    if response["result"] not in (0, 1):
        raise HardwareTestError("SAVE rejected: %s" % response["result_name"])
    if dangerous_power_loss:
        report.add("SAVE acknowledgement", "PASS", response["result_name"],
                   {"response_ms": response_ms, "revision_before": before_revision,
                    "saved_before": before_saved})
        _power_cycle_and_reconnect(client, report, True)
        report.add("interrupted SAVE coverage", "NOT VERIFIED WITH CURRENT EQUIPMENT",
                   "manual cut timing is imprecise; dump both slots and inspect CRC/commit")
        return True
    deadline = time.monotonic() + 5.0
    busy_seen = False
    while time.monotonic() < deadline:
        state = client.read(reg.STORAGE_STATE_DIAG, 8)[0]
        busy_seen |= state[0] != 0
        if state[0] == 0:
            break
        time.sleep(0.02)
    else:
        raise HardwareTestError("storage did not return idle")
    after_revision = (state[3] << 16) | state[4]
    after_saved = (state[5] << 16) | state[6]
    report.add("SAVE", "PASS", response["result_name"], {
        "response_ms": response_ms, "busy_seen": busy_seen,
        "revision_before": before_revision, "saved_before": before_saved,
        "revision_after": after_revision, "saved_after": after_saved,
        "sequence_before": before_sequence})
    sequence_words = client.read(reg.STORAGE_SEQUENCE, 2)[0]
    first_sequence = (sequence_words[0] << 16) | sequence_words[1]
    duplicate = execute_command(client, (token + 1) & 0xFFFF or 1,
                                reg.COMMANDS["REQUEST_SAVE"])
    deadline = time.monotonic() + 5.0
    while time.monotonic() < deadline:
        if client.read(reg.STORAGE_STATE_DIAG, 1)[0][0] == 0:
            break
        time.sleep(0.02)
    else:
        raise HardwareTestError("duplicate SAVE did not return idle")
    sequence_words = client.read(reg.STORAGE_SEQUENCE, 2)[0]
    duplicate_sequence = (sequence_words[0] << 16) | sequence_words[1]
    no_change = duplicate["result"] in (0, 1) and duplicate_sequence == first_sequence
    report.add("SAVE no-change", "PASS" if no_change else "FAIL",
               "sequence unchanged on identical second request",
               {"first_sequence": first_sequence,
                "duplicate_sequence": duplicate_sequence,
                "result": duplicate["result_name"]})
    if not no_change:
        raise HardwareTestError("identical SAVE changed storage sequence")
    report.add("Flash erase/program counters", "SKIPPED",
               "not Modbus-mapped; inspect Stage4BStorageDiagnosticSnapshot over SWD")
    report.add("A/B physical slots", "SKIPPED",
               "run dump-slots before and after SAVE, then parse_config_slots.py")
    if manual_power_cycle:
        _power_cycle_and_reconnect(client, report, False)
        report.add("manual power cycle", "PASS", "saved configuration identity is readable")
    return True
