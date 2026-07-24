import unittest

import modbus_frame as mf


class FrameTests(unittest.TestCase):
    def test_fc03_request(self):
        self.assertEqual(mf.build_read(1, 0, 2).hex(), "010300000002c40b")

    def test_signed_word_order(self):
        self.assertEqual(mf.decode_i32_words([0xFFFF, 0xFFFE]), -2)
        self.assertEqual(mf.decode_i32_words([0xFFFE, 0xFFFF], "low"), -2)
        self.assertEqual(mf.decode_i64_words([0xFFFF] * 4), -1)

    def test_exception(self):
        response = mf.build_request(1, 0x83, b"\x02")
        parsed = mf.parse_response(response, 1, 3)
        self.assertEqual(parsed.exception, 2)


if __name__ == "__main__": unittest.main()
