"""CRC-16/MODBUS helpers. CRC bytes are emitted low byte first."""

INITIAL = 0xFFFF
POLYNOMIAL = 0xA001


def update(state, data):
    for value in bytes(data):
        state ^= value
        for _ in range(8):
            state = ((state >> 1) ^ POLYNOMIAL) if state & 1 else state >> 1
    return state & 0xFFFF


def calculate(data):
    return update(INITIAL, data)


def append(data):
    frame = bytearray(data)
    crc = calculate(frame)
    frame.extend((crc & 0xFF, crc >> 8))
    return bytes(frame)


def verify(frame):
    frame = bytes(frame)
    return len(frame) >= 2 and calculate(frame[:-2]) == int.from_bytes(
        frame[-2:], "little")
