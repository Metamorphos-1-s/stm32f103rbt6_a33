import unittest

from d1c_hw import decode_d1c


class D1CHardwareDecoderTests(unittest.TestCase):
    def test_decode_signed_and_flags(self):
        words = [0] * 21
        words[0] = 0xD1C1
        words[1:3] = [0xFFFF, 0xFFF7]
        words[3:5] = [0xFFFF, 0xFFFC]
        words[5:9] = [0xFFFF, 0xFFFF, 0xFFFF, 0xFFFB]
        words[9] = 0xFFFD; words[10] = 0xFFFF
        words[11:13] = [0xFFFF, 0xFFFC]
        words[13] = 0x1F; words[14] = 2; words[15] = 7
        words[16:18] = [0x1234, 0x5678]
        words[18:21] = [1, 2, 5]
        value = decode_d1c(words)
        self.assertEqual(-9, value["desired_division"])
        self.assertEqual(-4, value["display_division"])
        self.assertEqual(-5, value["delta_divisions"])
        self.assertEqual(-3, value["evidence"])
        self.assertEqual(-1, value["direction"])
        self.assertEqual(0x12345678, value["follower_sequence"])
        self.assertEqual((1, 1, 1, 1, 1),
            tuple(value[key] for key in ("initialized", "locked",
                "large_step", "stable", "active")))

    def test_rejects_signature(self):
        with self.assertRaises(Exception):
            decode_d1c([0] * 21)


if __name__ == "__main__":
    unittest.main()
