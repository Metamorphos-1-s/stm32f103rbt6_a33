import unittest

import stage5nb_hw as hw


class Stage5NBHardwareToolTests(unittest.TestCase):
    def test_decode(self):
        words = [0] * hw.COUNT
        words[0] = 0x5BB5; words[1] = 2; words[2] = 1; words[3] = 2
        words[4] = 7; words[5] = 3; words[6] = 0x1F1
        value = hw.decode(words)
        self.assertEqual(value["mode"], 2)
        self.assertEqual(value["generation"], 0x00010002)
        self.assertEqual(value["formal_state"], 3)
        self.assertEqual(value["green"], 1)
        self.assertEqual(value["external_buzzer"], 1)

    def test_contract_ids(self):
        self.assertEqual(hw.COMMAND_SET_MODE, 34)
        self.assertEqual(hw.COMMAND_GET_STATUS, 35)
        self.assertEqual(hw.MODES, {"off": 0, "static": 1, "dynamic": 2})

    def test_signature(self):
        with self.assertRaises(Exception):
            hw.decode([0] * hw.COUNT)


if __name__ == "__main__":
    unittest.main()
