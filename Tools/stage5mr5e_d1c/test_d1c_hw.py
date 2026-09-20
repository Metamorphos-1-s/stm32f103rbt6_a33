import unittest

from d1c_hw import combined_state, decode_d1c


class D1CHardwareDecoderTests(unittest.TestCase):
    def test_decode_signed_and_flags(self):
        words = [0] * 24
        words[0] = 0xD1C1
        words[1:5] = [0x1234, 0x5678, 0x0001, 0x2345]
        words[5:7] = [0xFFFF, 0xFFFF]
        words[7:9] = [0xFFFF, 0xFFF7]
        words[9:11] = [0xFFFF, 0xFFFC]
        words[11] = 0xFDFF
        words[12] = 0x3F25
        words[13] = 0x1205
        words[14:22] = [0] * 8
        words[22:24] = [0xA400, 0x0808]
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
        class Client:
            def read(self, address, count):
                self.last = (address, count)
                return words, None
        combined = combined_state(Client())
        self.assertEqual(-100, combined["authoritative_display_input_ug"])
        self.assertEqual(-45, combined["desired_count"])

    def test_rejects_signature(self):
        with self.assertRaises(Exception):
            decode_d1c([0] * 24)


if __name__ == "__main__":
    unittest.main()
