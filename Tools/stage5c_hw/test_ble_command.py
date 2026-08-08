#!/usr/bin/env python3
"""Host-only tests for the Stage 5C-C Python frame codec/parser."""

import unittest

from stage5c_ble import (FrameParser, OPERATIONS, RESPONSE, crc16,
                         decode_response, encode_request)


class BleCommandToolTests(unittest.TestCase):
    def test_request_encoding(self):
        frame = encode_request(0x1234, OPERATIONS["tare"], b"", 7, 8)
        self.assertEqual(frame[:4], b"\xA5\x5A\x01\x80")
        self.assertEqual(frame[12:18], b"\x34\x12\x10\x00\x00\x00")
        self.assertEqual(int.from_bytes(frame[-2:], "little"), crc16(frame[:-2]))

    def test_parser_interleaves_telemetry_and_response(self):
        telemetry = encode_request(1, OPERATIONS["tare"])
        telemetry = telemetry[:3] + b"\x01" + telemetry[4:]
        telemetry = telemetry[:-2] + crc16(telemetry[:-2]).to_bytes(2, "little")
        payload = b"\x34\x12\x10\x00\x00\x00\x00\x00"
        response = (b"\xA5\x5A\x01" + bytes((RESPONSE,)) +
                    len(payload).to_bytes(2, "little") + b"\x09\x00" +
                    b"\x0A\x00\x00\x00" + payload)
        response += crc16(response).to_bytes(2, "little")
        parser = FrameParser()
        frames = []
        stream = b"noise" + telemetry + response
        for value in stream:
            frames.extend(parser.feed(bytes((value,))))
        self.assertEqual([item[0] for item in frames], [0x01, RESPONSE])
        decoded = decode_response(frames[-1][1])
        self.assertEqual(decoded["transaction_id"], 0x1234)
        self.assertEqual(decoded["result_name"], "OK")

    def test_crc_recovery(self):
        bad = bytearray(encode_request(1, OPERATIONS["tare"]))
        bad[-1] ^= 0x80
        good = encode_request(2, OPERATIONS["zero"])
        parser = FrameParser()
        frames = parser.feed(bytes(bad) + good)
        self.assertEqual(parser.crc_errors, 1)
        self.assertEqual(len(frames), 1)


if __name__ == "__main__":
    unittest.main()
