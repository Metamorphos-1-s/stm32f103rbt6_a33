"""Refuse write tests when Python constants diverge from current C sources."""

import re
from pathlib import Path

import register_map as reg


class RegisterMapMismatch(RuntimeError):
    pass


def _require(pattern, text, label):
    if not re.search(pattern, text, re.MULTILINE):
        raise RegisterMapMismatch("current source does not match %s" % label)


def check(repo_root=None):
    root = Path(repo_root) if repo_root else Path(__file__).resolve().parents[2]
    header = (root / "Protocol/modbus/modbus_register_map.h").read_text(
        encoding="utf-8")
    model = (root / "Protocol/modbus/modbus_register_model.c").read_text(
        encoding="utf-8")
    mailbox = (root / "Protocol/modbus/modbus_command_mailbox.c").read_text(
        encoding="utf-8")
    schema = (root / "Services/config_store/persistent_schema.h").read_text(
        encoding="utf-8")
    for name, value in {
        "MODBUS_REGISTER_MAP_VERSION": reg.REGISTER_MAP_VERSION,
        "MODBUS_REALTIME_FIRST": reg.REALTIME_FIRST,
        "MODBUS_MAILBOX_FIRST": reg.MAILBOX_FIRST,
        "MODBUS_ACTIVE_CONFIG_FIRST": reg.ACTIVE_CONFIG_FIRST,
        "MODBUS_STAGING_CONFIG_FIRST": reg.STAGING_CONFIG_FIRST,
        "MODBUS_CALIBRATION_FIRST": reg.CALIBRATION_FIRST,
        "MODBUS_COMMUNICATION_FIRST": reg.COMMUNICATION_FIRST,
        "MODBUS_STORAGE_FIRST": reg.STORAGE_FIRST,
        "MODBUS_EXECUTE_VALUE": reg.EXECUTE_VALUE,
    }.items():
        _require(r"#define\s+%s\s+0x%04XU" % (name, value), header, name)
    for address in (reg.REQUEST_TOKEN, reg.COMMAND_ID, reg.EXECUTE,
                    reg.RESPONSE_TOKEN, reg.COMMAND_RESULT):
        _require(r"case\s+0x%04XU" % address, mailbox, "mailbox 0x%04X" % address)
    _require(r"address\s*==\s*(?:14U|0x0*EU).*MODBUS_REGISTER_MAP_VERSION",
             model, "map ID")
    _require(r"address\s*==\s*(?:15U|0x0*FU).*0x%04XU" % reg.FIRMWARE_VERSION,
             model, "firmware ID")
    _require(r"address==0x%04XU" % reg.STORAGE_SCHEMA, model, "schema register")
    _require(r"#define\s+CONFIG_STORE_SCHEMA_V2\s+%dU" % reg.SCHEMA_VERSION,
             schema, "Schema V2")
    _require(r"COMMAND_FACTORY_RESET_CANCEL,\s*COMMAND_COMMUNICATION_APPLY",
             mailbox, "command ID 24")
    return True


if __name__ == "__main__":
    check()
    print("register map matches current firmware sources")
