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
    def test_v3_zero_range_is_encoded_before_overload(self):
        payload = bytearray(281)
        struct.pack_into("<q", payload, 16, 60000000)
        self.assertEqual(decode_v3(payload)["metrology"]["zero_range_ug"],
                         60000000)

    def test_v3_alarm_battery_and_requests_offsets(self):
        payload = bytearray(281)
        struct.pack_into("<qqq", payload, 207, 100000000, 400000000, 10000)
        payload[231:236] = bytes((0, 0, 0, 0, 1))
        struct.pack_into("<iIII", payload, 250, 400, 13600, 13200, 13900)
        payload[266] = 1
        payload[280] = 0x16
        decoded = decode_v3(payload)
        self.assertEqual(decoded["alarm"]["upper_limit_ug"], 400000000)
        self.assertEqual(decoded["alarm"]["hysteresis_ug"], 10000)
        self.assertTrue(decoded["alarm"]["limit_function_enable"])
        self.assertEqual(decoded["battery"]["calibration_offset_mv"], 400)
        self.assertEqual(decoded["requests"], {"r5_mode": 2,
            "r5_application": 1, "checkweigh_mode": 1})

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

    def test_v2_battery_and_runtime_prefix(self):
        data = slot(7)
        payload = memoryview(data)[HEADER_SIZE:HEADER_SIZE + 344]
        struct.pack_into("<IIiiIII", payload, 121, 47000, 10000, 25, -12,
                         14000, 13500, 14200)
        payload[149:164] = bytes([1, 1, 1, 1, 0, 0, 0, 0, 0,
                                  1, 0x34, 0x12, 0, 0, 1])
        covered = data[:CRC_OFFSET] + data[CRC_OFFSET + 4:HEADER_SIZE] + \
                  data[HEADER_SIZE:HEADER_SIZE + 344]
        struct.pack_into("<I", data, CRC_OFFSET,
                         zlib.crc32(covered) & 0xFFFFFFFF)
        parsed = parse_slot(data, "A")
        self.assertTrue(parsed["valid"])
        self.assertEqual(parsed["persistent"]["battery"]["divider_top_ohm"],
                         47000)
        self.assertEqual(parsed["persistent"]["battery"]["divider_bottom_ohm"],
                         10000)
        self.assertTrue(parsed["persistent"]["system"]["startup_auto_zero_enable"])
        self.assertEqual(len(parsed["slot_sha256"]), 64)
        self.assertEqual(len(parsed["payload_sha256"]), 64)


if __name__ == "__main__": unittest.main()
