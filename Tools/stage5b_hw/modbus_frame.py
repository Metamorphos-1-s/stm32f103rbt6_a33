"""Raw Modbus RTU ADU construction and parsing."""

from dataclasses import dataclass

from modbus_crc import append, verify


class FrameError(ValueError):
    pass


@dataclass
class ParsedResponse:
    address: int
    function: int
    payload: bytes
    exception: int = None


def encode_u16_be(value):
    if not 0 <= value <= 0xFFFF:
        raise FrameError("16-bit value out of range")
    return value.to_bytes(2, "big")


def decode_u16_be(data):
    if len(data) != 2:
        raise FrameError("u16 requires two bytes")
    return int.from_bytes(data, "big")


def append_crc(data):
    return append(data)


def verify_crc(frame):
    return verify(frame)


def build_request(slave, function, payload=b""):
    if not 0 <= slave <= 247:
        raise FrameError("slave must be 0..247")
    return append(bytes((slave, function)) + bytes(payload))


def build_read(slave, address, quantity):
    return build_request(slave, 3, encode_u16_be(address) + encode_u16_be(quantity))


def build_write_single(slave, address, value):
    return build_request(slave, 6, encode_u16_be(address) + encode_u16_be(value))


def build_write_multiple(slave, address, values):
    values = list(values)
    body = b"".join(encode_u16_be(value) for value in values)
    return build_request(slave, 16, encode_u16_be(address) +
                         encode_u16_be(len(values)) + bytes((len(body),)) + body)


def parse_response(frame, expected_slave=None, expected_function=None):
    frame = bytes(frame)
    if len(frame) < 5 or not verify(frame):
        raise FrameError("short response or invalid CRC")
    address, function = frame[0], frame[1]
    if expected_slave is not None and address != expected_slave:
        raise FrameError("response slave mismatch")
    if function & 0x80:
        if len(frame) != 5:
            raise FrameError("invalid exception length")
        original = function & 0x7F
        if expected_function is not None and original != expected_function:
            raise FrameError("exception function mismatch")
        return ParsedResponse(address, original, b"", frame[2])
    if expected_function is not None and function != expected_function:
        raise FrameError("response function mismatch")
    return ParsedResponse(address, function, frame[2:-2])


def parse_exception(frame):
    parsed = parse_response(frame)
    if parsed.exception is None:
        raise FrameError("not an exception response")
    return parsed.exception


def registers_from_read_response(frame, slave):
    parsed = parse_response(frame, slave, 3)
    if parsed.exception is not None:
        raise FrameError("Modbus exception 0x%02X" % parsed.exception)
    if not parsed.payload or parsed.payload[0] != len(parsed.payload) - 1 or \
            parsed.payload[0] % 2:
        raise FrameError("invalid FC03 byte count")
    data = parsed.payload[1:]
    return [decode_u16_be(data[index:index + 2])
            for index in range(0, len(data), 2)]


def _ordered_words(words, word_order):
    if word_order not in ("high", "low"):
        raise FrameError("word_order must be high or low")
    return list(words) if word_order == "high" else list(reversed(words))


def decode_i32_words(words, word_order="high"):
    ordered = _ordered_words(words, word_order)
    if len(ordered) != 2:
        raise FrameError("i32 requires two registers")
    value = (ordered[0] << 16) | ordered[1]
    return value - (1 << 32) if value & (1 << 31) else value


def decode_u32_words(words, word_order="high"):
    ordered = _ordered_words(words, word_order)
    if len(ordered) != 2:
        raise FrameError("u32 requires two registers")
    return (ordered[0] << 16) | ordered[1]


def decode_i64_words(words, word_order="high"):
    ordered = _ordered_words(words, word_order)
    if len(ordered) != 4:
        raise FrameError("i64 requires four registers")
    value = 0
    for word in ordered:
        value = (value << 16) | word
    return value - (1 << 64) if value & (1 << 63) else value
