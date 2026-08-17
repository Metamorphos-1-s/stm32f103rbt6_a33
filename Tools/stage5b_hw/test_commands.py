"""Command mailbox helpers and guarded command test."""

import time
from hw_common import execute_command
import register_map as reg


def validate_arguments(command, arg0=0, arg1=0):
    if not 0 <= arg0 <= 0xFFFFFFFF or not 0 <= arg1 <= 0xFFFFFFFF:
        raise ValueError("command arguments must be unsigned 32-bit values")
    if command == "RUNTIME_DRIFT_CONTROL" and arg0 not in (0, 1):
        raise ValueError("RUNTIME_DRIFT_CONTROL arg0 must be 0 or 1")
    if command in ("RUNTIME_DRIFT_ENABLE", "RUNTIME_DRIFT_DISABLE",
                   "RUNTIME_DRIFT_RESET") and (arg0 != 0 or arg1 != 0):
        raise ValueError("%s does not accept arguments" % command)


def run(client, report, command="NOP", repeat_token=False, arg0=0, arg1=0):
    command = command.upper()
    if command not in reg.COMMANDS:
        raise ValueError("unknown command %s" % command)
    validate_arguments(command, arg0, arg1)
    token = int(time.time() * 1000) & 0xFFFF or 1
    print("Executing command %s (%d), arg0=%d, arg1=%d" %
          (command, reg.COMMANDS[command], arg0, arg1))
    first = execute_command(client, token, reg.COMMANDS[command], arg0, arg1)
    report.add("command %s" % command, "PASS", first["result_name"], first)
    if repeat_token:
        second = execute_command(client, token, reg.COMMANDS[command], arg0, arg1)
        status = "PASS" if second == first else "FAIL"
        report.add("duplicate token", status, "response stable", second)
        return status == "PASS"
    return True
