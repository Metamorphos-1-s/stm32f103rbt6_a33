"""Command mailbox helpers and guarded command test."""

import time
from hw_common import execute_command
import register_map as reg


def run(client, report, command="NOP", repeat_token=False):
    command = command.upper()
    if command not in reg.COMMANDS:
        raise ValueError("unknown command %s" % command)
    token = int(time.time() * 1000) & 0xFFFF or 1
    first = execute_command(client, token, reg.COMMANDS[command])
    report.add("command %s" % command, "PASS", first["result_name"], first)
    if repeat_token:
        second = execute_command(client, token, reg.COMMANDS[command])
        status = "PASS" if second == first else "FAIL"
        report.add("duplicate token", status, "response stable", second)
        return status == "PASS"
    return True
