"""Guarded RAM-only brightness staging/apply/restore test."""

import time
from hw_common import execute_command, HardwareTestError
import register_map as reg


def _ok(response, label):
    if response["result"] not in (0, 1):
        raise HardwareTestError("%s failed: %s" % (label, response["result_name"]))


def run(client, report):
    original = client.read(reg.ACTIVE_BRIGHTNESS, 1)[0][0]
    candidate = (original + 1) % 8
    token = int(time.time() * 1000) & 0xFFFF or 1
    try:
        _ok(execute_command(client, token, reg.COMMANDS["CONFIG_BEGIN"]), "begin")
        client.write_single(reg.STAGING_BRIGHTNESS, candidate)
        _ok(execute_command(client, token + 1, reg.COMMANDS["CONFIG_VALIDATE"]), "validate")
        _ok(execute_command(client, token + 2, reg.COMMANDS["CONFIG_APPLY_RAM"]), "apply")
        actual = client.read(reg.ACTIVE_BRIGHTNESS, 1)[0][0]
        if actual != candidate:
            raise HardwareTestError("active brightness did not change")
        report.add("RAM configuration apply", "PASS", "%d -> %d" % (original, candidate))
    finally:
        token = (token + 10) & 0xFFFF or 1
        _ok(execute_command(client, token, reg.COMMANDS["CONFIG_BEGIN"]), "restore begin")
        client.write_single(reg.STAGING_BRIGHTNESS, original)
        _ok(execute_command(client, token + 1, reg.COMMANDS["CONFIG_VALIDATE"]), "restore validate")
        _ok(execute_command(client, token + 2, reg.COMMANDS["CONFIG_APPLY_RAM"]), "restore apply")
        if client.read(reg.ACTIVE_BRIGHTNESS, 1)[0][0] != original:
            raise HardwareTestError("RAM configuration restore failed")
    report.add("RAM configuration restore", "PASS", "original value restored; SAVE not issued")
    token = (token + 20) & 0xFFFF or 1
    _ok(execute_command(client, token, reg.COMMANDS["CONFIG_BEGIN"]), "invalid begin")
    try:
        client.write_single(reg.STAGING_BRIGHTNESS, 8)
        validation = execute_command(client, token + 1, reg.COMMANDS["CONFIG_VALIDATE"])
        applied = execute_command(client, token + 2, reg.COMMANDS["CONFIG_APPLY_RAM"])
        unchanged = client.read(reg.ACTIVE_BRIGHTNESS, 1)[0][0] == original
        if validation["result"] != 3 or applied["result"] in (0, 1) or not unchanged:
            raise HardwareTestError("invalid staging value was not rejected atomically")
        report.add("invalid configuration", "PASS",
                   "brightness 8 rejected by validate/apply; active value unchanged")
    finally:
        _ok(execute_command(client, token + 3, reg.COMMANDS["CONFIG_CANCEL"]),
            "invalid cancel")
    report.add("configuration cancel", "PASS", "staging reset; SAVE not issued")
    return True
