import struct
import unittest
import random

from ble_telemetry import (CHECKWEIGH_TYPE, FAST_TYPE, HEADER_SIZE, SLOW_TYPE,
                           TelemetryParser, crc16)


def frame(message_type, sequence, timestamp, payload):
    header = (b"\xA5\x5A" + bytes((1, message_type)) +
              struct.pack("<HHI", len(payload), sequence, timestamp))
    body = header + payload
    return body + struct.pack("<H", crc16(body))


class ParserTests(unittest.TestCase):
    def test_complete_split_and_merged(self):
        fast = frame(FAST_TYPE, 0, 100, struct.pack("<IqqqqBBBBBB", 1, 2, 3, 4, 5, 1, 1, 0, 0, 2, 1))
        slow = frame(SLOW_TYPE, 0, 100, bytes(59))
        parser = TelemetryParser()
        self.assertEqual(parser.feed(fast[:1]), [])
        self.assertEqual(parser.feed(fast[1:7]), [])
        self.assertEqual(len(parser.feed(fast[7:] + slow)), 2)
        self.assertEqual(parser.frames_received, 2)
        frames = [frame(FAST_TYPE, sequence, 1000 + sequence,
                        bytes([sequence & 0xFF]) * 42)
                  for sequence in range(20)]
        stream = b"".join(frames)
        expected_parser = TelemetryParser()
        expected = expected_parser.feed(stream)
        self.assertEqual(len(expected), 20)
        expected_wire = []
        for item in expected:
            normalized = dict(item)
            normalized.pop("host_timestamp", None)
            expected_wire.append(normalized)
        for seed in range(1000):
            rng = random.Random(0x5B000000 + seed)
            parser = TelemetryParser()
            parsed = []
            offset = 0
            while offset < len(stream):
                chunk_length = rng.randint(1, 97)
                parsed.extend(parser.feed(stream[offset:offset + chunk_length]))
                offset += chunk_length
            parsed_wire = []
            for item in parsed:
                normalized = dict(item)
                normalized.pop("host_timestamp", None)
                parsed_wire.append(normalized)
            self.assertEqual(parsed_wire, expected_wire)
            self.assertEqual(parser.crc_errors, 0)
            self.assertEqual(parser.sequence_gaps, 0)
            self.assertEqual(parser.duplicates, 0)
            self.assertEqual(parser.parser_resync, 0)

    def test_checkweigh_status_and_interleave(self):
        fast = frame(FAST_TYPE, 10, 100, bytes(42))
        status = frame(CHECKWEIGH_TYPE, 11, 101,
                       struct.pack("<BBBBI", 3, 0x65, 1, 0, 0x12345678))
        slow = frame(SLOW_TYPE, 12, 102, bytes(59))
        parser = TelemetryParser()
        parsed = parser.feed(fast + status[:9])
        parsed += parser.feed(status[9:] + slow)
        self.assertEqual([item["message_type"] for item in parsed],
                         [FAST_TYPE, CHECKWEIGH_TYPE, SLOW_TYPE])
        decoded = parsed[1]
        self.assertEqual(decoded["checkweigh_state"], 3)
        self.assertTrue(decoded["limit_enabled"])
        self.assertTrue(decoded["alarm_active"])
        self.assertTrue(decoded["red_active"])
        self.assertTrue(decoded["internal_buzzer_active"])
        self.assertFalse(decoded["external_buzzer_active"])
        self.assertEqual(decoded["weight_source"], 1)
        self.assertEqual(decoded["config_revision"], 0x12345678)

    def test_every_byte_and_garbage(self):
        data = frame(FAST_TYPE, 1, 200, bytes(42))
        parser = TelemetryParser()
        parsed = []
        for byte in b"garbage" + data:
            parsed.extend(parser.feed(bytes((byte,))))
        self.assertEqual(len(parsed), 1)
        self.assertGreater(parser.parser_resync, 0)

    def test_crc_resync_and_truncated(self):
        good = frame(FAST_TYPE, 2, 300, bytes(42))
        bad = bytearray(good)
        bad[-1] ^= 0xFF
        parser = TelemetryParser()
        self.assertEqual(parser.feed(bytes(bad) + good), [parser.frames[-1]])
        self.assertEqual(parser.crc_errors, 1)
        parser = TelemetryParser()
        parser.feed(good[:-1])
        self.assertEqual(len(parser.buffer), len(good) - 1)

    def test_rejects_version_type_length(self):
        good = bytearray(frame(FAST_TYPE, 3, 400, bytes(42)))
        bad_version = bytearray(good)
        bad_version[2] = 2
        bad_type = bytearray(good)
        bad_type[3] = 0x80
        bad_len = bytearray(good)
        bad_len[4] = 1
        parser = TelemetryParser()
        parser.feed(bytes(bad_version) + bytes(bad_type) + bytes(bad_len) + bytes(good))
        self.assertEqual(parser.frames_received, 1)
        self.assertEqual(parser.version_errors, 1)
        self.assertEqual(parser.type_errors, 1)
        self.assertEqual(parser.length_errors, 1)

    def test_sequence_gap_duplicate_wrap(self):
        payload = bytes(42)
        parser = TelemetryParser()
        parser.feed(frame(FAST_TYPE, 0xFFFE, 0xFFFFFF00, payload))
        parser.feed(frame(FAST_TYPE, 0xFFFF, 0xFFFFFF10, payload))
        parser.feed(frame(FAST_TYPE, 0, 0x00000010, payload))
        parser.feed(frame(FAST_TYPE, 0, 0x00000020, payload))
        parser.feed(frame(FAST_TYPE, 2, 0x00000030, payload))
        self.assertEqual(parser.duplicates, 1)
        self.assertEqual(parser.sequence_gaps, 1)
        self.assertEqual(parser.timestamp_anomalies, 0)


if __name__ == "__main__":
    unittest.main()
