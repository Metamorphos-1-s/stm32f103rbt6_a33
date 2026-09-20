import unittest

from stage5na_hw import decode


class Stage5NAHardwareTests(unittest.TestCase):
    def test_decode(self):
        words = [0] * 31; words[0] = 0x5AA5
        words[17:21] = [0x1234, 0x5678, 0, 7]
        words[21:24] = [0x2344, 0x0300, 0x4103]
        words[24:27] = [0x0432, 0xA210, 0x0808]
        words[27:31] = [0xFFFF, 0xFFFF, 0x0001, 0x0002]
        value = decode(words)
        self.assertEqual(0x12345678, value["sample_sequence"])
        self.assertEqual((2, 3, 4, 4), (value["static_immediate"],
            value["static_class"], value["dynamic_immediate"],
            value["dynamic_confirmed"]))
        self.assertEqual((1, 1, 1, 1), (value["process_active"], value["valid"],
            value["dirty"], value["overrun"]))
        self.assertEqual(-100, value["r5_input_ug"])
        self.assertEqual(0x00010002, value["timestamp_ms"])

    def test_signature(self):
        with self.assertRaises(Exception): decode([0] * 31)


if __name__ == "__main__": unittest.main()
