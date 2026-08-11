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
    command_service = (root / "Protocol/command_service/command_service.c").read_text(
        encoding="utf-8")
    schema = (root / "Services/config_store/persistent_schema.h").read_text(
        encoding="utf-8")
    project_config = (root / "Config/project_config.h").read_text(
        encoding="utf-8")
    command_types = (root / "Protocol/command_service/command_types.h").read_text(
        encoding="utf-8")
    drift_types = (root / "Domain/measurement/runtime_drift_compensator.h").read_text(
        encoding="utf-8")
    docs = (root / "Docs/MODBUS_REGISTER_MAP_V1.md").read_text(encoding="utf-8")
    for name, value in {
        "MODBUS_REGISTER_MAP_VERSION": reg.REGISTER_MAP_VERSION,
        "MODBUS_REALTIME_FIRST": reg.REALTIME_FIRST,
        "MODBUS_MAILBOX_FIRST": reg.MAILBOX_FIRST,
        "MODBUS_ACTIVE_CONFIG_FIRST": reg.ACTIVE_CONFIG_FIRST,
        "MODBUS_STAGING_CONFIG_FIRST": reg.STAGING_CONFIG_FIRST,
        "MODBUS_CALIBRATION_FIRST": reg.CALIBRATION_FIRST,
        "MODBUS_COMMUNICATION_FIRST": reg.COMMUNICATION_FIRST,
        "MODBUS_STORAGE_FIRST": reg.STORAGE_FIRST,
        "MODBUS_DISPLAY_CONDITION_FIRST": reg.DISPLAY_CONDITION_FIRST,
        "MODBUS_DISPLAY_CONDITION_LAST": reg.DISPLAY_CONDITION_LAST,
        "MODBUS_RUNTIME_DRIFT_FIRST": reg.RUNTIME_DRIFT_FIRST,
        "MODBUS_RUNTIME_DRIFT_LAST": reg.RUNTIME_DRIFT_LAST,
        "MODBUS_RUNTIME_DRIFT_RESERVED": reg.RUNTIME_DRIFT_RESERVED,
        "MODBUS_ALARM_ACTIVE_FIRST": reg.ALARM_ACTIVE_FIRST,
        "MODBUS_ALARM_ACTIVE_LAST": reg.ALARM_ACTIVE_LAST,
        "MODBUS_ALARM_STAGING_FIRST": reg.ALARM_STAGING_FIRST,
        "MODBUS_ALARM_STAGING_LAST": reg.ALARM_STAGING_LAST,
        "MODBUS_EXECUTE_VALUE": reg.EXECUTE_VALUE,
    }.items():
        _require(r"#define\s+%s\s+0x%04XU" % (name, value), header, name)
    for name, value in {
        "MODBUS_DISPLAY_CONDITION_STATE": reg.DISPLAY_CONDITION_STATE,
        "MODBUS_DISPLAY_CONDITION_LOCKED": reg.DISPLAY_CONDITION_LOCKED,
        "MODBUS_DISPLAY_CONDITION_MASS_FIRST": reg.DISPLAY_CONDITION_MASS,
        "MODBUS_DISPLAY_CONDITION_ANCHOR_FIRST": reg.DISPLAY_CONDITION_ANCHOR,
        "MODBUS_DISPLAY_CONDITION_THRESHOLD_FIRST": reg.DISPLAY_CONDITION_THRESHOLD,
        "MODBUS_DISPLAY_CONDITION_ELAPSED_FIRST": reg.DISPLAY_CONDITION_ELAPSED,
        "MODBUS_DISPLAY_CONDITION_RELEASE_REASON": reg.DISPLAY_CONDITION_RELEASE_REASON,
        "MODBUS_RUNTIME_DRIFT_STATE": reg.RUNTIME_DRIFT_STATE,
        "MODBUS_RUNTIME_DRIFT_ENABLED": reg.RUNTIME_DRIFT_ENABLED,
        "MODBUS_RUNTIME_DRIFT_LIMITED": reg.RUNTIME_DRIFT_LIMITED,
        "MODBUS_RUNTIME_DRIFT_OFFSET_FIRST": reg.RUNTIME_DRIFT_OFFSET,
        "MODBUS_RUNTIME_DRIFT_UNCOMPENSATED_FIRST": reg.RUNTIME_DRIFT_UNCOMPENSATED_GROSS,
        "MODBUS_RUNTIME_DRIFT_COMPENSATED_FIRST": reg.RUNTIME_DRIFT_COMPENSATED_GROSS,
        "MODBUS_RUNTIME_DRIFT_REFERENCE_FIRST": reg.RUNTIME_DRIFT_REFERENCE,
        "MODBUS_RUNTIME_DRIFT_ERROR_FIRST": reg.RUNTIME_DRIFT_ERROR,
        "MODBUS_RUNTIME_DRIFT_ARMING_ELAPSED_FIRST": reg.RUNTIME_DRIFT_ARMING_ELAPSED,
        "MODBUS_RUNTIME_DRIFT_WINDOW_ELAPSED_FIRST": reg.RUNTIME_DRIFT_WINDOW_ELAPSED,
        "MODBUS_RUNTIME_DRIFT_SAMPLE_COUNT_FIRST": reg.RUNTIME_DRIFT_SAMPLE_COUNT,
        "MODBUS_ALARM_LIMIT_ENABLE": reg.ALARM_LIMIT_ENABLE,
        "MODBUS_ALARM_WEIGHT_SOURCE": reg.ALARM_WEIGHT_SOURCE,
        "MODBUS_ALARM_LOWER_LIMIT_FIRST": reg.ALARM_LOWER_LIMIT,
        "MODBUS_ALARM_UPPER_LIMIT_FIRST": reg.ALARM_UPPER_LIMIT,
        "MODBUS_ALARM_HYSTERESIS_FIRST": reg.ALARM_HYSTERESIS,
        "MODBUS_ALARM_INTERNAL_ENABLE": reg.ALARM_INTERNAL_ENABLE,
        "MODBUS_ALARM_EXTERNAL_ENABLE": reg.ALARM_EXTERNAL_ENABLE,
        "MODBUS_ALARM_QUALIFIED_ENABLE": reg.ALARM_QUALIFIED_ENABLE,
        "MODBUS_ALARM_STATE": reg.ALARM_STATE,
        "MODBUS_ALARM_STABLE": reg.ALARM_STABLE,
        "MODBUS_ALARM_ACTIVE": reg.ALARM_ACTIVE,
        "MODBUS_ALARM_GREEN_ACTIVE": reg.ALARM_GREEN_ACTIVE,
        "MODBUS_ALARM_YELLOW_ACTIVE": reg.ALARM_YELLOW_ACTIVE,
        "MODBUS_ALARM_RED_ACTIVE": reg.ALARM_RED_ACTIVE,
        "MODBUS_ALARM_INTERNAL_ACTIVE": reg.ALARM_INTERNAL_ACTIVE,
        "MODBUS_ALARM_EXTERNAL_ACTIVE": reg.ALARM_EXTERNAL_ACTIVE,
        "MODBUS_ALARM_CONFIG_REVISION_FIRST": reg.ALARM_CONFIG_REVISION,
        "MODBUS_ALARM_CONFIG_DIRTY": reg.ALARM_CONFIG_DIRTY,
        "MODBUS_ALARM_STAGING_VALIDATION": reg.ALARM_STAGING_VALIDATION,
        "MODBUS_ALARM_STAGING_DIRTY": reg.ALARM_STAGING_DIRTY,
    }.items():
        _require(r"#define\s+%s\s+0x%04XU" % (name, value), header, name)
    for address in (reg.REQUEST_TOKEN, reg.COMMAND_ID, reg.EXECUTE,
                    reg.RESPONSE_TOKEN, reg.COMMAND_RESULT):
        _require(r"case\s+0x%04XU" % address, mailbox, "mailbox 0x%04X" % address)
    _require(r"address\s*==\s*(?:14U|0x0*EU).*MODBUS_REGISTER_MAP_VERSION",
             model, "map ID")
    _require(r"#define\s+FW_RELEASE_VERSION\s+0x%04XU" % reg.FIRMWARE_VERSION,
             project_config, "firmware version constant")
    _require(r"address\s*==\s*(?:15U|0x0*FU).*FW_RELEASE_VERSION",
             model, "firmware ID")
    _require(r"address==0x%04XU" % reg.STORAGE_SCHEMA, model, "schema register")
    _require(r"#define\s+CONFIG_STORE_SCHEMA_V2\s+%dU" % reg.SCHEMA_VERSION,
             schema, "Schema V2")
    _require(r"COMMAND_FACTORY_RESET_CANCEL,\s*COMMAND_COMMUNICATION_APPLY,\s*"
             r"COMMAND_SET_RUNTIME_DRIFT_ENABLED", mailbox,
             "external command ID 25")
    _require(r"case\s+COMMAND_SET_RUNTIME_DRIFT_ENABLED", command_service,
             "runtime drift command handling")
    for value, name in reg.RUNTIME_DRIFT_STATES.items():
        _require(r"RUNTIME_DRIFT_%s(?:\s*=\s*%d)?[,\s]" % (name, value),
                 drift_types, "runtime drift state %s" % name)
    _require(r"`0203`\s*\|\s*reserved, read-as-zero", docs,
             "documented reserved register")
    _require(r"`000E=0103`", docs, "documented map version")
    _require(r"Command ID 25", docs, "documented command 25")
    fields = [
        (reg.RUNTIME_DRIFT_STATE, reg.RUNTIME_DRIFT_STATE),
        (reg.RUNTIME_DRIFT_ENABLED, reg.RUNTIME_DRIFT_ENABLED),
        (reg.RUNTIME_DRIFT_LIMITED, reg.RUNTIME_DRIFT_LIMITED),
        (reg.RUNTIME_DRIFT_RESERVED, reg.RUNTIME_DRIFT_RESERVED),
        (reg.RUNTIME_DRIFT_OFFSET, reg.RUNTIME_DRIFT_OFFSET + 3),
        (reg.RUNTIME_DRIFT_UNCOMPENSATED_GROSS, reg.RUNTIME_DRIFT_UNCOMPENSATED_GROSS + 3),
        (reg.RUNTIME_DRIFT_COMPENSATED_GROSS, reg.RUNTIME_DRIFT_COMPENSATED_GROSS + 3),
        (reg.RUNTIME_DRIFT_REFERENCE, reg.RUNTIME_DRIFT_REFERENCE + 3),
        (reg.RUNTIME_DRIFT_ERROR, reg.RUNTIME_DRIFT_ERROR + 3),
        (reg.RUNTIME_DRIFT_ARMING_ELAPSED, reg.RUNTIME_DRIFT_ARMING_ELAPSED + 1),
        (reg.RUNTIME_DRIFT_WINDOW_ELAPSED, reg.RUNTIME_DRIFT_WINDOW_ELAPSED + 1),
        (reg.RUNTIME_DRIFT_SAMPLE_COUNT, reg.RUNTIME_DRIFT_SAMPLE_COUNT + 1),
    ]
    addresses = [address for first, last in fields for address in range(first, last + 1)]
    if len(addresses) != len(set(addresses)) or min(addresses) != reg.RUNTIME_DRIFT_FIRST or \
            max(addresses) != reg.RUNTIME_DRIFT_LAST:
        raise RegisterMapMismatch("runtime drift fields overlap or leave the declared range")
    alarm_active_fields = [
        (reg.ALARM_LIMIT_ENABLE, reg.ALARM_LIMIT_ENABLE),
        (reg.ALARM_WEIGHT_SOURCE, reg.ALARM_WEIGHT_SOURCE),
        (reg.ALARM_LOWER_LIMIT, reg.ALARM_LOWER_LIMIT + 3),
        (reg.ALARM_UPPER_LIMIT, reg.ALARM_UPPER_LIMIT + 3),
        (reg.ALARM_HYSTERESIS, reg.ALARM_HYSTERESIS + 3),
        (reg.ALARM_INTERNAL_ENABLE, reg.ALARM_CONFIG_DIRTY),
    ]
    alarm_staging_fields = [
        (reg.ALARM_STAGING_LIMIT_ENABLE, reg.ALARM_STAGING_LIMIT_ENABLE),
        (reg.ALARM_STAGING_WEIGHT_SOURCE, reg.ALARM_STAGING_WEIGHT_SOURCE),
        (reg.ALARM_STAGING_LOWER_LIMIT, reg.ALARM_STAGING_LOWER_LIMIT + 3),
        (reg.ALARM_STAGING_UPPER_LIMIT, reg.ALARM_STAGING_UPPER_LIMIT + 3),
        (reg.ALARM_STAGING_HYSTERESIS, reg.ALARM_STAGING_HYSTERESIS + 3),
        (reg.ALARM_STAGING_INTERNAL_ENABLE, reg.ALARM_STAGING_DIRTY),
    ]
    for fields, first, last, label in (
            (alarm_active_fields, reg.ALARM_ACTIVE_FIRST,
             reg.ALARM_ACTIVE_LAST, "alarm active"),
            (alarm_staging_fields, reg.ALARM_STAGING_FIRST,
             reg.ALARM_STAGING_LAST, "alarm staging")):
        addresses = [address for field_first, field_last in fields
                     for address in range(field_first, field_last + 1)]
        if len(addresses) != len(set(addresses)) or min(addresses) != first or \
                max(addresses) > last:
            raise RegisterMapMismatch("%s fields overlap or leave the declared range" % label)
    return True


if __name__ == "__main__":
    check()
    print("register map matches current firmware sources")
