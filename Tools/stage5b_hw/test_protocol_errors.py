"""Raw malformed-frame regression. All cases are non-persistent."""

import random
import time

import modbus_frame as mf
import register_map as reg


def _corrupt_crc(data):
    value = bytearray(data)
    value[-1] ^= 0x01
    return bytes(value)


def _exception(rx, slave, function, code):
    try:
        parsed = mf.parse_response(rx, slave, function)
        return parsed.exception == code
    except ValueError:
        return False


def run(client, report):
    slave = client.slave
    other = 1 if slave != 1 else 2
    valid = mf.build_read(slave, reg.REALTIME_FIRST, 1)
    cases = [
        ("bad CRC", _corrupt_crc(valid), None),
        ("other address", mf.build_read(other, 0, 1), None),
        ("broadcast FC03", mf.build_read(0, 0, 1), None),
        ("broadcast FC06", mf.build_write_single(0, reg.ACTIVE_BRIGHTNESS, 0), None),
        ("broadcast FC16", mf.build_write_multiple(0, reg.STAGING_BRIGHTNESS, [0]), None),
        ("illegal function", mf.build_request(slave, 4, b"\x00\x00\x00\x01"), (4, 1)),
        ("FC03 quantity zero", mf.build_read(slave, 0, 0), (3, 3)),
        ("FC03 quantity 126", mf.build_read(slave, 0, 126), (3, 3)),
        ("FC03 illegal address", mf.build_read(slave, 0xFFFF, 1), (3, 2)),
        ("FC06 read-only", mf.build_write_single(slave, 0, 0), (6, 2)),
        ("FC06 illegal enum", mf.build_write_single(slave, reg.COMM_ADDRESS, 0), (6, 3)),
        ("FC16 quantity zero", mf.build_request(slave, 16, b"\x01\x40\x00\x00\x00"), (16, 3)),
        ("FC16 quantity 124", mf.build_request(slave, 16, b"\x01\x40\x00\x7C\x00"), (16, 3)),
        ("FC16 byte count", mf.build_request(slave, 16, b"\x01\x40\x00\x01\x01\x00"), (16, 3)),
    ]
    truncated = mf.build_write_multiple(slave, reg.STAGING_BRIGHTNESS, [1])[:-3]
    cases.extend([
        ("FC16 truncated", truncated, None),
        ("FC16 trailing byte", mf.append_crc(mf.build_write_multiple(slave, reg.STAGING_BRIGHTNESS, [1])[:-2] + b"\x00"), (16, 3)),
        ("three-byte short frame", b"\x01\x03\x00", None),
        ("noise over 256", bytes([0xF8]) * 257, None),
    ])
    rng = random.Random(0x5B)
    random_bad = bytes(rng.randrange(0, 256) for _ in range(11))
    cases.append(("fixed-seed random", random_bad, None))
    passed = True
    for name, request, expected in cases:
        exchange = client.raw(request, expect_response=expected is not None)
        ok = (not exchange.rx) if expected is None else _exception(
            exchange.rx, slave, expected[0], expected[1])
        report.add(name, "PASS" if ok else "FAIL",
                   "no response" if expected is None else "exception %02X" % expected[1])
        passed &= ok
        time.sleep(0.01)
    # Back-to-back requests deliberately omit a legal RTU silent interval.
    client.transport.serial.reset_input_buffer()
    client.transport.serial.write(valid + valid)
    client.transport.serial.flush()
    time.sleep(client.transport.timeout_s)
    try:
        client.read(reg.REALTIME_FIRST, 1)
        report.add("back-to-back frames", "PASS", "valid FC03 remained responsive afterward")
        report.add("post-error recovery", "PASS", "valid FC03 succeeded")
    except Exception as exc:
        report.add("post-error recovery", "FAIL", str(exc))
        passed = False
    return passed
