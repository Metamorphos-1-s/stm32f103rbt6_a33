import struct
import unittest
import zlib

from parse_config_slots import *


def slot(sequence):
    data = bytearray(b"\xff" * SLOT_SIZE)
    struct.pack_into("<IHHHHII", data, 0, MAGIC, FORMAT_VERSION, 2,
                     HEADER_SIZE, 344, sequence, 0)
    struct.pack_into("<II", data, 24, 0, 0)
    data[HEADER_SIZE:HEADER_SIZE + 344] = bytes(range(256)) + bytes(range(88))
    covered = data[:CRC_OFFSET] + data[CRC_OFFSET + 4:HEADER_SIZE] + data[HEADER_SIZE:HEADER_SIZE + 344]
    struct.pack_into("<I", data, CRC_OFFSET, zlib.crc32(covered) & 0xFFFFFFFF)
    struct.pack_into("<I", data, COMMIT_OFFSET, COMMIT_MARKER)
    return data


class SlotTests(unittest.TestCase):
    def test_valid_slot(self):
        self.assertTrue(parse_slot(slot(7), "A")["valid"])

    def test_corruption(self):
        data = slot(7); data[50] ^= 1
        self.assertFalse(parse_slot(data, "A")["valid"])

    def test_sequence_wrap(self):
        self.assertTrue(sequence_newer(0, 0xFFFFFFFE))
        self.assertFalse(sequence_newer(0xFFFFFFFE, 0))

    def test_one_valid_slot_is_usable(self):
        result = {"active_slot": "A", "slots": [
            parse_slot(slot(1), "A"), parse_slot(b"\xff" * SLOT_SIZE, "B")]}
        self.assertTrue(dump_is_usable(result))


if __name__ == "__main__": unittest.main()
