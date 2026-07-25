"""Shared client, configuration, safety, and probe helpers."""

import json
import time
from pathlib import Path

import modbus_frame as frame
import register_map as reg
from register_map_check import check as check_register_map


class HardwareTestError(RuntimeError):
    pass


class ModbusClient:
    def __init__(self, transport, slave):
        self.transport = transport
        self.slave = slave

    def raw(self, request, expect_response=True):
        return self.transport.exchange(request, expect_response)

    def read(self, address, quantity):
        exchange = self.raw(frame.build_read(self.slave, address, quantity))
        if not exchange.rx:
            raise HardwareTestError("Modbus response timeout")
        values = frame.registers_from_read_response(exchange.rx, self.slave)
        if len(values) != quantity:
            raise HardwareTestError("unexpected FC03 register count")
        return values, exchange

    def write_single(self, address, value):
        request = frame.build_write_single(self.slave, address, value)
        exchange = self.raw(request)
        parsed = frame.parse_response(exchange.rx, self.slave, 6)
        if parsed.exception is not None:
            raise HardwareTestError("FC06 exception 0x%02X" % parsed.exception)
        if exchange.rx != request:
            raise HardwareTestError("FC06 echo mismatch")
        return exchange

    def write_multiple(self, address, values):
        values = list(values)
        exchange = self.raw(frame.build_write_multiple(self.slave, address, values))
        parsed = frame.parse_response(exchange.rx, self.slave, 16)
        if parsed.exception is not None:
            raise HardwareTestError("FC16 exception 0x%02X" % parsed.exception)
        expected = frame.encode_u16_be(address) + frame.encode_u16_be(len(values))
        if parsed.payload != expected:
            raise HardwareTestError("FC16 acknowledgement mismatch")
        return exchange


def load_config(path):
    if not path:
        return {}
    try:
        value = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise HardwareTestError("cannot read config %s: %s" % (path, exc))
    if not isinstance(value, dict):
        raise HardwareTestError("configuration root must be an object")
    validators = {
        "port": lambda x: isinstance(x, str) and bool(x),
        "interface": lambda x: x in ("rs232", "rs485"),
        "baud_rate": lambda x: x in reg.BAUD_ENUM,
        "parity": lambda x: x in reg.PARITY_ENUM,
        "stop_bits": lambda x: x in (1, 2),
        "slave_address": lambda x: isinstance(x, int) and 1 <= x <= 247,
        "timeout_ms": lambda x: isinstance(x, int) and x > 0,
        "poll_interval_ms": lambda x: isinstance(x, int) and x >= 0,
        "automation_authorizations": lambda x: isinstance(x, list),
    }
    for key, validator in validators.items():
        if key in value and not validator(value[key]):
            raise HardwareTestError("invalid configuration field: %s" % key)
    for key in ("allow_write", "allow_actions", "allow_flash",
                "allow_comm_change", "allow_factory_reset", "allow_calibration"):
        if key in value and not isinstance(value[key], bool):
            raise HardwareTestError("configuration field %s must be boolean" % key)
    return value


def require_authorization(args, action, flag, description, confirmation):
    if not getattr(args, flag, False):
        raise HardwareTestError("%s requires --%s" % (description, flag.replace("_", "-")))
    check_register_map()
    print("DANGEROUS ACTION: %s" % description)
    print("Target: %s" % action)
    configured = set(getattr(args, "automation_authorizations", []) or [])
    if getattr(args, "yes", False):
        if action not in configured:
            raise HardwareTestError("automation requires JSON authorization: %s" % action)
        return
    entered = input("Type %s to continue: " % confirmation).strip()
    if entered != confirmation:
        raise HardwareTestError("confirmation rejected")


def execute_command(client, token, command, arg0=0, arg1=0, arg64=0,
                    flags=0, timeout_s=2.0):
    values = [token, command, (arg0 >> 16) & 0xFFFF, arg0 & 0xFFFF,
              (arg1 >> 16) & 0xFFFF, arg1 & 0xFFFF,
              (arg64 >> 48) & 0xFFFF, (arg64 >> 32) & 0xFFFF,
              (arg64 >> 16) & 0xFFFF, arg64 & 0xFFFF, flags]
    client.write_multiple(reg.REQUEST_TOKEN, values)
    client.write_single(reg.EXECUTE, reg.EXECUTE_VALUE)
    deadline = time.monotonic() + timeout_s
    while True:
        try:
            response, _ = client.read(reg.RESPONSE_TOKEN, 4)
        except HardwareTestError as exc:
            if str(exc) != "Modbus response timeout" or time.monotonic() >= deadline:
                raise
            time.sleep(0.02)
            continue
        if response[0] == token:
            return {"token": response[0], "result": response[1],
                    "result_name": reg.COMMAND_RESULTS.get(response[1], "UNKNOWN"),
                    "state": response[2], "last_command": response[3]}
        if time.monotonic() >= deadline:
            raise HardwareTestError("command response token timeout")
        time.sleep(0.02)


def probe_device(client):
    realtime, exchange = client.read(reg.REALTIME_FIRST, 0x20)
    diag, _ = client.read(reg.SAMPLE_SEQUENCE, 0x1B)
    storage, _ = client.read(reg.STORAGE_FIRST, 5)
    word_order = "low" if client.read(reg.ACTIVE_WORD_ORDER, 1)[0][0] else "high"
    result = {
        "display_weight": frame.decode_i32_words(realtime[0:2], word_order),
        "decimals": realtime[2], "active_unit": realtime[3],
        "status_flags": (realtime[4] << 16) | realtime[5],
        "register_map_version": realtime[14], "firmware_version": realtime[15],
        "net_mass_ug": frame.decode_i64_words(realtime[16:20], word_order),
        "sample_sequence": (diag[0] << 16) | diag[1],
        "active_profile": diag[8], "storage_state": diag[16],
        "power_safe": diag[17], "config_dirty": diag[18],
        "schema_version": storage[0], "active_slot": storage[1],
        "storage_sequence": (storage[2] << 16) | storage[3],
        "word_order": word_order,
        "hardware_revision": "UNAVAILABLE_IN_REGISTER_MAP",
        "battery_voltage": "UNAVAILABLE_IN_REGISTER_MAP",
        "response_ms": exchange.complete_ms,
    }
    if result["register_map_version"] != reg.REGISTER_MAP_VERSION:
        raise HardwareTestError("unsupported register map 0x%04X" % result["register_map_version"])
    if result["firmware_version"] != reg.FIRMWARE_VERSION:
        raise HardwareTestError("unsupported firmware version 0x%04X" % result["firmware_version"])
    if result["schema_version"] != reg.SCHEMA_VERSION:
        raise HardwareTestError("unsupported persistent schema %d" % result["schema_version"])
    return result
