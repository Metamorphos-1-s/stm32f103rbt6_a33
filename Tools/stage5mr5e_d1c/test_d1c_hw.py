import unittest

from d1c_hw import decode_d1c


class D1CHardwareDecoderTests(unittest.TestCase):
    def test_decode_signed_and_flags(self):
        words = [0] * 51
        words[0] = 0xD1C1
        words[1:4] = [0x0514, 0x0104, 3]
        words[4:8] = [0x1234, 0x5678, 0x0001, 0x2345]
        words[8:12] = [0xFFFF, 0xFFFF, 0xFFFF, 0xFF9C]
        words[12:14] = [0xFFFF, 0xFFF7]
        words[14:16] = [0xFFFF, 0xFFFC]
        words[16:18] = [0xFFFF, 0xFFFB]
        words[18] = 0xFDFF
        words[19:21] = [0xFFFF, 0xFFFC]
        words[21] = 0x3F; words[22] = 0x0207
        words[23:26] = [0x0102, 5, 0x1205]
        words[26:42] = [0] * 16
        words[42:51] = [0, 4, 0, 0, 0, 0, 0x8002, 8, 8]
        value = decode_d1c(words)
        self.assertEqual(-9, value["desired_division"])
        self.assertEqual(-4, value["display_division"])
        self.assertEqual(-5, value["delta_divisions"])
        self.assertEqual(-3, value["evidence"])
        self.assertEqual(-1, value["direction"])
        self.assertEqual(0x12345678, value["follower_sequence"])
        self.assertEqual("0x0514", value["firmware"])
        self.assertEqual((1, 2, 5),
            (value["application"], value["mode"], value["state"]))
        self.assertEqual((1, 2, 8, 8), (value["dirty"],
            value["save_request_count_low"], value["revision"],
            value["saved_revision"]))
        self.assertEqual((1, 1, 1, 1, 1),
            tuple(value[key] for key in ("initialized", "locked",
                "large_step", "stable", "active")))

    def test_rejects_signature(self):
        with self.assertRaises(Exception):
            decode_d1c([0] * 51)


if __name__ == "__main__":
    unittest.main()
