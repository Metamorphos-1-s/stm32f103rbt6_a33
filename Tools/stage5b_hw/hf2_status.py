"""Read and decode the HF2-R1 read-only telemetry blocks."""

import modbus_frame as frame
import register_map as reg
from hw_common import HardwareTestError, probe_device


def _mass(value):
    return {"ug": value, "g": "%.6f" % (value / 1000000.0)}


def _enum(value, names):
    return {"value": value, "name": names.get(value, "UNKNOWN")}


def parse_status(identity, realtime, diagnostic, active, display, drift):
    order = identity["word_order"]
    # Firmware publishes status as low 16 bits at 0x0004 and high at 0x0005.
    # This legacy field is intentionally independent of configured word order.
    status_flags = realtime[4] | (realtime[5] << 16)
    unit = realtime[3]
    unit_base = 0x10 + unit * 2
    result = {
        "identity": identity,
        "weighing": {
            "display_count": frame.decode_i32_words(realtime[0:2], order),
            "decimal_places": realtime[2], "unit": unit,
            "division_digit": realtime[12], "status_flags": status_flags,
            "stable": bool(status_flags & reg.STATUS_STABLE),
            "tare_active": bool(status_flags & reg.STATUS_TARE_ACTIVE),
            "overload": bool(status_flags & reg.STATUS_OVERLOAD),
            "net": _mass(frame.decode_i64_words(realtime[0x10:0x14], order)),
            "gross": _mass(frame.decode_i64_words(realtime[0x14:0x18], order)),
            "tare": _mass(frame.decode_i64_words(realtime[0x18:0x1C], order)),
            "raw_count": frame.decode_i32_words(realtime[0x1C:0x1E], order),
            "filtered_raw": frame.decode_i32_words(realtime[0x1E:0x20], order),
            "sample_sequence": frame.decode_u32_words(diagnostic[0:2], order),
            "cs1237_state": diagnostic[0x0B],
            "config_dirty": bool(diagnostic[0x12]),
            "capacity": _mass(frame.decode_i64_words(active[4:8], order)),
            "configured_decimal_places": active[unit_base],
            "configured_division_digit": active[unit_base + 1],
            "overload_limit": "UNAVAILABLE_IN_REGISTER_MAP",
            "migration_pending": "UNAVAILABLE_IN_REGISTER_MAP",
        },
        "display_conditioner": {
            "state": _enum(display[0], reg.DISPLAY_CONDITION_STATES),
            "locked": bool(display[1]),
            "display_mass": _mass(frame.decode_i64_words(display[2:6], order)),
            "anchor": _mass(frame.decode_i64_words(display[6:10], order)),
            "release_threshold": _mass(frame.decode_i64_words(display[10:14], order)),
            "candidate_elapsed_ms": frame.decode_u32_words(display[14:16], order),
            "release_reason": _enum(display[16], reg.DISPLAY_RELEASE_REASONS),
        },
        "runtime_drift": {
            "state": _enum(drift[0], reg.RUNTIME_DRIFT_STATES),
            "enabled": bool(drift[1]), "limited": bool(drift[2]),
            "reserved": drift[3],
            "offset": _mass(frame.decode_i64_words(drift[4:8], order)),
            "uncompensated_gross": _mass(frame.decode_i64_words(drift[8:12], order)),
            "compensated_gross": _mass(frame.decode_i64_words(drift[12:16], order)),
            "plateau_reference": _mass(frame.decode_i64_words(drift[16:20], order)),
            "plateau_error": _mass(frame.decode_i64_words(drift[20:24], order)),
            "arming_elapsed_ms": frame.decode_u32_words(drift[24:26], order),
            "window_elapsed_ms": frame.decode_u32_words(drift[26:28], order),
            "sample_count": frame.decode_u32_words(drift[28:30], order),
        },
    }
    return result


def read_status(client, strict=True):
    identity = probe_device(client)
    realtime = client.read(reg.REALTIME_FIRST, 0x20)[0]
    diagnostic = client.read(reg.SAMPLE_SEQUENCE, 0x1B)[0]
    active = client.read(reg.ACTIVE_CONFIG_FIRST, 0x40)[0]
    display = client.read(reg.DISPLAY_CONDITION_FIRST,
                          reg.DISPLAY_CONDITION_LAST - reg.DISPLAY_CONDITION_FIRST + 1)[0]
    drift = client.read(reg.RUNTIME_DRIFT_FIRST,
                        reg.RUNTIME_DRIFT_LAST - reg.RUNTIME_DRIFT_FIRST + 1)[0]
    result = parse_status(identity, realtime, diagnostic, active, display, drift)
    if strict and result["runtime_drift"]["reserved"] != 0:
        raise HardwareTestError("runtime drift reserved register 0x0203 is nonzero")
    return result
